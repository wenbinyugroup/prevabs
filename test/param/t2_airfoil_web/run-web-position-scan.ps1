#requires -Version 7
<#
.SYNOPSIS
  Scan the front-web chordwise position for the MH-104 airfoil-with-web case.

.DESCRIPTION
  Sweeps the x-coordinate of the `web_right_1` point (the foot of the vertical
  front web) from the leading edge (x = 0) toward the trailing edge (x = 1).
  Generates one isolated case per position, runs PreVABS, and writes CSV and
  Markdown reports under test/param/build_msvc/t2_airfoil_web_position.

  A case passes only when PreVABS exits with code 0 and creates a non-empty
  mh104.sg file. The script exits with code 1 when any case fails, but a scan
  whose only failures sit at the ends of the range is the expected outcome:
  the goal is to map which x-range succeeds, not to make every point pass.

.EXAMPLE
  pwsh -NoProfile -File test/param/t2_airfoil_web/run-web-position-scan.ps1

.EXAMPLE
  pwsh -NoProfile -File test/param/t2_airfoil_web/run-web-position-scan.ps1 `
    -MinX 0 -MaxX 1 -Step 0.02
#>
param(
  [decimal]$MinX = 0,
  [decimal]$MaxX = 1,
  [decimal]$Step = 0.02,
  [ValidateRange(1, 3600)]
  [int]$TimeoutSec = 15
)

$ErrorActionPreference = "Stop"
$invariant = [System.Globalization.CultureInfo]::InvariantCulture
$caseDir = $PSScriptRoot
$repoRoot = Resolve-Path (Join-Path $caseDir "..\..\..")
$templatePath = Join-Path $caseDir "mh104.xml"
$prevabs = Join-Path $repoRoot "build_msvc\Release\prevabs.exe"
$buildRoot = Join-Path $repoRoot "test\param\build_msvc"
$outputRoot = Join-Path $buildRoot "t2_airfoil_web_position"
$casesRoot = Join-Path $outputRoot "cases"
$csvPath = Join-Path $outputRoot "web-position-scan.csv"
$reportPath = Join-Path $outputRoot "web-position-scan.md"

if ($MaxX -lt $MinX) {
  throw "MaxX must be greater than or equal to MinX."
}
if ($Step -le 0) {
  throw "Step must be greater than zero."
}
if (-not (Test-Path $prevabs -PathType Leaf)) {
  throw "prevabs.exe not found at $prevabs. Build the project first."
}
if (-not (Test-Path $templatePath -PathType Leaf)) {
  throw "template XML not found at $templatePath"
}

# Keep recursive cleanup constrained to this runner's build directory.
$buildRootFull = [System.IO.Path]::GetFullPath($buildRoot)
$outputRootFull = [System.IO.Path]::GetFullPath($outputRoot)
$requiredPrefix = $buildRootFull.TrimEnd('\') + '\'
if (-not $outputRootFull.StartsWith(
    $requiredPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
  throw "refusing to clean output outside $buildRootFull"
}
if (Test-Path $outputRootFull) {
  Remove-Item -LiteralPath $outputRootFull -Recurse -Force
}
New-Item -ItemType Directory -Path $casesRoot -Force | Out-Null

function Format-X([decimal]$Value) {
  return $Value.ToString("0.############################", $invariant)
}

function Get-Positions {
  $values = [System.Collections.Generic.List[decimal]]::new()
  $value = $MinX
  while ($value -le $MaxX) {
    $values.Add($value)
    $value += $Step
  }
  if ($values.Count -eq 0 -or $values[$values.Count - 1] -ne $MaxX) {
    $values.Add($MaxX)
  }
  return $values
}

function Remove-Ansi([string]$Text) {
  return $Text -replace "`e\[[0-9;?]*[ -/]*[@-~]", ""
}

