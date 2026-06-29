#requires -Version 7
<#
.SYNOPSIS
  Run the examples in this directory and write a Markdown report.

.DESCRIPTION
  Each example directory is discovered through its meta.json. If meta.json
  defines run_command, that command is used. Otherwise the first XML file in
  inputs is run with:

      prevabs -i <input.xml> --hm

  --nopopup is always added so visualization examples do not open Gmsh. Full
  stdout/stderr for every example is saved next to the report. The script exits
  with code 1 when any example fails, times out, or has invalid metadata.

  With -e, every example executes VABS. A case passes only when VABS reports
  successful completion and writes a fresh mode-specific result file.

.PARAMETER Filter
  Regex matched against the example directory name, for example "airfoil" or
  "ex_box$".

.PARAMETER TimeoutSec
  Per-example timeout in seconds (default: 60).

.PARAMETER ExecuteVabs
  Execute VABS for every example and verify its output. Alias: -e.

.PARAMETER ReportPath
  Markdown report path. Defaults to examples-report.md in this directory.

.EXAMPLE
  pwsh -NoProfile -File share\examples\run-examples.ps1

.EXAMPLE
  pwsh -NoProfile -File share\examples\run-examples.ps1 -Filter "ex_box$"

.EXAMPLE
  pwsh -NoProfile -File share\examples\run-examples.ps1 -e
#>
param(
  [string]$Filter = "",
  [ValidateRange(1, 86400)]
  [int]$TimeoutSec = 60,
  [Alias("e")]
  [switch]$ExecuteVabs,
  [string]$ReportPath = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$examplesDir = $PSScriptRoot
$prevabs = Join-Path $repoRoot "build_msvc\Release\prevabs.exe"

if (-not $ReportPath) {
  $ReportPath = Join-Path $examplesDir "examples-report.md"
} elseif (-not [System.IO.Path]::IsPathRooted($ReportPath)) {
  $ReportPath = Join-Path $examplesDir $ReportPath
}
$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)
$reportDir = Split-Path -Parent $ReportPath
$logsDir = Join-Path $reportDir "example-logs"

if (-not (Test-Path $prevabs)) {
  Write-Error "prevabs.exe not found at $prevabs`nBuild the project first."
  exit 1
}

New-Item -ItemType Directory -Force $reportDir | Out-Null
New-Item -ItemType Directory -Force $logsDir | Out-Null

function ConvertFrom-RunCommand([string]$Command) {
  $parseErrors = $null
  $tokens = @([System.Management.Automation.PSParser]::Tokenize(
    $Command, [ref]$parseErrors))
  if ($parseErrors.Count -gt 0) {
    throw "invalid run_command: $($parseErrors[0].Message)"
  }

  $allowedTypes = @("Command", "CommandParameter", "CommandArgument",
                    "String", "Number")
  $unsupported = @($tokens | Where-Object {
    $_.Type.ToString() -notin $allowedTypes
  })
  if ($unsupported.Count -gt 0) {
    throw "run_command contains unsupported syntax: $($unsupported[0].Content)"
  }

  $parts = @($tokens | ForEach-Object { $_.Content })
  if ($parts.Count -eq 0) {
    throw "run_command is empty"
  }
  $commandName = [System.IO.Path]::GetFileName($parts[0])
  if ($commandName -notin @("prevabs", "prevabs.exe")) {
    throw "run_command must invoke prevabs, got '$($parts[0])'"
  }
  return @($parts | Select-Object -Skip 1)
}

function Get-VabsResultPath($Arguments, [string]$WorkingDirectory) {
  $inputIndex = -1
  for ($i = 0; $i -lt $Arguments.Count; $i++) {
    if ($Arguments[$i] -in @("-i", "--input")) {
      $inputIndex = $i
      break
    }
  }
  if ($inputIndex -lt 0 -or $inputIndex + 1 -ge $Arguments.Count) {
    throw "cannot determine the main input from the example command"
  }

  $inputBase = [System.IO.Path]::GetFileNameWithoutExtension(
    [string]$Arguments[$inputIndex + 1])
  $resultSuffix = if ($Arguments -contains "--hm" -or
                      $Arguments -contains "--homogenization") {
    ".sg.K"
  } elseif ($Arguments -contains "--dh" -or
            $Arguments -contains "--dehomogenization") {
    ".sg.ELE"
  } elseif ($Arguments -contains "--fi" -or
            $Arguments -contains "--failure-index") {
    ".sg.fi"
  } else {
    throw "VABS output verification does not support this analysis mode"
  }
  return Join-Path $WorkingDirectory "$inputBase$resultSuffix"
}

