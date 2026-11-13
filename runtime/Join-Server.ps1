param([string]$Address='127.0.0.1:8888', [switch]$AskAddress, [string]$ClientIdentityFile='',
    [ValidateSet('','en','ko','zh-CN','zh-TW','ar','de','es','es-MX','fr','it','pl','pt','pt-BR','tr','ru','ja','th','vi','id','uk','ga-IE')][string]$Language='')
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
. (Join-Path $PSScriptRoot 'Portable2019Profile.ps1')
$script:PubgLaunchFile='Portable2019'
trap {
    $diagnostic=Get-Pubg2019LaunchFailure $_.Exception $script:PubgLaunchFile
    if ($diagnostic) { Write-Host $diagnostic; exit 1 }
    break
}

Initialize-Pubg2019Portable 'Client'
$script:PubgLaunchFile='TslGame.exe'
$game=Join-Path $PSScriptRoot 'TslGame.exe'
if (!(Test-Path -LiteralPath $game -PathType Leaf)) { throw 'Extract the add-on into the 2019 game TslGame\Binaries\Win64 folder.' }
if ($AskAddress) { $entered=Read-Host 'Server address (Enter = 127.0.0.1:8888)'; if ($entered.Trim()) { $Address=$entered.Trim() } }
if ($Address -notmatch '^[A-Za-z0-9][A-Za-z0-9.\-]*(?::[0-9]{1,5})?$') { throw 'Use a hostname or IPv4 address, optionally followed by :port.' }
if ($Address -match ':(\d+)$' -and ([int]$Matches[1] -lt 1 -or [int]$Matches[1] -gt 65535)) { throw 'Port must be 1..65535.' }
# Only the local server started by our host launcher writes this state file.
# Remote servers and other launchers are left to their existing behavior.
if ($Address -match '^(?i:127\.0\.0\.1|localhost)(?::(\d+))?$') {
    $joinPort=if ($Matches[1]) { [int]$Matches[1] } else { 8888 }
    $readyFile=Join-Path $PSScriptRoot 'ServerStartupState.json'
    if (Test-Path -LiteralPath $readyFile) {
        $deadline=[DateTime]::UtcNow.AddSeconds(180)
        $announced=$false
        while ($true) {
            try { $state=Get-Content -LiteralPath $readyFile -Raw | ConvertFrom-Json } catch { $state=$null }
            if (!$state -or [int]$state.port -ne $joinPort) { break }
            $proc=Get-CimInstance Win32_Process -Filter ('ProcessId='+[int]$state.processId) -ErrorAction SilentlyContinue
            if (!$proc -or $proc.ExecutablePath -ine $game -or $proc.CommandLine -notmatch '(?i)_Main\?listen') { break }
            if ($state.status -eq 'failed') { throw ('Local server failed to start: '+$state.reason) }
            if ($state.status -eq 'stopped') { throw 'The local server has stopped. Start it again with 01.' }
            if ($state.status -eq 'ready') { break }
            if (!$announced) { Write-Host 'Waiting for the local server to finish starting...'; $announced=$true }
            if ([DateTime]::UtcNow -ge $deadline) { throw 'The local server is still not ready; check its host window and PUBG2019PS_Server.log.' }
            Start-Sleep -Seconds 2
        }
    }
}
$namePath=Join-Path $PSScriptRoot 'PlayerName.txt'
if (!(Test-Path -LiteralPath $namePath)) {
    $defaultName=Join-Path $PSScriptRoot 'Defaults\PlayerName.txt'
    if (!(Test-Path -LiteralPath $defaultName -PathType Leaf)) { throw 'Missing Defaults\PlayerName.txt.' }
    Copy-Item -LiteralPath $defaultName -Destination $namePath
}
$playerName=[IO.File]::ReadAllText($namePath).Trim()
if ($playerName -and $playerName -notmatch '^[A-Za-z0-9_-]{1,24}$') { throw 'PlayerName.txt must be empty or contain 1-24 letters, numbers, underscores, or hyphens.' }
foreach ($file in @('Injector.exe','PUBG2019PlayerName.dll')) {
    if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot $file) -PathType Leaf)) { throw ('Missing required file: '+$file) }
}
if ((Get-FileHash -LiteralPath $game -Algorithm SHA256).Hash -ine 'C56464E44914719A53E8614346360075F8CEB79F4DE0A93A29A589DCF24B429B') { throw 'This client helper requires the supported PUBG 2019 executable.' }
$target=$Address
if ($playerName) {
    $target += '?Name=' + $playerName
    Write-Host ('Requested player name: '+$playerName)
}
$cultureArgument=if ($Language) { ' -culture='+$Language } else { '' }
if ($Language) {
    # This game also stores culture in TslGameUserSettings and can override the
    # Unreal command-line culture during startup. Change only those two keys.
    $settingsPath=Join-Path $PSScriptRoot 'Portable2019\Client\Config\WindowsNoEditor\GameUserSettings.ini'
    $raw=[IO.File]::ReadAllBytes($settingsPath)
    if ($raw.Length -ge 2 -and $raw[0] -eq 0xff -and $raw[1] -eq 0xfe) { $settingsEncoding=New-Object Text.UnicodeEncoding($false,$true,$true); $offset=2 }
    elseif ($raw.Length -ge 2 -and $raw[0] -eq 0xfe -and $raw[1] -eq 0xff) { $settingsEncoding=New-Object Text.UnicodeEncoding($true,$true,$true); $offset=2 }
    elseif ($raw.Length -ge 3 -and $raw[0] -eq 0xef -and $raw[1] -eq 0xbb -and $raw[2] -eq 0xbf) { $settingsEncoding=New-Object Text.UTF8Encoding($true,$true); $offset=3 }
    else { $settingsEncoding=New-Object Text.UTF8Encoding($false,$true); $offset=0 }
    $settingsText=$settingsEncoding.GetString($raw,$offset,$raw.Length-$offset)
    $sectionPattern='(?ims)^\[/Script/TslGame\.TslGameUserSettings\][^\r\n]*(?:\r?\n|$).*?(?=^\[|\z)'
    $section=[regex]::Match($settingsText,$sectionPattern)
    $newline=if ($settingsText.Contains("`r`n")) { "`r`n" } else { "`n" }
    if (!$section.Success) {
        $settingsText=$settingsText.TrimEnd("`r","`n")+$newline+$newline+'[/Script/TslGame.TslGameUserSettings]'+$newline
        $section=[regex]::Match($settingsText,$sectionPattern)
    }
    $updated=$section.Value
    foreach ($key in @('CultureName','LastCultureName')) {
        $pattern='(?im)^([ \t]*'+[regex]::Escape($key)+'[ \t]*=)[^\r\n]*'
        if ([regex]::IsMatch($updated,$pattern)) { $updated=[regex]::Replace($updated,$pattern,('$1'+$Language)) }
        else { $updated=[regex]::Replace($updated,'(?m)^(\[/Script/TslGame\.TslGameUserSettings\][^\r\n]*)(\r?\n|$)',('$1'+$newline+$key+'='+$Language+$newline),1) }
    }
    $newText=$settingsText.Substring(0,$section.Index)+$updated+$settingsText.Substring($section.Index+$section.Length)
    if ($newText -cne $settingsText) {
        $backupDir=Join-Path $PSScriptRoot 'UpdateBackups\Language'
        [IO.Directory]::CreateDirectory($backupDir) | Out-Null
        $backup=Join-Path $backupDir 'GameUserSettings.before-language.ini'
        if (!(Test-Path -LiteralPath $backup)) { [IO.File]::Copy($settingsPath,$backup,$false) }
        $temp=$settingsPath+'.language.'+[guid]::NewGuid().ToString('N')+'.tmp'
        $rollback=$temp+'.previous'
        try {
            [IO.File]::WriteAllText($temp,$newText,$settingsEncoding)
            $current=[IO.File]::ReadAllBytes($settingsPath)
            if ($current.Length -ne $raw.Length -or [Convert]::ToBase64String($current) -cne [Convert]::ToBase64String($raw)) { throw 'GameUserSettings.ini changed while preparing the language update. Please retry.' }
            [IO.File]::Replace($temp,$settingsPath,$rollback)
        }
        finally {
            if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Force }
            if (Test-Path -LiteralPath $rollback) { Remove-Item -LiteralPath $rollback -Force }
        }
    }
    Write-Host ('Requested game language: '+$Language+' (game settings and client startup)')
}
$portableArgs=(Get-Pubg2019PortableArguments 'Client') -join ' '
$client=Start-Process -FilePath $game -WorkingDirectory $PSScriptRoot -ArgumentList ($target+' -AllowJoinAnyMatchState -NoVerifyGC -NoBattleEye -NoBE -NoEAC -NotInstalled '+$portableArgs+$cultureArgument) -PassThru
if ($ClientIdentityFile) {
    $ticks=$client.StartTime.ToUniversalTime().Ticks
    [IO.File]::WriteAllText($ClientIdentityFile,([string]$client.Id+'|'+[string]$ticks),[Text.Encoding]::ASCII)
}
# Install before the first match HUD can appear. The helper waits for the game
# code signature and also provides zombie HUD compatibility when no name is set.
for ($i=0; $i -lt 2; $i++) { Start-Sleep -Seconds 1; $client.Refresh(); if ($client.HasExited) { throw 'Client exited before the helper loaded.' } }
Invoke-Pubg2019Injector -Injector (Join-Path $PSScriptRoot 'Injector.exe') -TargetProcessId $client.Id -Module 'PUBG2019PlayerName.dll'
if ($playerName) { Write-Host 'Player name request enabled. If the name is not shown, check PUBG2019ClientName.log.' }
else { Write-Host 'Client compatibility helper enabled.' }
