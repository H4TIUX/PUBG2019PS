param([ValidateRange(1,65535)][int]$Port=8888, [ValidateRange(1,600)][int]$LoadWait=10, [ValidateSet('Miramar','Erangel','ErangelClassic','Sanhok','Vikendi','Training')][string]$Map='Miramar', [switch]$LauncherMode, [switch]$LanMode, [ValidateSet('None','BattleRoyale','Survival','Elimination')][string]$ZombieRule='None')
$ErrorActionPreference='Stop'
# The supplied PUBG2019PS documentation lists these internal level names.
$mapLevels=@{
    Miramar='Desert_Main'
    Erangel='Baltic_Main'
    ErangelClassic='Erangel_Main'
    Sanhok='Savage_Main'
    Vikendi='DihorOtok_Main'
    Training='Range_Main'
}
if (-not $PSBoundParameters.ContainsKey('Map')) {
    Write-Host ''
    Write-Host 'Choose a map:'
    Write-Host '  1. Miramar (default)'
    Write-Host '  2. Erangel (new)'
    Write-Host '  3. Erangel (old)'
    Write-Host '  4. Sanhok'
    Write-Host '  5. Vikendi'
    Write-Host '  6. Training'
    $choices=@{'1'='Miramar';'2'='Erangel';'3'='ErangelClassic';'4'='Sanhok';'5'='Vikendi';'6'='Training'}
    do {
        $choice=(Read-Host 'Map number (Enter = Miramar)').Trim()
        if ($choice -eq '') { $Map='Miramar'; break }
        if ($choices.ContainsKey($choice)) { $Map=$choices[$choice]; break }
        Write-Host 'Please enter a number from 1 to 6.'
    } while ($true)
}
$level=$mapLevels[$Map]
Set-Location -LiteralPath $PSScriptRoot
. (Join-Path $PSScriptRoot 'Portable2019Profile.ps1')
$script:PubgLaunchFile='Portable2019'
$server=$null
$readyFile=Join-Path $PSScriptRoot 'ServerStartupState.json'
$engineLog=Join-Path $PSScriptRoot 'PUBG2019_ServerEngine.log'
$hostAttemptUtc=[DateTime]::UtcNow
function Write-StartupState([string]$status, [string]$reason='') {
    $serverId=if($server){$server.Id}else{0}
    $state=@{ processId=$serverId; port=$Port; status=$status; map=$Map; reason=$reason }
    $temp=$readyFile+'.'+$PID+'.tmp'
    [IO.File]::WriteAllText($temp,($state | ConvertTo-Json -Compress),[Text.Encoding]::UTF8)
    Move-Item -LiteralPath $temp -Destination $readyFile -Force
}
function Get-ServerExitReason([string]$context) {
    if ((Test-Pubg2019MemoryFailureCode $server.ExitCode) -or (Test-Pubg2019MemoryFailureLog $engineLog $hostAttemptUtc)) {
        return (Get-Pubg2019MemoryFailureMessage)
    }
    return ($context+'; code '+$server.ExitCode)
}
trap {
    $diagnostic=Get-Pubg2019LaunchFailure $_.Exception $script:PubgLaunchFile
    $reason=if($diagnostic){$diagnostic}else{$_.Exception.Message}
    try { Write-StartupState 'failed' $reason } catch { }
    if ($diagnostic) { Write-Host $diagnostic; exit 1 }
    break
}

