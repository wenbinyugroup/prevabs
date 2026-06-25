<#
.SYNOPSIS
  Run or clean PreVABS integration tests via CTest.

.DESCRIPTION
  run  (default) — cmake configure + ctest
  clean          — delete generated outputs, keep INDEX.txt and listed source files
  A run writes a Markdown report to test/integration/build_msvc/integration-report.md.
  Each case runs with PREVABS_SEGMENT_PATH=1, so every laminate segment emits a
  branch-trajectory file <case>.<segment>.segment.path.txt (which conditional
  path it took through buildAreas); the report ends with a "Build paths" section
  aggregating them. With -OpenGmsh, open each generated section in Gmsh after the run.

  Test names follow the pattern <dir>/<case>, e.g. t1_strip/strip.
  CTest -R accepts a regex, so "t1" matches all t1_strip/* cases.

.PARAMETER Mode
  run | clean  (default: run)

.PARAMETER Filter
  run:   passed to ctest -R, e.g. "t1" or "t1_strip/strip$"
  clean: directory-name prefix, e.g. "t6" matches t6_circle

.PARAMETER OpenGmsh
  After a run, start gmsh for every executed section case:
  gmsh <test-case>.geo_unrolled <test-case>.msh <test-case>.opt

.EXAMPLE
  # Run all tests
  pwsh -File test\run-integration-tests.ps1

  # Run only t1_strip tests
  pwsh -File test\run-integration-tests.ps1 -Filter t1

  # Run only one specific case
  pwsh -File test\run-integration-tests.ps1 -Filter "t1_strip/strip$"

  # Run one specific case and open its generated Gmsh view
  pwsh -File test\run-integration-tests.ps1 -Filter "t1_strip/strip$" -OpenGmsh

  # Clean all generated outputs
  pwsh -File test\run-integration-tests.ps1 clean

  # Clean only t6_circle
  pwsh -File test\run-integration-tests.ps1 clean -Filter t6
#>
param (
    [ValidateSet("run", "clean")]
    [string]$Mode   = "run",
    [string]$Filter = "",
    [switch]$OpenGmsh
)

$ErrorActionPreference = "Stop"

$repoRoot       = Resolve-Path (Join-Path $PSScriptRoot "..")
$integrationDir = Join-Path $PSScriptRoot "integration"
$buildDir       = Join-Path $integrationDir "build_msvc"
$prevabs        = Join-Path $repoRoot "build_msvc\Release\prevabs.exe"
$junitReport    = Join-Path $buildDir "integration-report.junit.xml"
$markdownReport = Join-Path $buildDir "integration-report.md"

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

function Find-Gmsh() {
    $found = Get-Command "gmsh" -ErrorAction SilentlyContinue
    if ($found) { return $found.Source }

    if ($env:Gmsh_ROOT) {
        foreach ($rel in @("bin\gmsh.exe", "gmsh.exe")) {
            $candidate = Join-Path $env:Gmsh_ROOT $rel
            if (Test-Path $candidate) { return $candidate }
        }
    }

    foreach ($candidate in @(
        "C:\Program Files\Gmsh\gmsh.exe",
        "C:\Program Files (x86)\Gmsh\gmsh.exe"
    )) {
        if (Test-Path $candidate) { return $candidate }
    }

    return $null
}

$cmake = Find-Tool "cmake"
$ctest = Find-Tool "ctest"
if (-not $cmake) { Write-Error "cmake not found. Install CMake or open a VS Developer shell."; exit 1 }
if (-not $ctest) { Write-Error "ctest not found. Install CMake or open a VS Developer shell."; exit 1 }
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue) {
    $PSNativeCommandUseErrorActionPreference = $false
}

function Format-Duration([string]$seconds) {
    $value = 0.0
    if (-not [double]::TryParse(
            $seconds,
            [System.Globalization.NumberStyles]::Float,
            [System.Globalization.CultureInfo]::InvariantCulture,
            [ref]$value)) {
        return ""
    }
    return ("{0:0.###}s" -f $value)
}

function Get-XmlAttribute($node, [string]$name) {
    if (-not $node -or -not $node.Attributes) { return "" }
    if ($node.Attributes[$name]) { return $node.Attributes[$name].Value }
    return ""
}

function Get-XmlIntAttribute($node, [string]$name) {
    $value = 0
    [int]::TryParse((Get-XmlAttribute $node $name), [ref]$value) | Out-Null
    return $value
}

function Get-CaseStatus($case) {
    if ($case.failure) { return "FAIL" }
    if ($case.error) { return "ERROR" }
    if ($case.skipped) { return "SKIP" }
    return "PASS"
}

function Get-CaseMessage($case) {
    if ($case.failure) { return Get-XmlAttribute $case.failure "message" }
    if ($case.error) { return Get-XmlAttribute $case.error "message" }
    if ($case.skipped) { return Get-XmlAttribute $case.skipped "message" }
    return ""
}

function Get-JUnitCases([string]$JUnitPath) {
    if (-not (Test-Path $JUnitPath)) { return @() }
    [xml]$junit = Get-Content -Path $JUnitPath -Raw
    return @($junit.SelectNodes("/testsuite/testcase"))
}

# Collect the per-segment branch-trajectory files a case emitted under
# PREVABS_SEGMENT_PATH=1. Returns objects { segment; steps }, one per segment.
function Get-CaseBuildPaths([string]$CaseName) {
    $parts = $CaseName -split "/", 2
    if ($parts.Count -ne 2) { return @() }
    $caseDir  = Join-Path $integrationDir $parts[0]
    $caseBase = $parts[1]
    if (-not (Test-Path $caseDir)) { return @() }

    $files = @(Get-ChildItem -Path $caseDir `
        -Filter "$caseBase.*.segment.path.txt" -ErrorAction SilentlyContinue |
        Sort-Object Name)

    $result = @()
    foreach ($f in $files) {
        # filename: <caseBase>.<segment>.segment.path.txt
        $seg = $f.Name
        if ($seg.StartsWith("$caseBase.")) { $seg = $seg.Substring($caseBase.Length + 1) }
        $seg = $seg -replace "\.segment\.path\.txt$", ""
        # Drop the leading "# build path ..." header comment(s).
        $steps = @(Get-Content $f.FullName | Where-Object { $_ -notmatch "^\s*#" })
        $result += [pscustomobject]@{ segment = $seg; steps = $steps }
    }
    return $result
}

function Write-IntegrationReport(
    [string]$JUnitPath,
    [string]$MarkdownPath,
    [string]$Filter,
    [int]$CTestExitCode
) {
    if (-not (Test-Path $JUnitPath)) {
        Write-Warning "CTest did not write $JUnitPath; report not generated."
        return
    }

    [xml]$junit = Get-Content -Path $JUnitPath -Raw
    $suite = $junit.DocumentElement
    $cases = @(Get-JUnitCases $JUnitPath)
    $tests = Get-XmlIntAttribute $suite "tests"
    $failures = Get-XmlIntAttribute $suite "failures"
    $errors = Get-XmlIntAttribute $suite "errors"
    $skipped = Get-XmlIntAttribute $suite "skipped"
    $passed = $tests - $failures - $errors - $skipped
    $duration = Format-Duration (Get-XmlAttribute $suite "time")
    $generatedAt = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss zzz")
    $filterText = if ($Filter) { $Filter } else { "(none)" }

    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add("# PreVABS Integration Test Report")
    $lines.Add("")
    $lines.Add("- Generated: $generatedAt")
    $lines.Add("- Filter: ``$filterText``")
    $lines.Add("- CTest exit code: $CTestExitCode")
    $lines.Add("- JUnit XML: ``$JUnitPath``")
    $lines.Add("")
    $lines.Add("## Summary")
    $lines.Add("")
    $lines.Add("| Total | Passed | Failed | Errors | Skipped | Time |")
    $lines.Add("| ---: | ---: | ---: | ---: | ---: | ---: |")
    $lines.Add("| $tests | $passed | $failures | $errors | $skipped | $duration |")

    $problemCases = @($cases | Where-Object {
        (Get-CaseStatus $_) -in @("FAIL", "ERROR", "SKIP")
    })
    if ($problemCases.Count -gt 0) {
        $lines.Add("")
        $lines.Add("## Problems")
        $lines.Add("")
        $lines.Add("| Status | Test | Time | Message |")
        $lines.Add("| --- | --- | ---: | --- |")
        foreach ($case in $problemCases) {
            $status = Get-CaseStatus $case
            $name = Get-XmlAttribute $case "name"
            $time = Format-Duration (Get-XmlAttribute $case "time")
            $message = (Get-CaseMessage $case) -replace "\r?\n", " "
            $message = $message -replace "\|", "\|"
            $lines.Add("| $status | ``$name`` | $time | $message |")
        }
    }

    $lines.Add("")
    $lines.Add("## Cases")
    $lines.Add("")
    $lines.Add("| Status | Test | Time |")
    $lines.Add("| --- | --- | ---: |")
    foreach ($case in $cases) {
        $status = Get-CaseStatus $case
        $name = Get-XmlAttribute $case "name"
        $time = Format-Duration (Get-XmlAttribute $case "time")
        $lines.Add("| $status | ``$name`` | $time |")
    }

    # Build paths: which conditional branch each laminate segment took through
    # buildAreas (emitted per segment under PREVABS_SEGMENT_PATH=1). Only cases
    # that actually built a laminate segment appear here.
    $pathCases = @()
    foreach ($case in $cases) {
        $name = Get-XmlAttribute $case "name"
        if (-not $name -or $name.EndsWith(".logcheck")) { continue }
        $segs = @(Get-CaseBuildPaths $name)
        if ($segs.Count -gt 0) {
            $pathCases += [pscustomobject]@{ name = $name; segments = $segs }
        }
    }
    if ($pathCases.Count -gt 0) {
        $lines.Add("")
        $lines.Add("## Build paths")
        $lines.Add("")
        $lines.Add("Call-stack trajectory each laminate segment took while its geometry was built — offset phase (``offsetCurveBase`` -> ``buildBaseOffsetMap`` -> ...) then area phase (``buildAreas`` -> ``buildLayeredOffsetAreas`` -> ...). Indentation = call depth. Per-segment file: ``<case>.<segment>.segment.path.txt``.")
        foreach ($pc in $pathCases) {
            $lines.Add("")
            $lines.Add("### ``$($pc.name)``")
            foreach ($seg in $pc.segments) {
                $lines.Add("")
                $lines.Add("**$($seg.segment)**")
                $lines.Add('```')
                if ($seg.steps.Count -eq 0) {
                    $lines.Add("(no steps recorded)")
                } else {
                    foreach ($step in $seg.steps) { $lines.Add($step.TrimEnd()) }
                }
                $lines.Add('```')
            }
        }
    }

    Set-Content -Path $MarkdownPath -Value $lines -Encoding utf8
    Write-Host "Integration report: $MarkdownPath"
}

function Open-GmshCases([string]$JUnitPath, [string]$GmshPath) {
    $cases = @(Get-JUnitCases $JUnitPath)
    if ($cases.Count -eq 0) {
        Write-Warning "No executed test cases found for Gmsh visualization."
        return
    }

    $opened = 0
    foreach ($case in $cases) {
        $name = Get-XmlAttribute $case "name"
        if (-not $name -or $name.EndsWith(".logcheck")) { continue }

        $parts = $name -split "/", 2
        if ($parts.Count -ne 2) {
            Write-Warning "Skipping '$name': expected test name '<dir>/<case>'."
            continue
        }

        $caseDir = Join-Path $integrationDir $parts[0]
        $caseBase = $parts[1]
        $geo = Join-Path $caseDir "$caseBase.geo_unrolled"
        $msh = Join-Path $caseDir "$caseBase.msh"
        $opt = Join-Path $caseDir "$caseBase.opt"
        $visualFiles = @($geo, $msh, $opt)
        $missing = @($visualFiles | Where-Object { -not (Test-Path $_) })
        if ($missing.Count -gt 0) {
            Write-Warning "Skipping '$name': missing $($missing -join ', ')."
            continue
        }

        Write-Host "Opening Gmsh: gmsh $caseBase.geo_unrolled $caseBase.msh $caseBase.opt"
        Start-Process `
            -FilePath $GmshPath `
            -ArgumentList @("$caseBase.geo_unrolled", "$caseBase.msh", "$caseBase.opt") `
            -WorkingDirectory $caseDir
        $opened += 1
    }

    if ($opened -eq 0) {
        Write-Warning "No Gmsh visualizations were opened."
    }
}

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
$prevabsArgs = "-v;--nopopup"
$cmakeArgs = @(
    "-S", $integrationDir,
    "-B", $buildDir,
    "-DPREVABS:FILEPATH=$prevabs",
    "-DPREVABS_ARGS:STRING=$prevabsArgs"
)
$ninja = Find-Tool "ninja"
if ($ninja) { $cmakeArgs += @("-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=$ninja") }
& $cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$ctestArgs = @(
    "--test-dir", $buildDir,
    "--output-on-failure",
    "--output-junit", $junitReport
)
if (-not $ninja) { $ctestArgs += @("-C", "Release") }
if ($Filter) { $ctestArgs += @("-R", $Filter) }
& $ctest @ctestArgs
$ctestExitCode = $LASTEXITCODE
Write-IntegrationReport $junitReport $markdownReport $Filter $ctestExitCode
if ($OpenGmsh) {
    $gmsh = Find-Gmsh
    if (-not $gmsh) {
        Write-Warning "gmsh not found. Add it to PATH or set Gmsh_ROOT."
    } else {
        Open-GmshCases $junitReport $gmsh
    }
}
exit $ctestExitCode
