#requires -Version 7
<#
.SYNOPSIS
  Batch-run every example variant in this directory and write pass/fail logs.

.DESCRIPTION
  Each variant lives in its own subdirectory with a variant.json:

      { "base": "ex_box", "input": "box_cus.xml",
        "args": ["-i", "box_cus.xml", "--hm", "--nopopup"] }

  For every variant this script runs prevabs with the given args in the
  variant's own directory, captures stdout/stderr into <variant>/run.log, and
  records the outcome. A failing (or timed-out) variant is logged and SKIPPED
  so it never blocks the variants that follow.

  A variant PASSes when prevabs exits 0 and freshly writes its SG input file
  (<input-base>.sg). When the variant's args request VABS execution (-e /
  --execute), the variant additionally must have VABS report success and freshly
  write the mode-specific result file (--hm -> <input-base>.sg.K); prevabs
  returns 0 even when VABS fails, so the result file is the real success signal.
  Otherwise it is FAIL, VABS-FAIL, TIMEOUT, or ERROR.

  A Markdown summary is written to variants-report.md in this directory.

.PARAMETER Filter
  Regex matched against the variant directory name, e.g. "airfoil" or "box_v1$".

.PARAMETER TimeoutSec
  Per-variant timeout in seconds (default: 60).

.EXAMPLE
  pwsh -NoProfile -File test\manual\example_variants\run-variants.ps1

.EXAMPLE
  test\manual\example_variants\run-variants.cmd -Filter airfoil
#>
param(
  [string]$Filter = "",
  [ValidateRange(1, 86400)]
  [int]$TimeoutSec = 60,
  [string]$ReportPath = ""
)

$ErrorActionPreference = "Stop"

$variantsDir = $PSScriptRoot
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$prevabs = Join-Path $repoRoot "build_msvc\Release\prevabs.exe"

if (-not $ReportPath) {
  $ReportPath = Join-Path $variantsDir "variants-report.md"
} elseif (-not [System.IO.Path]::IsPathRooted($ReportPath)) {
  $ReportPath = Join-Path $variantsDir $ReportPath
}
$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)

if (-not (Test-Path $prevabs)) {
  Write-Error "prevabs.exe not found at $prevabs`nBuild the project first (tools\build-msvc.cmd fast)."
  exit 1
}

function Remove-Ansi([string]$Text) {
  return $Text -replace "`e\[[0-9;?]*[ -/]*[@-~]", ""
}

function Get-LastOutputLine([string]$Output) {
  $lines = @((Remove-Ansi $Output) -split "\r?\n" | Where-Object {
    $_.Trim().Length -gt 0
  })
  if ($lines.Count -eq 0) { return "no output" }
  return $lines[-1].Trim()
}

# Load one variant's run configuration from variant.json.
function Get-Variants([string]$NameFilter) {
  $cases = @()
  $dirs = @(Get-ChildItem -Path $variantsDir -Directory | Sort-Object Name)
  foreach ($dir in $dirs) {
    if ($NameFilter -and $dir.Name -notmatch $NameFilter) { continue }

    $metaPath = Join-Path $dir.FullName "variant.json"
    if (-not (Test-Path $metaPath)) { continue }

    $setupError = ""
    $args = @()
    $input = ""
    $base = ""
    try {
      $meta = Get-Content -Path $metaPath -Raw | ConvertFrom-Json
      $input = [string]$meta.input
      $base = [string]$meta.base
      if (-not $input) { throw "variant.json has no 'input'" }
      if (-not (Test-Path (Join-Path $dir.FullName $input) -PathType Leaf)) {
        throw "input file not found: $input"
      }
      if ($meta.args) {
        $args = @($meta.args | ForEach-Object { [string]$_ })
      } else {
        $args = @("-i", $input, "--hm", "--nopopup")
      }
      if ($args -notcontains "--nopopup") { $args += "--nopopup" }
    } catch {
      $setupError = $_.Exception.Message
    }

    $executeVabs = ($args -contains "-e") -or ($args -contains "--execute")
    $inputBase = if ($input) {
      [System.IO.Path]::GetFileNameWithoutExtension($input)
    } else { "" }
    # Mode-specific VABS result file (only --hm build mode is used here).
    $vabsOutput = if ($executeVabs -and $inputBase) {
      Join-Path $dir.FullName "$inputBase.sg.K"
    } else { "" }

    $cases += [pscustomobject]@{
      name = $dir.Name
      directory = $dir.FullName
      base = $base
      input = $input
      args = $args
      command = "prevabs " + ($args -join " ")
      sgOutput = if ($inputBase) {
        Join-Path $dir.FullName "$inputBase.sg"
      } else { "" }
      executeVabs = $executeVabs
      vabsOutput = $vabsOutput
      setupError = $setupError
    }
  }
  return @($cases | Sort-Object name)
}

