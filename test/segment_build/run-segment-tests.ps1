#requires -Version 7
<#
.SYNOPSIS
  Run the "build a single laminate segment" test set and produce a report.

.DESCRIPTION
  For every case <name>.xml in this directory (materials.xml excluded), runs
  prevabs with the layered-offset path and the per-segment inspection dump
  enabled:

      PREVABS_LAYERED_OFFSET=1  PREVABS_SEGMENT_DUMP=1  prevabs -i <name>.xml --hm

  Each run emits, per laminate segment:
    <name>.<seg>.segment.svg    human-readable (base/offset/tiled faces, flags)
    <name>.<seg>.segment.json   machine-readable quality metrics
  plus the usual pipeline artifacts (<name>.msh gmsh mesh, <name>.dcel_dump.txt).

  The script aggregates every segment JSON into:
    report.md     human-readable table + per-segment status
    report.csv    machine-readable, one row per segment

  Status per segment:
    FAIL  run crashed / timed out, or any inverted or degenerate face
    WARN  sliver face(s), or an open segment that fell back to legacy
    PASS  otherwise

.PARAMETER Filter
  Only run cases whose name matches this substring.

.PARAMETER TimeoutSec
  Per-case wall-clock timeout (default 30s). A timeout is a FAIL.

.EXAMPLE
  pwsh -NoProfile -File run-segment-tests.ps1
  pwsh -NoProfile -File run-segment-tests.ps1 -Filter arc
