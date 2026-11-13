#Requires -RunAsAdministrator
param(
    [string]$GameDirectory=$PSScriptRoot,
    [ValidateRange(1,65535)][int]$RoomPort=18887,
    [ValidateRange(1,65535)][int]$GamePort=8888,
    [switch]$DedicatedPorts
)
$ErrorActionPreference='Stop'
foreach($file in @('PUBG2019Launcher.exe','TslGame.exe')) {
    if(!(Test-Path -LiteralPath (Join-Path $GameDirectory $file) -PathType Leaf)) {
        throw ('Not a PUBG 2019 installation: missing '+$file)
    }
}
# Room TCP and game UDP are separate services. A visible UDP advertisement
# does not establish that the room's TCP listener is reachable.
# Port rules also cover dedicated workers launched from isolated copies.
$specs=@(
    @{Name='PUBG2019-Launcher-Room-TCP';Label='PUBG 2019 room connections';Protocol='TCP';Ports=@([string]$RoomPort)},
    @{Name='PUBG2019-Game-UDP';Label='PUBG 2019 gameplay';Protocol='UDP';Ports=@([string]$GamePort)}
)
if($DedicatedPorts) {
    $specs+=@(
        @{Name='PUBG2019-Dedicated-TCP';Label='PUBG 2019 dedicated rooms';Protocol='TCP';Ports=@('8000-10000')},
        @{Name='PUBG2019-Dedicated-UDP';Label='PUBG 2019 dedicated gameplay';Protocol='UDP';Ports=@('8000-10000')}
    )
}
foreach($spec in $specs) {
    Get-NetFirewallRule -Name $spec.Name -ErrorAction SilentlyContinue | Remove-NetFirewallRule
    New-NetFirewallRule -Name $spec.Name -DisplayName $spec.Label -Group 'PUBG 2019' `
        -Description 'Inbound traffic for the configured PUBG 2019 hosting ports.' `
        -Direction Inbound -Action Allow -Enabled True -Profile Any `
        -Protocol $spec.Protocol -LocalPort $spec.Ports | Out-Null
    Write-Output ($spec.Label+': '+$spec.Protocol+' '+($spec.Ports -join ', '))
}
