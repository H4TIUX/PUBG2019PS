param([Parameter(Mandatory=$true)][string]$Root,[Parameter(Mandatory=$true)][string]$Payload,[int]$MaxInstances=8,[long]$MinimumFreeKB=0,[string]$DirectoryUrl="http://127.0.0.1:18900/",[string]$GameDir="",[string]$DisplayName="",[string]$DisplayNameBase64="",[int]$Capacity=100,[int]$BotCount=95)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
if(!$GameDir){$GameDir=Split-Path -Parent $Payload}
$GameDir=[IO.Path]::GetFullPath($GameDir)
# Retain the old parameter for command-line compatibility; it no longer blocks starts.
. (Join-Path $GameDir 'Portable2019Profile.ps1')
if(!(Test-Path -LiteralPath (Join-Path $GameDir 'TslGame.exe'))){throw 'PUBG 2019 game directory is invalid.'}
if($DisplayNameBase64){try{$DisplayName=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($DisplayNameBase64))}catch{throw 'Invalid server name encoding.'}}
if(!$DisplayName){$DisplayName='QUICK MATCH - '+$env:COMPUTERNAME}
if($Capacity -lt 1 -or $Capacity -gt 100 -or $BotCount -lt 0 -or $BotCount -gt 95 -or $BotCount -ge $Capacity -or $MaxInstances -lt 1 -or $MaxInstances -gt 16){throw 'Invalid dedicated capacity, bot count or room limit.'}
if($DisplayName.Length -gt 32){$DisplayName=$DisplayName.Substring(0,32)}
if([string]::IsNullOrWhiteSpace($DisplayName) -or $DisplayName -match '[\r\n]'){throw 'Invalid Quick Match host name.'}
[IO.Directory]::CreateDirectory($Root)|Out-Null
$log=Join-Path $Root 'supervisor.log';$stop=Join-Path $Root 'stop-supervisor.flag'
$workers=New-Object System.Collections.ArrayList
$nextAttempt=[DateTime]::UtcNow
function Log([string]$message){[IO.File]::AppendAllText($log,([DateTime]::Now.ToString('yyyy-MM-dd HH:mm:ss')+' '+$message+"`r`n"))}
function Find-FreePort {
 foreach($port in 8000..10000){
  if(@($workers | Where-Object {$_.Port -eq $port}).Count){continue}
  $tcp=$null;$udp=$null
  try{$tcp=New-Object System.Net.Sockets.TcpListener([System.Net.IPAddress]::Any,$port);$tcp.Start();$udp=New-Object System.Net.Sockets.UdpClient($port);return $port}catch{}finally{if($udp){$udp.Close()};if($tcp){$tcp.Stop()}}
 }
 throw 'No free Quick Match port in 8000..10000.'
}
function Start-Worker {
 $port=Find-FreePort;$instance=Join-Path $Root ('instance-'+$port+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
 & (Join-Path $Payload 'SetupFixture.ps1') -Role host -Payload $Payload -Root $instance -GameDir $GameDir | Out-Null
 $bin=Join-Path $instance 'TslGame\Binaries\Win64';$saved=Join-Path $bin 'Saved';[IO.Directory]::CreateDirectory($saved)|Out-Null
 [IO.File]::WriteAllText((Join-Path $saved 'PUBG2019Directory.txt'),$DirectoryUrl)
 [IO.File]::WriteAllText((Join-Path $bin 'quick-port.txt'),[string]$port)
 [IO.File]::WriteAllText((Join-Path $bin 'quick-host-root.txt'),$GameDir)
 [IO.File]::WriteAllText((Join-Path $bin 'quick-host-name.txt'),$DisplayName)
 [IO.File]::WriteAllText((Join-Path $bin 'quick-capacity.txt'),[string]$Capacity)
 [IO.File]::WriteAllText((Join-Path $bin 'quick-bots.txt'),[string]$BotCount)
 $identity=[string]$PID+'|'+[string]([Diagnostics.Process]::GetCurrentProcess().StartTime.ToUniversalTime().Ticks);[IO.File]::WriteAllText((Join-Path $bin 'supervisor-identity.txt'),$identity)
 $preview=Join-Path $bin 'PreviewDesktop.exe';$worker=Join-Path $bin 'NativePairAudit.exe'
 $arg=('"'+$worker+'" "'+$bin+'" IA==')
 $process=Start-Process -FilePath $preview -ArgumentList $arg -WorkingDirectory $bin -WindowStyle Hidden -PassThru
 [void]$workers.Add([pscustomobject]@{Port=$port;Root=$instance;Bin=$bin;Process=$process;Started=[DateTime]::UtcNow})
 Log ('['+$port+'] started Quick Match instance')
}
function Stop-Workers {
 foreach($w in @($workers)){try{[IO.File]::WriteAllText((Join-Path $w.Bin 'stop-dedicated.flag'),'stop')}catch{}}
 $deadline=[DateTime]::UtcNow.AddSeconds(15)
 while([DateTime]::UtcNow -lt $deadline){$live=0;foreach($w in @($workers)){try{$w.Process.Refresh();if(!$w.Process.HasExited){$live++}}catch{}};if(!$live){break};Start-Sleep -Milliseconds 500}
 foreach($w in @($workers)){try{$w.Process.Refresh();if(!$w.Process.HasExited){$identity=Get-CimInstance Win32_Process -Filter ('ProcessId='+$w.Process.Id);if($identity.ExecutablePath -ieq (Join-Path $w.Bin 'PreviewDesktop.exe')){Stop-Process -Id $w.Process.Id}}}catch{}}
}
try{
 if(Test-Path -LiteralPath $stop){throw 'Stop marker exists; remove it before starting supervisor.'}
 Log ('Supervisor started: '+$DisplayName+', slots='+$Capacity+', bots='+$BotCount+', max rooms='+$MaxInstances)
 while(!(Test-Path -LiteralPath $stop)){
  $blocked=$false;$waiting=$false
  foreach($w in @($workers)){
   try{$w.Process.Refresh();if($w.Process.HasExited){Log ('['+$w.Port+'] instance exited');[void]$workers.Remove($w);continue}}catch{[void]$workers.Remove($w);continue}
   $file=Join-Path $w.Bin 'worker-state.txt';$state=if(Test-Path $file){try{[IO.File]::ReadAllText($file).Trim()}catch{'opening'}}else{'opening'}
   if($state -in @('authenticating','opening') -and ([DateTime]::UtcNow-$w.Started).TotalSeconds -gt 90){Log ('['+$w.Port+'] startup timed out');try{[IO.File]::WriteAllText((Join-Path $w.Bin 'stop-dedicated.flag'),'stop')}catch{}}
   if($state -in @('authenticating','opening','waiting')){$waiting=$true}
   if($state -eq 'starting'){$blocked=$true}
  }
  if(!$waiting -and !$blocked -and $workers.Count -lt $MaxInstances -and [DateTime]::UtcNow -ge $nextAttempt){
   try{Start-Worker;$nextAttempt=[DateTime]::UtcNow.AddSeconds(15)}catch{
    $reason=Get-Pubg2019LaunchFailure $_.Exception 'Quick Match room'
    if(!$reason){$reason=$_.Exception.Message}
    Log ('Worker start failed: '+$reason);$nextAttempt=[DateTime]::UtcNow.AddSeconds(20)
   }
  }
  Start-Sleep -Seconds 2
 }
 Log 'Supervisor stop requested'
} catch {Log ('Supervisor error: '+$_.Exception.Message);throw} finally {Stop-Workers}
