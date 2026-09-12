<#
.SYNOPSIS
    Prints a short pass/fail summary of an Unreal automation run.

.DESCRIPTION
    Run_SourceMovement_Tests.bat prints the full engine log, which for 57 tests is thousands of
    lines -- the per-test "Test Completed. Result={...}" lines scroll out of the console buffer long
    before the run ends. This reads the JSON report the run already exports and prints only what
    matters: the counts, and the name plus error text of anything that failed.

.PARAMETER ReportPath
    Path to index.json, as exported by -ReportExportPath.
#>
param(
    [Parameter(Mandatory = $true)]
    [string] $ReportPath
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $ReportPath)) {
    Write-Host " No report at $ReportPath" -ForegroundColor Yellow
    Write-Host " The run probably crashed before any test executed." -ForegroundColor Yellow
    exit 0
}

try {
    $report = Get-Content -Raw -LiteralPath $ReportPath | ConvertFrom-Json
}
catch {
    Write-Host " Could not parse $ReportPath : $($_.Exception.Message)" -ForegroundColor Yellow
    exit 0
}

$passed   = [int] $report.succeeded
$warned   = [int] $report.succeededWithWarnings
$failed   = [int] $report.failed
$notRun   = [int] $report.notRun
$duration = [double] $report.totalDuration

$line = '=' * 78
Write-Host $line

$summary = " RESULT   {0} passed" -f ($passed + $warned)
if ($warned -gt 0) { $summary += " ({0} with warnings)" -f $warned }
$summary += "   {0} failed" -f $failed
if ($notRun -gt 0) { $summary += "   {0} not run" -f $notRun }
$summary += "   [{0:N2}s]" -f $duration

if ($failed -gt 0 -or $notRun -gt 0) {
    Write-Host $summary -ForegroundColor Red
} else {
    Write-Host $summary -ForegroundColor Green
}

Write-Host $line

if ($failed -gt 0) {
    Write-Host ""
    Write-Host " FAILED:" -ForegroundColor Red

    foreach ($test in $report.tests) {
        if ($test.state -eq 'Success') { continue }

        Write-Host ("   {0}" -f $test.fullTestPath) -ForegroundColor Red

        foreach ($entry in $test.entries) {
            # Report schema nests the message under .event; older exports put it at the top level.
            $type = if ($entry.event) { $entry.event.type } else { $entry.type }
            if ($type -ne 'Error') { continue }

            $message = if ($entry.event) { $entry.event.message } else { $entry.message }
            Write-Host ("     {0}" -f $message)
        }
    }

    Write-Host ""
    Write-Host " Every expected number in these tests is derived from the Source code and explained" -ForegroundColor Yellow
    Write-Host " in the test's own comment. A failure means either the port or the derivation is" -ForegroundColor Yellow
    Write-Host " wrong -- do NOT update the expected value to match the output." -ForegroundColor Yellow
    Write-Host ""
}
