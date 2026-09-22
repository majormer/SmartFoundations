<#
.SYNOPSIS
    Runs Smart!'s automation tests headless and gates on the exported report.

.DESCRIPTION
    Launches UnrealEditor-Cmd with -NullRHI, runs every test under the filter, and reads the
    report's index.json. The run fails if any test failed or did not run, if the report is
    missing (e.g. the editor crashed), or if fewer tests ran than the editor discovered.
    Build the FactoryEditor target first: Live Coding never updates the on-disk DLLs, so a stale
    editor DLL would silently test old code.

.PARAMETER ProjectRoot
    CI workspace root created by New-CIWorkspace.ps1.

.PARAMETER Filter
    Automation test filter. Every Smart! test is named SmartFoundations.*.

.PARAMETER ReportDir
    Receives index.json plus the editor log.

.PARAMETER MinimumTests
    Guards against a filter or discovery problem that runs zero tests and "passes".
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ProjectRoot,
    [string]$Filter = "SmartFoundations",
    [string]$ReportDir = (Join-Path $env:TEMP "smart-ci-tests"),
    [int]$MinimumTests = 1,
    [string]$EngineRoot,
    [int]$TimeoutMinutes = 30
)

$ErrorActionPreference = "Stop"
Import-Module (Join-Path $PSScriptRoot "SmartCI.psm1") -Force

$uproject = Assert-CIWorkspace -ProjectRoot $ProjectRoot
Assert-NoCIEditorRunning -ProjectRoot $ProjectRoot
$engine = Resolve-EngineRoot -EngineRoot $EngineRoot
$editorCmd = Join-Path $engine "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
if (-not (Test-Path $editorCmd)) { throw "UnrealEditor-Cmd.exe not found under engine root '$engine'." }

if (Test-Path $ReportDir) { Remove-Item -Recurse -Force $ReportDir }
New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
$editorLog = Join-Path $ReportDir "editor.log"

# Start-Process joins an argument array without quoting, so build one explicitly quoted string.
$editorArgs = @(
    "`"$uproject`"",
    "-ExecCmds=`"Automation RunTests $Filter`"",
    "-TestExit=`"Automation Test Queue Empty`"",
    "-ReportExportPath=`"$ReportDir`"",
    "-abslog=`"$editorLog`"",
    "-unattended", "-nopause", "-nosplash", "-nullrhi", "-nosound", "-NoLiveCoding"
) -join " "
Write-Host "Running automation tests '$Filter'..."
$proc = Start-Process -FilePath $editorCmd -ArgumentList $editorArgs -PassThru -NoNewWindow
if (-not $proc.WaitForExit($TimeoutMinutes * 60 * 1000)) {
    $proc | Stop-Process -Force
    throw "Automation run exceeded $TimeoutMinutes minutes and was killed. Log: $editorLog"
}

$index = Join-Path $ReportDir "index.json"
if (-not (Test-Path $index)) {
    Write-Host "::error::No automation report was written (editor exit $($proc.ExitCode)). The editor likely crashed or failed to load the module. Log: $editorLog"
    exit 1
}
$report = Get-Content -Raw -Path $index | ConvertFrom-Json

$discovered = $null
if (Test-Path $editorLog) {
    $m = Select-String -Path $editorLog -Pattern "Found (\d+) automation tests based on" | Select-Object -Last 1
    if ($m) { $discovered = [int]$m.Matches[0].Groups[1].Value }
}
$passed = [int]$report.succeeded + [int]$report.succeededWithWarnings
$failures = @($report.tests | Where-Object { $_.state -ne "Success" })

$summary = @(
    "### Automation tests ($Filter)", "",
    "| Passed | With warnings | Failed | Not run | Discovered |",
    "|---|---|---|---|---|",
    "| $($report.succeeded) | $($report.succeededWithWarnings) | $($report.failed) | $($report.notRun) | $(if ($null -ne $discovered) { $discovered } else { '?' }) |", ""
)
foreach ($t in $failures) {
    $messages = @($t.entries | Where-Object { $_.event.type -eq "Error" } | ForEach-Object { $_.event.message }) -join " / "
    $summary += "- **$($t.fullTestPath)**: $($t.state). $messages"
    Write-Host "::error title=$($t.fullTestPath)::$($t.state): $messages"
}
Write-StepSummary -Lines ($summary + "")

$problems = @()
if ([int]$report.failed -gt 0) { $problems += "$($report.failed) test(s) failed" }
if ([int]$report.notRun -gt 0) { $problems += "$($report.notRun) test(s) did not run" }
if ($passed -lt $MinimumTests) { $problems += "only $passed test(s) passed (minimum $MinimumTests)" }
if ($null -ne $discovered -and $passed -ne $discovered) { $problems += "$passed of $discovered discovered test(s) passed" }

if ($problems) {
    Write-Host "::error::Automation gate failed: $($problems -join '; ')"
    exit 1
}
Write-Host "Automation gate passed: $passed/$passed."
