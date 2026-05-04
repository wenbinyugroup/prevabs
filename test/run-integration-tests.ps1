<#
.SYNOPSIS
  Run or clean PreVABS integration tests via CTest.

.DESCRIPTION
  run  (default) — cmake configure + ctest
  clean          — delete generated outputs, keep INDEX.txt and listed source files

  Test names follow the pattern <dir>/<case>, e.g. t1_strip/strip.
  CTest -R accepts a regex, so "t1" matches all t1_strip/* cases.

.PARAMETER Mode
  run | clean  (default: run)

.PARAMETER Filter
  run:   passed to ctest -R, e.g. "t1" or "t1_strip/strip$"
  clean: directory-name prefix, e.g. "t6" matches t6_circle

.EXAMPLE
  # Run all tests
  pwsh -File test\run-integration-tests.ps1

  # Run only t1_strip tests
  pwsh -File test\run-integration-tests.ps1 -Filter t1

  # Run only one specific case
  pwsh -File test\run-integration-tests.ps1 -Filter "t1_strip/strip$"

  # Clean all generated outputs
  pwsh -File test\run-integration-tests.ps1 clean

  # Clean only t6_circle
  pwsh -File test\run-integration-tests.ps1 clean -Filter t6
#>
param (
    [ValidateSet("run", "clean")]
    [string]$Mode   = "run",
    [string]$Filter = ""
)

$ErrorActionPreference = "Stop"

$repoRoot       = Resolve-Path (Join-Path $PSScriptRoot "..")
$integrationDir = Join-Path $PSScriptRoot "integration"
$buildDir       = Join-Path $integrationDir "build_msvc"
$prevabs        = Join-Path $repoRoot "build_msvc\Release\prevabs.exe"

# ── clean ────────────────────────────────────────────────────────────────────
if ($Mode -eq "clean") {
    $dirs = Get-ChildItem -Path $integrationDir -Directory |
            Where-Object { $_.Name -like "t*" }
    if ($Filter) {
        $dirs = $dirs | Where-Object { $_.Name -match "^$Filter" }
    }

    foreach ($dir in $dirs) {
        $indexFile = Join-Path $dir.FullName "INDEX.txt"
        if (-not (Test-Path $indexFile)) { continue }

        $keep = [System.Collections.Generic.HashSet[string]]::new(
            [System.StringComparer]::OrdinalIgnoreCase)
        $keep.Add("INDEX.txt") | Out-Null
        foreach ($line in Get-Content $indexFile) {
            if ($line -match "^-\s+(.+)$") {
                $keep.Add($Matches[1].Trim()) | Out-Null
            }
        }

        Write-Host "Cleaning $($dir.Name)..."
        Get-ChildItem -Path $dir.FullName -File |
        Where-Object { -not $keep.Contains($_.Name) } |
        ForEach-Object {
            Write-Host "  Del: $($_.Name)"
            Remove-Item $_.FullName
        }
    }
    exit 0
}

# ── locate cmake/ctest ───────────────────────────────────────────────────────
function Find-Tool([string]$name) {
    # 1. Already in PATH
    $found = Get-Command $name -ErrorAction SilentlyContinue
    if ($found) { return $found.Source }

    # 2. Bundled with Visual Studio (via vswhere)
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -prerelease -products * -property installationPath 2>$null
        if ($vsPath) {
            # cmake and ctest live under CMake\bin; ninja lives under Ninja\bin
            foreach ($rel in @("Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
                               "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja")) {
                $candidate = Join-Path $vsPath "$rel\$name.exe"
                if (Test-Path $candidate) { return $candidate }
            }
        }
    }

    # 3. Standalone CMake installer default location
    $candidate = "C:\Program Files\CMake\bin\$name.exe"
    if (Test-Path $candidate) { return $candidate }

    return $null
}

$cmake = Find-Tool "cmake"
$ctest = Find-Tool "ctest"
if (-not $cmake) { Write-Error "cmake not found. Install CMake or open a VS Developer shell."; exit 1 }
if (-not $ctest) { Write-Error "ctest not found. Install CMake or open a VS Developer shell."; exit 1 }

# ── run ──────────────────────────────────────────────────────────────────────
if (-not (Test-Path $prevabs)) {
    Write-Error "prevabs.exe not found at $prevabs`nBuild the project first."
    exit 1
}

# Always re-configure so INDEX.txt changes are picked up immediately.
# Wipe the build dir first: cmake refuses to switch generators on an existing cache.
# Configure-only (no compilation) is fast, so a clean slate each time is fine.
# Use Ninja (single-config) when available to avoid needing "-C <config>" with
# ctest. VS ships Ninja alongside its bundled cmake, so the vswhere path works.
Remove-Item -Recurse -Force $buildDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $buildDir | Out-Null
$cmakeArgs = @("-S", $integrationDir, "-B", $buildDir, "-DPREVABS:FILEPATH=$prevabs")
$ninja = Find-Tool "ninja"
if ($ninja) { $cmakeArgs += @("-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=$ninja") }
& $cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$ctestArgs = @("--test-dir", $buildDir, "--output-on-failure")
if (-not $ninja) { $ctestArgs += @("-C", "Release") }
if ($Filter) { $ctestArgs += @("-R", $Filter) }
& $ctest @ctestArgs
exit $LASTEXITCODE
