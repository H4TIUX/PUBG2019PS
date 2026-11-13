param([ValidateSet('host')][string]$Role,[Parameter(Mandatory=$true)][string]$Payload,[Parameter(Mandatory=$true)][string]$Root,[Parameter(Mandatory=$true)][string]$GameDir)
$ErrorActionPreference='Stop'
$src=[IO.Path]::GetFullPath($GameDir);$game=[IO.Path]::GetFullPath((Join-Path $src '..\..\..'));$bin=Join-Path $Root 'TslGame\Binaries\Win64'
if(Test-Path -LiteralPath $Root){throw 'Instance directory already exists.'}
[IO.Directory]::CreateDirectory($bin)|Out-Null
[IO.File]::WriteAllText((Join-Path $Root 'quickmatch-instance.marker'),'PUBG2019 Quick Match instance')
foreach($rel in @('Engine','TslGame\Content','TslGame\Config','TslGame\Plugins','TslGame\Binaries\ThirdParty')){if(Test-Path (Join-Path $game $rel)){New-Item -ItemType Junction -Path (Join-Path $Root $rel) -Target (Join-Path $game $rel)|Out-Null}}
$sourceGame=Join-Path $src 'TslGame.exe';$targetGame=Join-Path $bin 'TslGame.exe'
try {New-Item -ItemType HardLink -Path $targetGame -Target $sourceGame -ErrorAction Stop | Out-Null} catch {Copy-Item -LiteralPath $sourceGame -Destination $targetGame}
foreach($f in @('Injector.exe','PUBG2019PS-LAN.dll','PUBG2019PlayerName.dll','Host-Server.ps1','Join-Server.ps1','Portable2019Profile.ps1')){Copy-Item -LiteralPath (Join-Path $src $f) -Destination $bin}
Get-ChildItem -LiteralPath $src -File -Filter '*.dat' | Copy-Item -Destination $bin
Copy-Item -LiteralPath (Join-Path $src 'Defaults') -Destination $bin -Recurse
foreach($f in @('PUBG2019Launcher.exe','NativePairAudit.exe','PreviewDesktop.exe')){Copy-Item -LiteralPath (Join-Path $Payload $f) -Destination $bin}
[IO.File]::WriteAllText((Join-Path $bin 'PlayerName.txt'),'QuickHost',[Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $bin 'PUBG2019Launcher.ini'),("PlayerName=QuickHost`r`nMap=Miramar`r`nBotCount=95`r`nAutoJoin=0`r`n"),[Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $bin 'PUBG2019Profile.ini'),("PlayerName=QuickHost`r`nGameLanguage=en`r`n"),[Text.Encoding]::ASCII)
Write-Output ('READY '+$bin)
