#requires -Version 7
<#
.SYNOPSIS
  Scan the Gel_Coat lamina thickness for the airfoil skin-only case.

.DESCRIPTION
  Generates one isolated case per thickness, runs PreVABS, and writes CSV and
  Markdown reports under test/param/build_msvc/t1_airfoil_thickness.

  A case passes only when PreVABS exits with code 0 and creates a non-empty
  airfoil_skin_only.sg file. The script exits with code 1 when any case fails.

.EXAMPLE
  pwsh -NoProfile -File test/param/t1_airfoil/run-thickness-scan.ps1

.EXAMPLE
  pwsh -NoProfile -File test/param/t1_airfoil/run-thickness-scan.ps1 `
    -MinThickness 0.001 -MaxThickness 0.1 -Step 0.001
#>
param(
  [decimal]$MinThickness = 0.001,
  [decimal]$MaxThickness = 0.1,
  [decimal]$Step = 0.001,
  [ValidateRange(1, 3600)]
  [int]$TimeoutSec = 15
)

$ErrorActionPreference = "Stop"
$invariant = [System.Globalization.CultureInfo]::InvariantCulture
$caseDir = $PSScriptRoot
$repoRoot = Resolve-Path (Join-Path $caseDir "..\..\..")
$templatePath = Join-Path $caseDir "airfoil_skin_only.xml"
$prevabs = Join-Path $repoRoot "build_msvc\Release\prevabs.exe"
$buildRoot = Join-Path $repoRoot "test\param\build_msvc"
$outputRoot = Join-Path $buildRoot "t1_airfoil_thickness"
$casesRoot = Join-Path $outputRoot "cases"
$csvPath = Join-Path $outputRoot "thickness-scan.csv"
$reportPath = Join-Path $outputRoot "thickness-scan.md"

if ($MinThickness -le 0) {
  throw "MinThickness must be greater than zero."
}
if ($MaxThickness -lt $MinThickness) {
  throw "MaxThickness must be greater than or equal to MinThickness."
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

function Format-Thickness([decimal]$Value) {
  return $Value.ToString("0.############################", $invariant)
}

function Get-Thicknesses {
  $values = [System.Collections.Generic.List[decimal]]::new()
  $value = $MinThickness
  while ($value -le $MaxThickness) {
    $values.Add($value)
    $value += $Step
  }
  if ($values.Count -eq 0 -or $values[$values.Count - 1] -ne $MaxThickness) {
    $values.Add($MaxThickness)
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

function New-Case([decimal]$Thickness) {
  $thicknessText = Format-Thickness $Thickness
  $workDir = Join-Path $casesRoot $thicknessText
  New-Item -ItemType Directory -Path $workDir -Force | Out-Null

  [xml]$xml = Get-Content -Path $templatePath -Raw
  $thicknessNode = $xml.SelectSingleNode(
    "/cross_section/materials/lamina[@name='Gel_Coat']/thickness")
  if (-not $thicknessNode) {
    throw "Gel_Coat thickness node not found in $templatePath"
  }
  $thicknessNode.InnerText = $thicknessText

  foreach ($pointsNode in $xml.SelectNodes("//points[@data='file']")) {
    $relativePath = $pointsNode.InnerText.Trim()
    $sourcePath = Join-Path $caseDir $relativePath
    if (-not (Test-Path $sourcePath -PathType Leaf)) {
      throw "referenced points file not found: $sourcePath"
    }
    $targetPath = Join-Path $workDir $relativePath
    $targetDir = Split-Path -Parent $targetPath
    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $targetPath
  }

  $inputPath = Join-Path $workDir "airfoil_skin_only.xml"
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
    thickness = $thicknessText
    workDir = $workDir
    inputPath = $inputPath
  }
}

function Invoke-Case($Case) {
  $psi = [System.Diagnostics.ProcessStartInfo]::new()
  $psi.FileName = $prevabs
  foreach ($arg in @(
      "-i", "airfoil_skin_only.xml", "--hm", "--nopopup")) {
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
    "Thickness: $($Case.thickness)",
    "Command: prevabs -i airfoil_skin_only.xml --hm --nopopup",
    "Exit code: $exitCode",
    "Elapsed: $($stopwatch.Elapsed.TotalSeconds.ToString('0.###', $invariant))s",
    "",
    "--- stdout ---",
    $stdout,
    "--- stderr ---",
    $stderr
  )
  Set-Content -Path $logPath -Value $logLines -Encoding utf8

  $sgPath = Join-Path $Case.workDir "airfoil_skin_only.sg"
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
    thickness = $Case.thickness
    status = $status
    exit_code = $exitCode
    seconds = [math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
    message = $message
    log = "cases/$($Case.thickness)/runner.log"
  }
}

function Format-MarkdownCell([string]$Text) {
  if (-not $Text) { return "" }
  $value = (Remove-Ansi $Text) -replace "\r?\n", " "
  return ($value -replace '\|', '\|').Trim()
}

$results = @()
foreach ($thickness in Get-Thicknesses) {
  $text = Format-Thickness $thickness
  Write-Host -NoNewline "Thickness $text ... "
  $case = New-Case $thickness
  $result = Invoke-Case $case
  $results += $result
  Write-Host "$($result.status) ($($result.seconds)s)"
}

$results | Export-Csv -Path $csvPath -NoTypeInformation -Encoding utf8
$failures = @($results | Where-Object status -ne "PASS")
$firstFailure = $failures | Select-Object -First 1
$lastPassBeforeFailure = $null
$recovered = @()
if ($firstFailure) {
  $firstFailureValue = [decimal]::Parse($firstFailure.thickness, $invariant)
  $lastPassBeforeFailure = $results |
    Where-Object {
      $_.status -eq "PASS" -and
      [decimal]::Parse($_.thickness, $invariant) -lt $firstFailureValue
    } |
    Select-Object -Last 1
  $recovered = @($results | Where-Object {
    $_.status -eq "PASS" -and
    [decimal]::Parse($_.thickness, $invariant) -gt $firstFailureValue
  })
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Airfoil thickness scan")
$lines.Add("")
$lines.Add("- Generated: $((Get-Date).ToString('yyyy-MM-dd HH:mm:ss zzz'))")
$lines.Add("- Range: ``$(Format-Thickness $MinThickness)`` to " +
  "``$(Format-Thickness $MaxThickness)``")
$lines.Add("- Step: ``$(Format-Thickness $Step)``")
$lines.Add("- Command: ``prevabs -i airfoil_skin_only.xml --hm --nopopup``")
$lines.Add("- Pass criterion: exit code 0 and a non-empty ``.sg`` file")
$lines.Add("")
$lines.Add("## Result")
$lines.Add("")
if ($firstFailure) {
  $previousText = if ($lastPassBeforeFailure) {
    "The preceding passing grid point is ``$($lastPassBeforeFailure.thickness)``."
  } else {
    "No lower grid point passed."
  }
  $lines.Add("The first failing grid point is ``$($firstFailure.thickness)``. " +
    $previousText)
  $lines.Add("")
  $lines.Add("Failure: $(Format-MarkdownCell $firstFailure.message)")
  if ($recovered.Count -gt 0) {
    $lines.Add("")
    $lines.Add("Non-monotonic result: $($recovered.Count) later grid point(s) " +
      "passed after the first failure.")
  }
} else {
  $lines.Add("All $($results.Count) grid points passed.")
}
$lines.Add("")
$lines.Add("## Cases")
$lines.Add("")
$lines.Add("| Status | Thickness | Exit | Time | Message | Log |")
$lines.Add("| --- | ---: | ---: | ---: | --- | --- |")
foreach ($result in $results) {
  $message = Format-MarkdownCell $result.message
  $lines.Add("| $($result.status) | $($result.thickness) | " +
    "$($result.exit_code) | $($result.seconds)s | $message | " +
    "[log]($($result.log)) |")
}
Set-Content -Path $reportPath -Value $lines -Encoding utf8

Write-Host ""
if ($firstFailure) {
  Write-Host "First failure: thickness $($firstFailure.thickness)"
  if ($lastPassBeforeFailure) {
    Write-Host "Previous pass: thickness $($lastPassBeforeFailure.thickness)"
  }
  if ($recovered.Count -gt 0) {
    Write-Host "Warning: later passing points make the result non-monotonic."
  }
} else {
  Write-Host "All $($results.Count) thicknesses passed."
}
Write-Host "Report: $reportPath"

exit ([int]($failures.Count -gt 0))
