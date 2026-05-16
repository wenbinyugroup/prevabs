<#
.SYNOPSIS
  Build (and optionally run) the standalone Clipper2 airfoil test
  suite via MSVC.

.DESCRIPTION
  Implements A0 of plan-20260515-progress-review-and-next-steps.md
  §4.3.1: a self-contained test executable that loads UIUC-style
  airfoil .dat files and exercises Clipper2 InflatePaths against them.
  No PreVABS source is compiled.

  Clipper2 is pulled via CMake FetchContent (tag Clipper2_1.4.0).
  Catch2 amalgamated is read from test/unit/.

.PARAMETER Mode
  one of: full | fast | clean

.PARAMETER Log
  Path to the log file. Defaults to
  test\geo_library\clipper2\build_msvc\build-<timestamp>.log

.PARAMETER Run
  If specified, run the test executable after a successful build.

.EXAMPLE
  pwsh -NoProfile -ExecutionPolicy Bypass `
    -File test\geo_library\clipper2\build-clipper2-airfoil-tests.ps1 full -Run
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

# ----- Visual Studio devshell -----------------------------------------------
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstallPath = & $vswhere -latest -prerelease -products * -property installationPath
if (-not $vsInstallPath) {
  Write-Error "Visual Studio not found via vswhere"
  exit 1
}
$devShellDll = Get-ChildItem -Path $vsInstallPath -Recurse -Filter "Microsoft.VisualStudio.DevShell.dll" |
               Sort-Object -Property FullName -Descending |
               Select-Object -First 1
Import-Module $devShellDll.FullName
Enter-VsDevShell -VsInstallPath $vsInstallPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'

# ----- paths ----------------------------------------------------------------
$srcDir   = $PSScriptRoot
$buildDir = Join-Path $srcDir "build_msvc"

if ($Mode -ne 'clean') {
    if ($Log -eq "") {
        $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
        New-Item -ItemType Directory -Force $buildDir | Out-Null
        $Log = Join-Path $buildDir "build-$timestamp.log"
    }
    $logPath = [System.IO.Path]::GetFullPath($Log)
    Write-Host "Build log: $logPath"
}

function Write-ToLog {
    param([string]$Line)
    Write-Host $Line
    [System.IO.File]::AppendAllText($logPath, "$Line`r`n", [System.Text.Encoding]::UTF8)
}

function Invoke-Logged {
    param([string]$Cmd, [string[]]$CmdArgs)
    & $Cmd @CmdArgs 2>&1 | ForEach-Object { Write-ToLog "$_" }
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Command failed (exit $LASTEXITCODE): $Cmd $($CmdArgs -join ' ')"
        exit $LASTEXITCODE
    }
}

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
    $exeRelease = Join-Path $buildDir "Release\test_clipper2_airfoil.exe"
    $exeRoot    = Join-Path $buildDir "test_clipper2_airfoil.exe"
    $exe = if (Test-Path $exeRelease) { $exeRelease } else { $exeRoot }
    Write-ToLog "=== running test_clipper2_airfoil ($exe) ==="
    Invoke-Logged $exe @()
    Write-ToLog "=== test_clipper2_airfoil finished ==="
}