#>
param(
  [string]$Filter = "",
  [int]$TimeoutSec = 30
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path $here "..\..\build_msvc\Release\prevabs.exe"
if (-not (Test-Path $exe)) {
  Write-Error "prevabs.exe not found at $exe — build first (tools\build-msvc.ps1 fast)."
}

# Collect cases.
$cases = Get-ChildItem -Path $here -Filter *.xml |
  Where-Object { $_.BaseName -ne "materials" } |
  Where-Object { $Filter -eq "" -or $_.BaseName -like "*$Filter*" } |
  Sort-Object BaseName

if ($cases.Count -eq 0) { Write-Error "no cases match filter '$Filter'." }

# Clean stale dumps for the cases we are about to run.
foreach ($c in $cases) {
  Get-ChildItem -Path $here -Filter "$($c.BaseName).*.segment.*" -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
}

# runResult keyed by case name -> @{ exit; timedout; seconds }
$runResults = @{}

foreach ($c in $cases) {
  Write-Host ("==> {0}" -f $c.BaseName) -ForegroundColor Cyan
  $env:PREVABS_LAYERED_OFFSET = "1"
  $env:PREVABS_SEGMENT_DUMP = "1"
  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  $p = Start-Process -FilePath $exe -ArgumentList @("-i", $c.Name, "--hm") `
    -WorkingDirectory $here -PassThru -WindowStyle Hidden `
    -RedirectStandardOutput (Join-Path $here "$($c.BaseName).run.out") `
    -RedirectStandardError  (Join-Path $here "$($c.BaseName).run.err")
  $timedout = $false
  if (-not $p.WaitForExit($TimeoutSec * 1000)) {
    $timedout = $true
    try { $p.Kill($true) } catch {}
    $p.WaitForExit()
  }
  $sw.Stop()
  $runResults[$c.BaseName] = @{
    exit     = if ($timedout) { -1 } else { $p.ExitCode }
    timedout = $timedout
    seconds  = [math]::Round($sw.Elapsed.TotalSeconds, 2)
  }
  $tag = if ($timedout) { "TIMEOUT" } elseif ($p.ExitCode -ne 0) { "exit=$($p.ExitCode)" } else { "ok" }
  Write-Host ("    {0}  {1}s" -f $tag, $runResults[$c.BaseName].seconds)
}

# Aggregate every segment JSON into rows.
$rows = @()
foreach ($c in $cases) {
  $run = $runResults[$c.BaseName]
  $jsons = Get-ChildItem -Path $here -Filter "$($c.BaseName).*.segment.json" -ErrorAction SilentlyContinue

  if ($run.timedout -or $run.exit -ne 0 -or $jsons.Count -eq 0) {
    # Crash / timeout / no dump: a single synthetic FAIL row for the case.
    $reason = if ($run.timedout) { "timeout" }
              elseif ($run.exit -ne 0) { "exit=$($run.exit)" }
              else { "no segment dump (build did not reach buildAreas)" }
    $rows += [pscustomobject]@{
      case = $c.BaseName; segment = "-"; route = "-"; closed = "-"
      base = 0; layers = 0; faces = 0; inverted = 0; sliver = 0; degenerate = 0
      seconds = $run.seconds; status = "FAIL"; notes = $reason; path = @()
    }
    continue
  }

  foreach ($jf in ($jsons | Sort-Object Name)) {
    $d = Get-Content $jf.FullName -Raw | ConvertFrom-Json
    $status = "PASS"; $notes = @()
    if ($d.n_inverted -gt 0)   { $status = "FAIL"; $notes += "$($d.n_inverted) inverted" }
    if ($d.n_degenerate -gt 0) { $status = "FAIL"; $notes += "$($d.n_degenerate) degenerate" }
    if ($status -ne "FAIL") {
      if ($d.n_sliver -gt 0) { $status = "WARN"; $notes += "$($d.n_sliver) sliver" }
      if (-not $d.closed -and $d.route -ne "layered") {
        $status = "WARN"; $notes += "open fell back to $($d.route)"
      }
    }
    $rows += [pscustomobject]@{
      case = $c.BaseName; segment = $d.segment; route = $d.route
      closed = $d.closed; base = $d.base_vertices; layers = $d.layers
      faces = $d.face_count; inverted = $d.n_inverted; sliver = $d.n_sliver
      degenerate = $d.n_degenerate; seconds = $run.seconds; status = $status
      notes = ($notes -join "; "); path = @($d.path)
    }
    # Echo the build path to the console for immediate inspection.
    Write-Host ("    [{0}] path:" -f $d.segment) -ForegroundColor DarkGray
    foreach ($step in $d.path) { Write-Host ("      - {0}" -f $step) -ForegroundColor DarkGray }
  }
}

# CSV (machine-readable).
$csvPath = Join-Path $here "report.csv"
$rows | Select-Object case, segment, route, closed, base, layers, faces,
  inverted, sliver, degenerate, seconds, status, notes |
  Export-Csv -Path $csvPath -NoTypeInformation -Encoding UTF8

# Markdown (human-readable).
$nPass = ($rows | Where-Object status -eq "PASS").Count
$nWarn = ($rows | Where-Object status -eq "WARN").Count
$nFail = ($rows | Where-Object status -eq "FAIL").Count
$md = New-Object System.Text.StringBuilder
[void]$md.AppendLine("# Segment build report")
[void]$md.AppendLine("")
[void]$md.AppendLine("Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
[void]$md.AppendLine("")
[void]$md.AppendLine("Segments: **$($rows.Count)**  —  PASS **$nPass** / WARN **$nWarn** / FAIL **$nFail**")
[void]$md.AppendLine("")
[void]$md.AppendLine("| status | case | segment | route | closed | base | layers | faces | inv | sliver | degen | sec | notes |")
[void]$md.AppendLine("|---|---|---|---|---|---|---|---|---|---|---|---|---|")
foreach ($r in $rows) {
  $mark = switch ($r.status) { "PASS" { "✅" } "WARN" { "⚠️" } "FAIL" { "❌" } }
  [void]$md.AppendLine("| $mark $($r.status) | $($r.case) | $($r.segment) | $($r.route) | $($r.closed) | $($r.base) | $($r.layers) | $($r.faces) | $($r.inverted) | $($r.sliver) | $($r.degenerate) | $($r.seconds) | $($r.notes) |")
}
[void]$md.AppendLine("")
[void]$md.AppendLine("Per-segment visuals: ``<case>.<segment>.segment.svg``  ·  gmsh mesh: ``<case>.msh``")
[void]$md.AppendLine("")
[void]$md.AppendLine("## Build paths")
[void]$md.AppendLine("")
[void]$md.AppendLine("Which conditional branch each segment actually took (full per-segment file: ``<case>.<segment>.segment.path.txt``).")
foreach ($r in $rows) {
  [void]$md.AppendLine("")
  [void]$md.AppendLine("### $($r.case) / $($r.segment) — $($r.status)")
  [void]$md.AppendLine('```')
  if ($r.path.Count -eq 0) {
    [void]$md.AppendLine("(no path: $($r.notes))")
  } else {
    $i = 1
    foreach ($step in $r.path) { [void]$md.AppendLine(("{0}. {1}" -f $i, $step)); $i++ }
  }
  [void]$md.AppendLine('```')
}
$mdPath = Join-Path $here "report.md"
[System.IO.File]::WriteAllText($mdPath, $md.ToString())

# Console summary.
Write-Host ""
$rows | Format-Table case, segment, route, base, layers, faces, inverted, sliver, status -AutoSize
Write-Host ""
Write-Host ("PASS {0} / WARN {1} / FAIL {2}   ->  {3}" -f $nPass, $nWarn, $nFail, $mdPath) -ForegroundColor Yellow

if ($nFail -gt 0) { exit 1 } else { exit 0 }