function Get-Examples([string]$NameFilter) {
  $cases = @()
  $dirs = @(Get-ChildItem -Path $examplesDir -Directory | Sort-Object Name)
  foreach ($dir in $dirs) {
    if ($NameFilter -and $dir.Name -notmatch $NameFilter) { continue }

    $metaPath = Join-Path $dir.FullName "meta.json"
    if (-not (Test-Path $metaPath)) { continue }

    $meta = $null
    $setupError = ""
    $order = [int]::MaxValue
    $args = @()
    $command = ""
    $vabsOutput = ""
    try {
      $meta = Get-Content -Path $metaPath -Raw | ConvertFrom-Json
      if ($null -ne $meta.order) { $order = [int]$meta.order }

      $inputs = @($meta.inputs)
      foreach ($inputFile in $inputs) {
        $inputPath = Join-Path $dir.FullName $inputFile
        if (-not (Test-Path $inputPath -PathType Leaf)) {
          throw "input listed in meta.json does not exist: $inputFile"
        }
      }

      if ($meta.run_command) {
        $args = @(ConvertFrom-RunCommand ([string]$meta.run_command))
      } else {
        $mainInput = $inputs | Where-Object {
          [System.IO.Path]::GetExtension([string]$_) -ieq ".xml"
        } | Select-Object -First 1
        if (-not $mainInput) {
          throw "meta.json inputs does not contain an XML input"
        }
        $args = @("-i", [string]$mainInput, "--hm")
      }

      if ($args -notcontains "--nopopup") { $args += "--nopopup" }
      if ($ExecuteVabs) {
        if ($args -contains "--sc") {
          throw "-e mode requires VABS, but the example selects SwiftComp"
        }
        if ($args -notcontains "-e" -and $args -notcontains "--execute") {
          $args += "-e"
        }
        $vabsOutput = Get-VabsResultPath $args $dir.FullName
      }
      $command = "prevabs " + ($args -join " ")
    } catch {
      $setupError = $_.Exception.Message
    }

    $cases += [pscustomobject]@{
      name = $dir.Name
      directory = $dir.FullName
      order = $order
      args = $args
      command = $command
      vabsOutput = $vabsOutput
      setupError = $setupError
    }
  }
  return @($cases | Sort-Object order, name)
}

function Remove-Ansi([string]$Text) {
  return $Text -replace "`e\[[0-9;?]*[ -/]*[@-~]", ""
}