function Get-FailureMessage([string]$Output) {
  $plain = Remove-Ansi $Output
  foreach ($pattern in @(
    "fatal exception:[^\r\n]*",
    "(?im)^\s*(?:Error|ERROR):[^\r\n]*"
  )) {
    $match = [regex]::Match(
      $plain, $pattern,
      [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if ($match.Success) { return $match.Value.Trim() }
  }

  $lines = @($plain -split "\r?\n" | Where-Object { $_.Trim() })
  if ($lines.Count -eq 0) { return "no error message was emitted" }
  return $lines[-1].Trim()
}

function Copy-ReferencedFiles($Xml, [string]$WorkDir) {
  # Airfoil / baseline point data files.
  foreach ($pointsNode in $Xml.SelectNodes("//points[@data='file']")) {
    $relativePath = $pointsNode.InnerText.Trim()
    $sourcePath = Join-Path $caseDir $relativePath
    if (-not (Test-Path $sourcePath -PathType Leaf)) {
      throw "referenced points file not found: $sourcePath"
    }
    $targetPath = Join-Path $WorkDir $relativePath
    $targetDir = Split-Path -Parent $targetPath
    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $targetPath
  }

  # Included material databases: <include><material>NAME</material></include>
  # references NAME.xml alongside the input file.
  foreach ($matNode in $Xml.SelectNodes("//include/material")) {
    $baseName = $matNode.InnerText.Trim()
    if (-not $baseName) { continue }
    $relativePath = "$baseName.xml"
    $sourcePath = Join-Path $caseDir $relativePath
    if (-not (Test-Path $sourcePath -PathType Leaf)) {
      throw "referenced material database not found: $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath `
      -Destination (Join-Path $WorkDir $relativePath)
  }
}

function New-Case([decimal]$X) {
  $xText = Format-X $X
  $workDir = Join-Path $casesRoot $xText
  New-Item -ItemType Directory -Path $workDir -Force | Out-Null

  [xml]$xml = Get-Content -Path $templatePath -Raw
  $pointNode = $xml.SelectSingleNode(
    "/cross_section/baselines/point[@name='web_right_1']")
  if (-not $pointNode) {
    throw "web_right_1 point node not found in $templatePath"
  }
  # Move the web foot along the chord; keep it on the x2 = 0 line.
  $pointNode.InnerText = "$xText  0"

  Copy-ReferencedFiles $xml $workDir

  $inputPath = Join-Path $workDir "mh104.xml"
  $settings = [System.Xml.XmlWriterSettings]::new()
  $settings.Indent = $true
  $settings.IndentChars = "  "
  $settings.NewLineChars = "`n"
  $settings.NewLineHandling =
    [System.Xml.NewLineHandling]::Replace
  $settings.OmitXmlDeclaration = $true
  $writer = [System.Xml.XmlWriter]::Create($inputPath, $settings)
  try {
    $xml.Save($writer)
  } finally {
    $writer.Dispose()
  }

  return [pscustomobject]@{
    x = $xText
    workDir = $workDir
    inputPath = $inputPath
  }
}

function Invoke-Case($Case) {
  $psi = [System.Diagnostics.ProcessStartInfo]::new()
  $psi.FileName = $prevabs
  foreach ($arg in @(
      "-i", "mh104.xml", "--hm", "--nopopup")) {
    $psi.ArgumentList.Add($arg)
  }
  $psi.WorkingDirectory = $Case.workDir
  $psi.UseShellExecute = $false
  $psi.RedirectStandardOutput = $true
  $psi.RedirectStandardError = $true
  $psi.CreateNoWindow = $true

  $process = [System.Diagnostics.Process]::new()
  $process.StartInfo = $psi
  $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
  $stdout = ""
  $stderr = ""
  $timedOut = $false
  try {
    $process.Start() | Out-Null
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $timedOut = -not $process.WaitForExit($TimeoutSec * 1000)
    if ($timedOut) {
      try { $process.Kill($true) } catch {}
      $process.WaitForExit()
    }
    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result
    $exitCode = if ($timedOut) { 124 } else { $process.ExitCode }
  } catch {
    $exitCode = -1
    $stderr = $_.Exception.Message
  } finally {
    $stopwatch.Stop()
    $process.Dispose()
  }

  $combined = "$stdout`n$stderr"
  $logPath = Join-Path $Case.workDir "runner.log"
  $logLines = @(
    "X: $($Case.x)",
    "Command: prevabs -i mh104.xml --hm --nopopup",
    "Exit code: $exitCode",
    "Elapsed: $($stopwatch.Elapsed.TotalSeconds.ToString('0.###', $invariant))s",
    "",
    "--- stdout ---",
    $stdout,
    "--- stderr ---",
    $stderr
  )
  Set-Content -Path $logPath -Value $logLines -Encoding utf8

  $sgPath = Join-Path $Case.workDir "mh104.sg"
  $sgInfo = Get-Item $sgPath -ErrorAction SilentlyContinue
  $hasSg = $null -ne $sgInfo -and $sgInfo.Length -gt 0
  if ($timedOut) {
    $status = "TIMEOUT"
    $message = "killed after ${TimeoutSec}s"
  } elseif ($exitCode -ne 0) {
    $status = "FAIL"
    $message = Get-FailureMessage $combined
  } elseif (-not $hasSg) {
    $status = "FAIL"
    $message = "PreVABS exited 0 but did not create a non-empty .sg file"
  } else {
    $status = "PASS"
    $message = ""
  }

  return [pscustomobject]@{
    x = $Case.x
    status = $status
    exit_code = $exitCode
    seconds = [math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
    message = $message
    log = "cases/$($Case.x)/runner.log"
  }
}

function Format-MarkdownCell([string]$Text) {
  if (-not $Text) { return "" }
  $value = (Remove-Ansi $Text) -replace "\r?\n", " "
  return ($value -replace '\|', '\|').Trim()
}

# Summarise a boolean status series into contiguous [start, end] runs.
function Get-Runs($Results, [string]$Status) {
  $runs = [System.Collections.Generic.List[object]]::new()
  $start = $null
  $prev = $null
  foreach ($r in $Results) {
    if ($r.status -eq $Status) {
      if ($null -eq $start) { $start = $r.x }
      $prev = $r.x
    } elseif ($null -ne $start) {
      $runs.Add([pscustomobject]@{ from = $start; to = $prev })
      $start = $null
    }
  }
  if ($null -ne $start) {
    $runs.Add([pscustomobject]@{ from = $start; to = $prev })
  }
  return $runs
}

$results = @()
foreach ($x in Get-Positions) {
  $text = Format-X $x
  Write-Host -NoNewline "web x = $text ... "
  $case = New-Case $x
  $result = Invoke-Case $case
  $results += $result
  Write-Host "$($result.status) ($($result.seconds)s)"
}

$results | Export-Csv -Path $csvPath -NoTypeInformation -Encoding utf8

$passRuns = Get-Runs $results "PASS"
$failCount = @($results | Where-Object status -ne "PASS").Count

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Front-web chordwise position scan (MH-104)")
$lines.Add("")
$lines.Add("- Generated: $((Get-Date).ToString('yyyy-MM-dd HH:mm:ss zzz'))")
$lines.Add("- Swept parameter: ``web_right_1`` x-coordinate (chord fraction)")
$lines.Add("- Range: ``$(Format-X $MinX)`` to ``$(Format-X $MaxX)``")
$lines.Add("- Step: ``$(Format-X $Step)``")
$lines.Add("- Command: ``prevabs -i mh104.xml --hm --nopopup``")
$lines.Add("- Pass criterion: exit code 0 and a non-empty ``.sg`` file")
$lines.Add("")
$lines.Add("## Result")
$lines.Add("")
if ($passRuns.Count -eq 0) {
  $lines.Add("No grid point passed across the scanned range.")
} else {
  $lines.Add("PreVABS succeeds on the following x-range(s):")
  $lines.Add("")
  foreach ($run in $passRuns) {
    if ($run.from -eq $run.to) {
      $lines.Add("- ``x = $($run.from)``")
    } else {
      $lines.Add("- ``$($run.from) <= x <= $($run.to)``")
    }
  }
}
$lines.Add("")
$lines.Add("$($results.Count) grid points total, $failCount failing.")
$lines.Add("")
$lines.Add("## Cases")
$lines.Add("")
$lines.Add("| Status | x | Exit | Time | Message | Log |")
$lines.Add("| --- | ---: | ---: | ---: | --- | --- |")
foreach ($result in $results) {
  $message = Format-MarkdownCell $result.message
  $lines.Add("| $($result.status) | $($result.x) | " +
    "$($result.exit_code) | $($result.seconds)s | $message | " +
    "[log]($($result.log)) |")
}
Set-Content -Path $reportPath -Value $lines -Encoding utf8

Write-Host ""
if ($passRuns.Count -eq 0) {
  Write-Host "No x passed across the scanned range."
} else {
  Write-Host "Passing x-range(s):"
  foreach ($run in $passRuns) {
    if ($run.from -eq $run.to) {
      Write-Host "  x = $($run.from)"
    } else {
      Write-Host "  $($run.from) <= x <= $($run.to)"
    }
  }
}
Write-Host "Report: $reportPath"

exit ([int]($failCount -gt 0))