Initialize-Pubg2019Portable 'Server'
$script:PubgLaunchFile='TslGame.exe'
$game=Join-Path $PSScriptRoot 'TslGame.exe'
$moduleName=if ($LanMode -or $ZombieRule -ne 'None') { 'PUBG2019PS-LAN.dll' } else { 'PUBG2019PS.dll' }
$dll=Join-Path $PSScriptRoot $moduleName
$injector=Join-Path $PSScriptRoot 'Injector.exe'
foreach ($file in @($game,$dll,$injector,(Join-Path $PSScriptRoot 'Defaults\settings.ini'))) {
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw ('Missing required file: '+$file) }
}
if ((Get-FileHash -LiteralPath $game -Algorithm SHA256).Hash -ine 'C56464E44914719A53E8614346360075F8CEB79F4DE0A93A29A589DCF24B429B') { throw 'Unsupported PUBG 2019 executable; this candidate requires the tested game build.' }
if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'settings.ini'))) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Defaults\settings.ini') -Destination (Join-Path $PSScriptRoot 'settings.ini')
}
$previous=@(Get-CimInstance Win32_Process -Filter "Name='TslGame.exe'" | Where-Object { $_.ExecutablePath -ieq $game -and $_.CommandLine -match '(?i)(?:^|\s)[A-Za-z0-9_]+_Main\?listen(?:\?|\s|$)' })
if ($previous.Count) { throw 'A game server is already running from this folder.' }
if (@(Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue).Count) { throw ('UDP port already in use: '+$Port) }
$log=Join-Path $PSScriptRoot 'PUBG2019PS_Server.log'
$offset=if (Test-Path -LiteralPath $log) { (Get-Item -LiteralPath $log).Length } else { 0 }
$script:logPosition=$offset
$script:logPartial=''
function Write-ServerLogUpdates {
    if (!$LauncherMode -or !(Test-Path -LiteralPath $log)) { return }
    try {
        $stream=[IO.File]::Open($log,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
        try {
            if ($stream.Length -lt $script:logPosition) { $script:logPosition=0; $script:logPartial='' }
            $null=$stream.Seek($script:logPosition,[IO.SeekOrigin]::Begin)
            $reader=New-Object IO.StreamReader($stream,[Text.Encoding]::UTF8)
            try { $chunk=$reader.ReadToEnd(); $script:logPosition=$stream.Position } finally { $reader.Dispose() }
        } finally { $stream.Dispose() }
        if (!$chunk) { return }
        $parts=($script:logPartial+$chunk) -split "`r?`n"
        $script:logPartial=$parts[$parts.Count-1]
        for ($i=0; $i -lt $parts.Count-1; $i++) { if ($parts[$i]) {
            if ($LanMode -and $parts[$i].Contains('BOT ZONE observed closing completed 2')) {
                [IO.File]::WriteAllText((Join-Path $PSScriptRoot 'LAN_LateJoinClosed.flag'),'1',[Text.Encoding]::ASCII)
            }
            if ($LanMode -and ($parts[$i].Contains('Match over (') -or $parts[$i].Contains('Match state -> WaitingPostMatch'))) {
                [IO.File]::WriteAllText((Join-Path $PSScriptRoot 'LAN_MatchEnded.flag'),'1',[Text.Encoding]::ASCII)
            }
            Write-Host ('SERVER: '+$parts[$i])
        } }
    } catch { }
}
$zombieRules=@{ BattleRoyale='1'; Survival='2'; Elimination='3' }
$gameOption=if($ZombieRule -ne 'None'){'?game=/Game/Blueprints/TslGameMode_ZombieBP.TslGameMode_ZombieBP_C'}else{''}
$arguments=($level+'?listen'+$gameOption+' -port='+$Port+' -LOG -nullrhi -nosound -unattended -AllowJoinAnyMatchState -NoBattleEye -NoBE -NoEAC -NoVerifyGC -NotInstalled '+((Get-Pubg2019PortableArguments 'Server') -join ' '))
$arguments+=' -abslog="'+$engineLog+'"'
$previousZombieRule=[Environment]::GetEnvironmentVariable('PUBG2019_ZOMBIE_RULE','Process')
[Environment]::SetEnvironmentVariable('PUBG2019_ZOMBIE_RULE',$(if($ZombieRule -ne 'None'){$zombieRules[$ZombieRule]}else{$null}),'Process')
if ($LauncherMode) { $env:PUBG2019_LAUNCHER_LOG='1' }
try {
    if ($LauncherMode) {
        $server=Start-Process -FilePath $game -WorkingDirectory $PSScriptRoot -ArgumentList $arguments -WindowStyle Hidden -PassThru
    } else {
        $server=Start-Process -FilePath $game -WorkingDirectory $PSScriptRoot -ArgumentList $arguments -PassThru
    }
} finally {
    [Environment]::SetEnvironmentVariable('PUBG2019_ZOMBIE_RULE',$previousZombieRule,'Process')
    if ($LauncherMode) { Remove-Item Env:PUBG2019_LAUNCHER_LOG -ErrorAction SilentlyContinue }
}
$null=$server.Handle
Write-StartupState 'starting'
try {
Write-Host ($Map+' server PID '+$server.Id+'; waiting for map loading.')
$deadline=[DateTime]::UtcNow.AddSeconds([Math]::Max(180,$LoadWait+120))
$started=[DateTime]::UtcNow
# The extension waits for native map readiness itself; this is only the
# minimal process startup delay, and applies consistently to every map.
$effectiveLoadWait=$LoadWait
while ($true) {
    $server.Refresh(); if ($server.HasExited) { throw (Get-ServerExitReason 'Server exited while loading') }
    $bound=@(Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue | Where-Object { $_.OwningProcess -eq $server.Id }).Count -gt 0
    if ($bound -and ([DateTime]::UtcNow-$started).TotalSeconds -ge $effectiveLoadWait) { break }
    if ([DateTime]::UtcNow -ge $deadline) { throw 'Server did not open its UDP port during map loading.' }
    Start-Sleep -Seconds 2
}
Invoke-Pubg2019Injector -Injector $injector -TargetProcessId $server.Id -Module $moduleName
$deadline=[DateTime]::UtcNow.AddSeconds(180)
while ($true) {
    $server.Refresh(); if ($server.HasExited) { throw (Get-ServerExitReason 'Server exited during initialization') }
    Write-ServerLogUpdates
    $fresh=''
    if (Test-Path -LiteralPath $log) {
        $stream=[IO.File]::Open($log,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
        try {
            if ($stream.Length -ge $offset) { $null=$stream.Seek($offset,[IO.SeekOrigin]::Begin) }
            $reader=New-Object IO.StreamReader($stream)
            try { $fresh=$reader.ReadToEnd() } finally { $reader.Dispose() }
        } finally { $stream.Dispose() }
    }
    if ($fresh.Contains('guard installation failed')) { Stop-Process -Id $server.Id -Force; throw 'Crash guard installation failed; the test server was stopped.' }
    if ($fresh.Contains('Native startup refused:') -or $fresh.Contains('Match startup refused:') -or $fresh.Contains('Native match accounting refused:')) { throw 'Server initialization was refused; inspect PUBG2019PS_Server.log.' }
    if ($fresh -match '(?m)^MatchState: (WaitingPostMatch|LeavingMap|Aborted)\s*$') {
        throw 'Server reached an ended match before it was ready. Close the server and start it again; see PUBG2019PS_Server.log.'
    }
    if ($fresh.Contains('ProcessEvent hooked - bots are live.') -and $fresh.Contains('First AI think OK') -and $fresh.Contains('Server terrain ready:') -and $fresh.Contains('Match startup ready:') -and $fresh.Contains('Native null guard installed') -and $fresh.Contains('second null guard installed') -and $fresh.Contains('character null guard installed')) { break }
    if ([DateTime]::UtcNow -ge $deadline) {
        $terrainMatches=[regex]::Matches($fresh,'Server terrain loading:\s*(\d+)/(\d+) detailed')
        if ($terrainMatches.Count -gt 0) {
            $last=$terrainMatches[$terrainMatches.Count-1]
            throw ("Server initialization timed out: detailed terrain " + $last.Groups[1].Value + '/' + $last.Groups[2].Value + ' loaded. See PUBG2019PS_Server.log.')
        }
        throw 'Server initialization timed out; inspect PUBG2019PS_Server.log.'
    }
    Start-Sleep -Seconds 2
}
Write-StartupState 'ready'
Write-Host ($Map+' ready. Join 127.0.0.1:'+$Port+' on this PC, or use the host address from another PC.')
Write-Host 'Keep this window open while the server runs.'
while ($true) {
    $server.Refresh()
    Write-ServerLogUpdates
    if ($server.HasExited) { break }
    Start-Sleep -Milliseconds 500
}
Write-Host ($Map+' server exited: '+$server.ExitCode)
if ($server.ExitCode -ne 0 -or (Test-Pubg2019MemoryFailureLog $engineLog $hostAttemptUtc)) { throw (Get-ServerExitReason 'Server exited unexpectedly') }
Write-StartupState 'stopped'
} catch {
    $diagnostic=Get-Pubg2019LaunchFailure $_.Exception $script:PubgLaunchFile
    $reason=if ($diagnostic) { $diagnostic } else { $_.Exception.Message }
    try { Write-StartupState 'failed' $reason } catch { }
    # A failed initialization cannot accept clients. Do not leave its game
    # process occupying the UDP port and blocking the next host attempt.
    try { $server.Refresh(); if (!$server.HasExited) { Stop-Process -Id $server.Id -Force -ErrorAction Stop } } catch { }
    if ($diagnostic) { Write-Host $diagnostic; exit 1 }
    throw
}
