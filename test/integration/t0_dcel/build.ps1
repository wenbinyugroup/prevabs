# Standalone build for the t0_dcel harness.
# Usage:
#   pwsh -NoProfile -ExecutionPolicy Bypass -File build.ps1 [full|fast|clean] [-Run]

[CmdletBinding()]
param(
  [ValidateSet('full','fast','clean')]
  [string]$Mode = 'fast',
  [switch]$Run
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Definition
$build = Join-Path $here 'build_msvc'

# Enter VS dev shell so cmake + MSVC are on PATH (same pattern as build-msvc.ps1).
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstallPath = & $vswhere -latest -prerelease -products * -property installationPath
if (-not $vsInstallPath) { throw "Visual Studio not found via vswhere" }
$devShellDll = Get-ChildItem -Path $vsInstallPath -Recurse `
  -Filter "Microsoft.VisualStudio.DevShell.dll" |
  Sort-Object -Property FullName -Descending | Select-Object -First 1
Import-Module $devShellDll.FullName
Enter-VsDevShell -VsInstallPath $vsInstallPath -SkipAutomaticLocation `
  -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null

if ($Mode -eq 'clean') {
  if (Test-Path $build) { Remove-Item -Recurse -Force $build }
  Write-Host "cleaned $build"
  return
}

if ($Mode -eq 'full' -and (Test-Path $build)) {
  Remove-Item -Recurse -Force $build
}
if (-not (Test-Path $build)) {
  New-Item -ItemType Directory -Path $build | Out-Null
}

Push-Location $build
try {
  if (-not (Test-Path 'CMakeCache.txt')) {
    & cmake -G "Visual Studio 17 2022" -A x64 $here
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
  }
  & cmake --build . --config Release
  if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }
}
finally { Pop-Location }

$exe = Join-Path $build 'Release\t0_dcel.exe'
Write-Host "built: $exe"

if ($Run) {
  & $exe
  $rc = $LASTEXITCODE
  Write-Host "t0_dcel exit code: $rc"
  Write-Host "outputs in: $(Join-Path $build 'out')"
  if ($rc -ne 0) { throw "t0_dcel run reported failures" }
}
