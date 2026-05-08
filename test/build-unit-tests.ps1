<#
.SYNOPSIS
  Build (and optionally run) PreVABS unit tests using MSVC via Developer PowerShell.

.DESCRIPTION
  Call from the repository root or any directory; $PSScriptRoot anchors all paths.
  The build directory is test\unit\build_msvc\.

.PARAMETER Mode
  one of: full | fast | clean

.PARAMETER Log
  Path to the log file. Defaults to test\unit\build_msvc\build-<timestamp>.log

.PARAMETER Run
  If specified, run the test executable after a successful build.

.EXAMPLE
  # Full clean build and run all tests
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File test\build-unit-tests.ps1 full -Run

  # Incremental rebuild only
  pwsh -NoProfile -ExecutionPolicy Bypass -File test\build-unit-tests.ps1 fast

  # Remove the build directory
  pwsh -NoProfile -ExecutionPolicy Bypass -File test\build-unit-tests.ps1 clean
#>
param (
  [Parameter(Mandatory=$true)]
  [ValidateSet("full","fast","clean")]
  [string]$Mode,

  [Parameter(Mandatory=$false)]
  [string]$Log = "",

  [Parameter(Mandatory=$false)]
  [switch]$Run
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

# All paths are relative to the script's own directory (test\)
$unitDir  = Join-Path $PSScriptRoot "unit"
$buildDir = Join-Path $unitDir "build_msvc"

# Resolve log file path (not used for 'clean' mode)
if ($Mode -ne 'clean') {
    if ($Log -eq "") {
        $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
        New-Item -ItemType Directory -Force $buildDir | Out-Null
        $Log = Join-Path $buildDir "build-$timestamp.log"
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
        Remove-Item -Recurse -Force $buildDir -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force $buildDir | Out-Null
        Write-ToLog "=== full build started at $(Get-Date) ==="
        Push-Location $buildDir
        Invoke-Logged cmake @("..", "-DCMAKE_BUILD_TYPE=Release")
        Invoke-Logged cmake @("--build", ".", "--config", "Release")
        Pop-Location
        Write-ToLog "=== build finished at $(Get-Date) ==="
    }
    'fast' {
        Write-ToLog "=== fast build started at $(Get-Date) ==="
        Push-Location $buildDir
        Invoke-Logged cmake @("--build", ".", "--config", "Release")
        Pop-Location
        Write-ToLog "=== build finished at $(Get-Date) ==="
    }
    'clean' {
        Remove-Item -Recurse -Force $buildDir -ErrorAction SilentlyContinue
    }
}

if ($Run -and $Mode -ne 'clean') {
    # Multi-config generators (VS) put executables in Release\; single-config at root.
    foreach ($name in @("test_geo", "test_dcel", "test_pui")) {
        $exeRelease = Join-Path $buildDir "Release\$name.exe"
        $exeRoot    = Join-Path $buildDir "$name.exe"
        $exe = if (Test-Path $exeRelease) { $exeRelease } else { $exeRoot }
        Write-ToLog "=== running $name ($exe) ==="
        Invoke-Logged $exe @()
        Write-ToLog "=== $name finished ==="
    }
}