function Invoke-Variant($Case, [int]$Timeout) {
  $logPath = Join-Path $Case.directory "run.log"
  if ($Case.setupError) {
    Set-Content -Path $logPath -Value $Case.setupError -Encoding utf8
    return [pscustomobject]@{
      name = $Case.name; status = "ERROR"; exitCode = "-"; seconds = 0.0
      base = $Case.base; command = $Case.command
      message = $Case.setupError; logPath = $logPath
    }
  }

  $sgBefore = $null
  if (Test-Path $Case.sgOutput -PathType Leaf) {
    $sgBefore = (Get-Item $Case.sgOutput).LastWriteTimeUtc
  }
  $vabsBefore = $null
  if ($Case.executeVabs -and $Case.vabsOutput -and
      (Test-Path $Case.vabsOutput -PathType Leaf)) {
    $vabsBefore = (Get-Item $Case.vabsOutput).LastWriteTimeUtc
  }

  $psi = [System.Diagnostics.ProcessStartInfo]::new()
  $psi.FileName = $prevabs
  foreach ($arg in $Case.args) { $psi.ArgumentList.Add([string]$arg) }
  $psi.WorkingDirectory = $Case.directory
  $psi.UseShellExecute = $false
  $psi.RedirectStandardOutput = $true
  $psi.RedirectStandardError = $true
  $psi.CreateNoWindow = $true

  $process = [System.Diagnostics.Process]::new()
  $process.StartInfo = $psi
  $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
  $stdout = ""; $stderr = ""; $timedOut = $false
  try {
    $process.Start() | Out-Null
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $timedOut = -not $process.WaitForExit($Timeout * 1000)
    if ($timedOut) {
      try { $process.Kill($true) } catch {}
      $process.WaitForExit()
    }
    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result
    $exitCode = if ($timedOut) { 124 } else { $process.ExitCode }
  } catch {
    $stopwatch.Stop()
    $message = "failed to start prevabs: $($_.Exception.Message)"
    Set-Content -Path $logPath -Value $message -Encoding utf8
    return [pscustomobject]@{
      name = $Case.name; status = "ERROR"; exitCode = "-"
      seconds = $stopwatch.Elapsed.TotalSeconds; base = $Case.base
      command = $Case.command; message = $message; logPath = $logPath
    }
  } finally {
    if ($stopwatch.IsRunning) { $stopwatch.Stop() }
    $process.Dispose()
  }

  $combined = "$stdout`n$stderr"
  if ($timedOut) {
    $status = "TIMEOUT"; $message = "killed after ${Timeout}s"
  } elseif ($exitCode -ne 0) {
    $status = "FAIL"; $message = Get-LastOutputLine $combined
  } else {
    $sgInfo = Get-Item $Case.sgOutput -ErrorAction SilentlyContinue
    $sgFresh = $null -ne $sgInfo -and
      ($null -eq $sgBefore -or $sgInfo.LastWriteTimeUtc -gt $sgBefore)
    if (-not $sgFresh) {
      $status = "FAIL"
      $message = "SG file not created: " +
        [System.IO.Path]::GetFileName($Case.sgOutput)
    } elseif ($Case.executeVabs) {
      # prevabs exits 0 even when VABS fails; the fresh result file plus the
      # success banner are the real signals.
      $vabsReported = $combined -match "(?im)^\s*VABS finished successfully\s*$"
      $vabsInfo = Get-Item $Case.vabsOutput -ErrorAction SilentlyContinue
      $vabsFresh = $null -ne $vabsInfo -and
        ($null -eq $vabsBefore -or $vabsInfo.LastWriteTimeUtc -gt $vabsBefore)
      if (-not $vabsReported) {
        $status = "VABS-FAIL"
        $message = "VABS did not report successful completion"
      } elseif (-not $vabsFresh) {
        $status = "VABS-FAIL"
        $message = "VABS result not created: " +
          [System.IO.Path]::GetFileName($Case.vabsOutput)
      } else {
        $status = "PASS"; $message = ""
      }
    } else {
      $status = "PASS"; $message = ""
    }
  }

  $logLines = @(
    "Variant: $($Case.name)  (base: $($Case.base))",
    "Command: $($Case.command)",
    "Working directory: $($Case.directory)",
    "Exit code: $exitCode",
    "Status: $status",
    "Elapsed: $($stopwatch.Elapsed.TotalSeconds.ToString('0.###'))s",
    "",
    "--- stdout ---",
    $stdout,
    "--- stderr ---",
    $stderr
  )
  Set-Content -Path $logPath -Value $logLines -Encoding utf8

  return [pscustomobject]@{
    name = $Case.name; status = $status; exitCode = $exitCode
    seconds = $stopwatch.Elapsed.TotalSeconds; base = $Case.base
    command = $Case.command; message = $message; logPath = $logPath
  }
}

