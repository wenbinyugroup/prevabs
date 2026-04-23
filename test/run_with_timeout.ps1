# Run prevabs with a hard timeout.
# Exit codes: 0 = success, 1-123 = process failure, 124 = timed out.
param(
    [string]$Exe,
    [string]$InputXml,
    [string]$LogFile,
    [int]$TimeoutSec = 15
)

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName        = $Exe
$psi.Arguments       = "-i `"$InputXml`" --hm"
$psi.UseShellExecute        = $false
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError  = $true
$psi.CreateNoWindow         = $true

$p = New-Object System.Diagnostics.Process
$p.StartInfo = $psi
$p.Start() | Out-Null

$outTask = $p.StandardOutput.ReadToEndAsync()
$errTask = $p.StandardError.ReadToEndAsync()

$timedOut = -not $p.WaitForExit($TimeoutSec * 1000)
if ($timedOut) {
    $p.Kill()
    $p.WaitForExit()
}

[System.IO.File]::WriteAllText($LogFile, $outTask.Result + $errTask.Result)

if ($timedOut) { exit 124 } else { exit $p.ExitCode }
