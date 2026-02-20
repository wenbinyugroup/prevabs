<#
.SYNOPSIS
  Setup MSVC environment via Developer PowerShell and run build steps.

.PARAMETER Mode
  one of: full | fast | clean

.PARAMETER Log
  Path to the log file. Defaults to build_msvc\build-<timestamp>.log

#>
param (
  [Parameter(Mandatory=$true)]
  [ValidateSet("full","fast","clean")]
  [string]$Mode,

  [Parameter(Mandatory=$false)]
  [string]$Log = ""
)

# Locate the latest Visual Studio install via vswhere
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstallPath = & $vswhere -latest -prerelease -products * -property installationPath

if (-not $vsInstallPath) {
  Write-Error "Visual Studio not found via vswhere"
  exit 1
}

# Import the DevShell module (searching recursively)
$devShellDll = Get-ChildItem -Path $vsInstallPath -Recurse -Filter "Microsoft.VisualStudio.DevShell.dll" |
               Sort-Object -Property FullName -Descending |
               Select-Object -First 1

Import-Module $devShellDll.FullName

# Enter the DevShell environment
Enter-VsDevShell -VsInstallPath $vsInstallPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'

# Resolve log file path (not used for 'clean' mode)
if ($Mode -ne 'clean') {
    if ($Log -eq "") {
        $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
        New-Item -ItemType Directory -Force .\build_msvc | Out-Null
        $Log = ".\build_msvc\build-$timestamp.log"
    }
    $logPath = [System.IO.Path]::GetFullPath($Log)
    Write-Host "Build log: $logPath"
}

# Helper: print a line to the console and append it to the log file (UTF-8, no BOM)
# Works on Windows PowerShell 5.1 and PowerShell 7+.
function Write-ToLog {
    param([string]$Line)
    Write-Host $Line
    [System.IO.File]::AppendAllText($logPath, "$Line`r`n", [System.Text.Encoding]::UTF8)
}

# Helper: run a command, mirror stdout+stderr to console and log file
function Invoke-Logged {
    param([string]$Cmd, [string[]]$CmdArgs)
    & $Cmd @CmdArgs 2>&1 | ForEach-Object { Write-ToLog "$_" }
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Command failed (exit $LASTEXITCODE): $Cmd $($CmdArgs -join ' ')"
        exit $LASTEXITCODE
    }
}

# Now run build logic
switch ($Mode) {
    'full' {
        Remove-Item -Recurse -Force .\build_msvc -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force .\build_msvc | Out-Null
        Write-ToLog "=== full build started at $(Get-Date) ==="
        Set-Location .\build_msvc
        Invoke-Logged cmake @("..")
        Invoke-Logged cmake @("--build", ".", "--config", "Release")
        Set-Location ..
        Write-ToLog "=== build finished at $(Get-Date) ==="
    }
    'fast' {
        Write-ToLog "=== fast build started at $(Get-Date) ==="
        Set-Location .\build_msvc
        Invoke-Logged cmake @("--build", ".", "--config", "Release")
        Set-Location ..
        Write-ToLog "=== build finished at $(Get-Date) ==="
    }
    'clean' {
        Remove-Item -Recurse -Force .\build_msvc -ErrorAction SilentlyContinue
    }
}