function Format-Duration([double]$Seconds) { return ("{0:0.###}s" -f $Seconds) }

function Format-MarkdownCell([string]$Text) {
  if (-not $Text) { return "" }
  $value = (Remove-Ansi $Text) -replace "\r?\n", " "
  $value = $value -replace '\|', '\|'
  $value = $value.Replace('`', '&#96;')
  return $value.Trim()
}

function Write-Report($Results, [double]$TotalSeconds) {
  $total = $Results.Count
  $passed = @($Results | Where-Object status -eq "PASS").Count
  $failed = @($Results | Where-Object status -eq "FAIL").Count
  $errors = @($Results | Where-Object status -eq "ERROR").Count
  $timeouts = @($Results | Where-Object status -eq "TIMEOUT").Count
  $filterText = if ($Filter) { $Filter } else { "(none)" }

  $lines = [System.Collections.Generic.List[string]]::new()
  $lines.Add("# Example Variants Report")
  $lines.Add("")
  $lines.Add("- Generated: $((Get-Date).ToString('yyyy-MM-dd HH:mm:ss zzz'))")
  $lines.Add("- Executable: ``$prevabs``")
  $lines.Add("- Filter: ``$filterText``")
  $lines.Add("- Per-variant timeout: ${TimeoutSec}s")
  $lines.Add("- Total time: $(Format-Duration $TotalSeconds)")
  $lines.Add("")
  $lines.Add("## Summary")
  $lines.Add("")
  $lines.Add("| Total | Passed | Failed | Errors | Timeouts |")
  $lines.Add("| ---: | ---: | ---: | ---: | ---: |")
  $lines.Add("| $total | $passed | $failed | $errors | $timeouts |")
  $lines.Add("")
  $lines.Add("## Variants")
  $lines.Add("")
  $lines.Add("| Status | Variant | Base | Exit | Time | Message | Log |")
  $lines.Add("| --- | --- | --- | ---: | ---: | --- | --- |")
  foreach ($r in $Results) {
    $message = Format-MarkdownCell $r.message
    $logLink = "$($r.name)/run.log"
    $lines.Add("| $($r.status) | ``$($r.name)`` | ``$($r.base)`` | " +
      "$($r.exitCode) | $(Format-Duration $r.seconds) | $message | " +
      "[log]($logLink) |")
  }
  Set-Content -Path $ReportPath -Value $lines -Encoding utf8
}

$cases = @(Get-Variants $Filter)
if ($cases.Count -eq 0) {
  Write-Error "No variants matched filter '$Filter'."
  exit 1
}

Write-Host "Running $($cases.Count) variant(s)..."
$results = @()
$totalStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
foreach ($case in $cases) {
  Write-Host -NoNewline "  $($case.name) ... "
  # Failures are recorded and skipped; they never stop the loop.
  $result = Invoke-Variant $case $TimeoutSec
  Write-Host "$($result.status) ($(Format-Duration $result.seconds))"
  $results += $result
}
$totalStopwatch.Stop()

Write-Report $results $totalStopwatch.Elapsed.TotalSeconds

$passed = @($results | Where-Object status -eq "PASS").Count
$problems = $results.Count - $passed
Write-Host ""
Write-Host "PASS $passed / PROBLEMS $problems"
Write-Host "Variants report: $ReportPath"

# Exit 0 so a CI wrapper does not treat expected variant failures as a hard
# error; inspect the report for per-variant status.
exit 0
