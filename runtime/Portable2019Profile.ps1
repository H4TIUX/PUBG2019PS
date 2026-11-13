$ErrorActionPreference='Stop'
function Initialize-Pubg2019Portable {
    param([Parameter(Mandatory=$true)][ValidateSet('Client','Server')][string]$Role)
    $target=Join-Path $PSScriptRoot ('Portable2019\'+$Role+'\Config\WindowsNoEditor')
    [IO.Directory]::CreateDirectory($target)|Out-Null
    $legacy=Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'Saved\Config\WindowsNoEditor'
    foreach($name in @('Engine','Game','Input','GameUserSettings','Scalability','DeviceProfiles','Hardware','Compat','GameplayTags','Lightmass')) {
        $path=Join-Path $target ($name+'.ini')
        if(-not(Test-Path -LiteralPath $path -PathType Leaf)) {
            $old=Join-Path $legacy ($name+'.ini')
            if($Role -eq 'Client' -and (Test-Path -LiteralPath $old -PathType Leaf)) {
                try {[IO.File]::Copy($old,$path,$false)} catch [IO.IOException] {if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw}}
            } else {
                try {$f=[IO.File]::Open($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read);$f.Dispose()} catch [IO.IOException] {if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw}}
            }
        }
    }
}
function Get-Pubg2019PortableArguments {
    param([Parameter(Mandatory=$true)][ValidateSet('Client','Server')][string]$Role)
    $target=Join-Path $PSScriptRoot ('Portable2019\'+$Role+'\Config\WindowsNoEditor')
    foreach($name in @('Engine','Game','Input','GameUserSettings','Scalability','DeviceProfiles','Hardware','Compat','GameplayTags','Lightmass')) {
        '-'+$name+'INI="'+(Join-Path $target ($name+'.ini'))+'"'
    }
}

function Test-Pubg2019MemoryFailureCode {
    param([int]$Code)
    $status=[BitConverter]::ToUInt32([BitConverter]::GetBytes($Code),0)
    return ($Code -in @(8,14,1455) -or $status -in @(2147942414,3221225495,3221225773))
}
function Get-Pubg2019MemoryFailureMessage {
    return 'Not enough memory to run the server. Windows or PUBG reported a memory allocation failure. Free memory or increase the Windows paging file, then try again.'
}
function Test-Pubg2019MemoryFailureLog {
    param([string]$Path,[DateTime]$StartedUtc)
    # Inspect only a current-run log, and cap reads even after a long match.
    if (!(Test-Path -LiteralPath $Path) -or [IO.File]::GetLastWriteTimeUtc($Path) -lt $StartedUtc) { return $false }
    try {
        $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
        try {
            $null=$stream.Seek([Math]::Max(0,$stream.Length-65536),[IO.SeekOrigin]::Begin)
            $reader=New-Object IO.StreamReader($stream)
            try { $tail=$reader.ReadToEnd() } finally { $reader.Dispose() }
        } finally { $stream.Dispose() }
        return $tail -match '(?im)(Ran out of memory allocating|Fatal error[^\r\n]*out of memory|Critical error[^\r\n]*out of memory|The paging file is too small for this operation to complete)'
    } catch { return $false }
}

# Stable diagnostic tokens are translated by the launcher's current UI language.
function Get-Pubg2019LaunchFailure {
    param([Exception]$Exception,[string]$Component='Launcher files')
    $label=($Component -replace '[\]\|\r\n]',' ').Trim()
    if (!$label) { $label='Launcher files' }
    $current=$Exception
    while ($null -ne $current) {
        if ($current -is [OutOfMemoryException] -or (Test-Pubg2019MemoryFailureCode $current.HResult) -or
            ($current -is [ComponentModel.Win32Exception] -and (Test-Pubg2019MemoryFailureCode $current.NativeErrorCode))) {
            return Get-Pubg2019MemoryFailureMessage
        }
        $existing=[regex]::Match($current.Message,'\[PUBG2019_(?:ACCESS_DENIED|ELEVATION_CANCELLED|INJECTOR_FAILED)\|-?\d+\|[^\]\r\n|]+\]')
        if ($existing.Success) { return $existing.Value }
        $code=0
        if ($current -is [UnauthorizedAccessException]) { $code=5 }
        elseif ($current -is [ComponentModel.Win32Exception]) { $code=$current.NativeErrorCode }
        elseif (($current.HResult -band 0xffff) -eq 5) { $code=5 }
        if ($code -eq 5 -or $code -eq 740) { return ('[PUBG2019_ACCESS_DENIED|'+$code+'|'+$label+']') }
        if ($code -eq 1223) { return ('[PUBG2019_ELEVATION_CANCELLED|'+$code+'|'+$label+']') }
        $current=$current.InnerException
    }
    return $null
}

function Invoke-Pubg2019Injector {
    param([Parameter(Mandatory=$true)][string]$Injector,
          [Parameter(Mandatory=$true)][int]$TargetProcessId,
          [Parameter(Mandatory=$true)][string]$Module)
    $component='TslGame.exe + '+[IO.Path]::GetFileName($Module)
    # Verify the actual rights needed for a LoadLibrary-style injection. Being
    # unelevated alone is not treated as a failure: same-integrity launches work.
    if (-not ('Pubg2019InjectionAccess' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class Pubg2019InjectionAccess {
 [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,int id);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
 public static int Check(int id) {
  IntPtr handle=OpenProcess(0x43a,false,id);
  if(handle==IntPtr.Zero)return Marshal.GetLastWin32Error();
  CloseHandle(handle);return 0;
 }
}
'@
    }
    $accessCode=[Pubg2019InjectionAccess]::Check($TargetProcessId)
    if ($accessCode -eq 5) { throw ('[PUBG2019_ACCESS_DENIED|5|'+$component+']') }
    if ($accessCode -ne 0) { throw ('Cannot access the target game process; Windows error '+$accessCode+'.') }
    $process=New-Object Diagnostics.Process
    $process.StartInfo=New-Object Diagnostics.ProcessStartInfo
    $process.StartInfo.FileName=$Injector
    $process.StartInfo.Arguments=('-p '+$TargetProcessId+' -i "'+$Module.Replace('"','\"')+'"')
    $process.StartInfo.WorkingDirectory=$PSScriptRoot
    $process.StartInfo.UseShellExecute=$false
    $process.StartInfo.CreateNoWindow=$true
    $process.StartInfo.RedirectStandardOutput=$true
    $process.StartInfo.RedirectStandardError=$true
    try {
        try { if (!$process.Start()) { throw 'The injector did not start.' } }
        catch {
            $diagnostic=Get-Pubg2019LaunchFailure $_.Exception 'Injector.exe'
            if ($diagnostic) { throw $diagnostic }
            throw
        }
        $stdout=$process.StandardOutput.ReadToEndAsync()
        $stderr=$process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        foreach ($line in (($stdout.Result+"`n"+$stderr.Result) -split "`r?`n")) { if ($line) { Write-Host $line } }
        $code=$process.ExitCode
        if ($code -ne 0) { throw ('[PUBG2019_INJECTOR_FAILED|'+$code+'|'+[IO.Path]::GetFileName($Module)+']') }
    } finally { $process.Dispose() }
}