function Get-FailureMessage([string]$Output) {
  $plain = Remove-Ansi $Output
  foreach ($pattern in @(
    "fatal exception:[^\r\n]*",
    "cannot find executable[^\r\n]*",
    "failed to launch[^\r\n]*",
    "solver timed out[^\r\n]*",
    "process exited with code[^\r\n]*",
    "Something wrong[^\r\n]*",
    "(?im)^\s*(?:Error|ERROR):[^\r\n]*"
  )) {
    $match = [regex]::Match(
      $plain, $pattern,
      [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if ($match.Success) { return $match.Value.Trim() }
  }
  return ""
}

function Get-LastOutputLine([string]$Output) {
  $lines = @((Remove-Ansi $Output) -split "\r?\n" | Where-Object {
    $_.Trim().Length -gt 0
  })
  if ($lines.Count -eq 0) { return "no error message was emitted" }
  return $lines[-1].Trim()
}

function Invoke-Example($Case, [int]$Timeout) {
  $logPath = Join-Path $logsDir "$($Case.name).log"
  if ($Case.setupError) {
    Set-Content -Path $logPath -Value $Case.setupError -Encoding utf8
    return [pscustomobject]@{
      name = $Case.name; status = "ERROR"; exitCode = "-"; seconds = 0.0
      command = $Case.command; message = $Case.setupError; logPath = $logPath
      vabsOutput = $Case.vabsOutput
    }
  }

  $vabsOutputBefore = $null
  if ($ExecuteVabs -and (Test-Path $Case.vabsOutput -PathType Leaf)) {
    $vabsOutputBefore = (Get-Item $Case.vabsOutput).LastWriteTimeUtc
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
  $stdout = ""
  $stderr = ""
  $timedOut = $false
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
      seconds = $stopwatch.Elapsed.TotalSeconds; command = $Case.command
      message = $message; logPath = $logPath
      vabsOutput = $Case.vabsOutput
    }
  } finally {
    if ($stopwatch.IsRunning) { $stopwatch.Stop() }
    $process.Dispose()
  }

  $combined = "$stdout`n$stderr"
  $failureMessage = Get-FailureMessage $combined
  if ($timedOut) {
    $status = "TIMEOUT"
    $message = "killed after ${Timeout}s"
  } elseif ($exitCode -ne 0) {
    $status = "FAIL"
    $message = if ($failureMessage) {
      $failureMessage
    } else {
      Get-LastOutputLine $combined
    }
  } elseif ($failureMessage) {
    # Solver launch/execution errors currently do not propagate to prevabs's
    # process exit code, so known failure diagnostics must also fail the case.
    $status = "FAIL"
    $message = $failureMessage
  } elseif ($ExecuteVabs) {
    $vabsReportedSuccess = $combined -match
      "(?im)^\s*VABS finished successfully\s*$"
    $vabsOutputInfo = Get-Item $Case.vabsOutput -ErrorAction SilentlyContinue
    $vabsOutputFresh = $null -ne $vabsOutputInfo -and
      ($null -eq $vabsOutputBefore -or
       $vabsOutputInfo.LastWriteTimeUtc -gt $vabsOutputBefore)

    if (-not $vabsReportedSuccess) {
      $status = "VABS-FAIL"
      $message = "VABS did not report successful completion"
    } elseif (-not $vabsOutputFresh) {
      $status = "VABS-FAIL"
      $message = "VABS result was not created or updated: " +
        [System.IO.Path]::GetFileName($Case.vabsOutput)
    } else {
      $status = "PASS"
      $message = ""
    }
  } else {
    $status = "PASS"
    $message = ""
  }

  $logLines = @(
    "Command: $($Case.command)",
    "Working directory: $($Case.directory)",
    "Exit code: $exitCode",
    "Elapsed: $($stopwatch.Elapsed.TotalSeconds.ToString('0.###'))s",
    $(if ($ExecuteVabs) {
      "VABS result: $($Case.vabsOutput)"
    }),
    "",
    "--- stdout ---",
    $stdout,
    "--- stderr ---",
    $stderr
  )
  Set-Content -Path $logPath -Value $logLines -Encoding utf8

  return [pscustomobject]@{
    name = $Case.name
    status = $status
    exitCode = $exitCode
    seconds = $stopwatch.Elapsed.TotalSeconds
    command = $Case.command
    message = $message
    logPath = $logPath
    vabsOutput = $Case.vabsOutput
  }
}

function Format-Duration([double]$Seconds) {
  return ("{0:0.###}s" -f $Seconds)
}

function Format-MarkdownCell([string]$Text) {
  if (-not $Text) { return "" }
  $value = (Remove-Ansi $Text) -replace "\r?\n", " "
  $value = $value -replace '\|', '\|'
  $value = $value.Replace('`', '&#96;')
  return $value.Trim()
}

function Write-ExamplesReport($Results, [double]$TotalSeconds) {
  $total = $Results.Count
  $passed = @($Results | Where-Object status -eq "PASS").Count
  $failed = @($Results | Where-Object {
    $_.status -in @("FAIL", "VABS-FAIL")
  }).Count
  $errors = @($Results | Where-Object status -eq "ERROR").Count
  $timeouts = @($Results | Where-Object status -eq "TIMEOUT").Count
  $filterText = if ($Filter) { $Filter } else { "(none)" }
  $modeText = if ($ExecuteVabs) { "PreVABS + VABS (-e)" } else { "PreVABS" }

  $lines = [System.Collections.Generic.List[string]]::new()
  $lines.Add("# PreVABS Examples Report")
  $lines.Add("")
  $lines.Add("- Generated: $((Get-Date).ToString('yyyy-MM-dd HH:mm:ss zzz'))")
  $lines.Add("- Executable: ``$prevabs``")
  $lines.Add("- Mode: $modeText")
  $lines.Add("- Filter: ``$filterText``")
  $lines.Add("- Per-example timeout: ${TimeoutSec}s")
  $lines.Add("- Total time: $(Format-Duration $TotalSeconds)")
  $lines.Add("")
  $lines.Add("## Summary")
  $lines.Add("")
  $lines.Add("| Total | Passed | Failed | Errors | Timeouts |")
  $lines.Add("| ---: | ---: | ---: | ---: | ---: |")
  $lines.Add("| $total | $passed | $failed | $errors | $timeouts |")
  $lines.Add("")
  $lines.Add("## Cases")
  $lines.Add("")
  $lines.Add("| Status | Example | Exit | VABS result | Time | Message | Log |")
  $lines.Add("| --- | --- | ---: | --- | ---: | --- | --- |")
  foreach ($result in $Results) {
    $message = Format-MarkdownCell $result.message
    $logName = [System.IO.Path]::GetFileName($result.logPath)
    $logLink = "example-logs/$logName"
    $vabsResult = if ($result.vabsOutput) {
      "``$([System.IO.Path]::GetFileName($result.vabsOutput))``"
    } else {
      "-"
    }
    $lines.Add("| $($result.status) | ``$($result.name)`` | " +
      "$($result.exitCode) | $vabsResult | " +
      "$(Format-Duration $result.seconds) | " +
      "$message | [log]($logLink) |")
  }

  Set-Content -Path $ReportPath -Value $lines -Encoding utf8
}

$cases = @(Get-Examples $Filter)
if ($cases.Count -eq 0) {
  Write-Error "No examples matched filter '$Filter'."
  exit 1
}

$results = @()
$totalStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
foreach ($case in $cases) {
  Write-Host -NoNewline "Example $($case.name) ... "
  $result = Invoke-Example $case $TimeoutSec
  Write-Host "$($result.status) ($(Format-Duration $result.seconds))"
  $results += $result
}
$totalStopwatch.Stop()

Write-ExamplesReport $results $totalStopwatch.Elapsed.TotalSeconds

$passed = @($results | Where-Object status -eq "PASS").Count
$problems = $results.Count - $passed
Write-Host ""
Write-Host "PASS $passed / PROBLEMS $problems"
Write-Host "Examples report: $ReportPath"

exit ([int]($problems -gt 0))
