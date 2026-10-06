<#
.SYNOPSIS
    Builds Smart! for every shipping target in a CI workspace.

.DESCRIPTION
    Runs UnrealBuildTool once per target and fails on the first error. -WaitMutex makes a build
    queue behind any other UBT instance on this engine install (including the maintainer's own
    builds) instead of failing.

.PARAMETER ProjectRoot
    CI workspace root created by New-CIWorkspace.ps1.

.PARAMETER Targets
    "Target Platform Configuration" triples. Defaults to everything a release ships plus the
    editor target the automation tests run in.

.PARAMETER LogDir
    Where per-target UBT logs are written.

.PARAMETER ExtraUbtArgs
    Additional UnrealBuildTool arguments, defaulting to the SF_UBT_ARGS environment variable. The
    CI VM sets SF_UBT_ARGS=-AllCores: Hyper-V presents its vCPUs as hyperthreaded pairs, and UBT's
    default of one compile process per physical core would use only half of them.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ProjectRoot,
    [string[]]$Targets = @(
        "FactoryEditor Win64 Development",
        "FactoryGameSteam Win64 Shipping",
        "FactoryGameEGS Win64 Shipping",
        "FactoryServer Win64 Shipping",
        "FactoryServer Linux Shipping"
    ),
    [string]$EngineRoot,
    [string]$LogDir = (Join-Path $env:TEMP "smart-ci-build"),
    [string[]]$ExtraUbtArgs = @(if ($env:SF_UBT_ARGS) { $env:SF_UBT_ARGS -split '\s+' | Where-Object { $_ } })
)

$ErrorActionPreference = "Stop"
Import-Module (Join-Path $PSScriptRoot "SmartCI.psm1") -Force

$uproject = Assert-CIWorkspace -ProjectRoot $ProjectRoot
Assert-NoCIEditorRunning -ProjectRoot $ProjectRoot
$engine = Resolve-EngineRoot -EngineRoot $EngineRoot
$buildBat = Join-Path $engine "Engine\Build\BatchFiles\Build.bat"
if (-not (Test-Path $buildBat)) { throw "Build.bat not found under engine root '$engine'." }
if (($Targets -match "\bLinux\b") -and -not $env:LINUX_MULTIARCH_ROOT) {
    throw "A Linux target was requested but LINUX_MULTIARCH_ROOT is not set for this account."
}
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$summary = @("### Build", "", "| Target | Result | Time |", "|---|---|---|")
$failed = $false
foreach ($triple in $Targets) {
    $parts = $triple -split '\s+'
    if ($parts.Count -ne 3) { throw "Target '$triple' must be 'Target Platform Configuration'." }
    $log = Join-Path $LogDir (($parts -join '-') + ".log")
    Write-Host "::group::Build $triple"
    $sw = [Diagnostics.Stopwatch]::StartNew()
    & $buildBat $parts[0] $parts[1] $parts[2] "-project=$uproject" -WaitMutex -NoHotReloadFromIDE @ExtraUbtArgs 2>&1 | Tee-Object -FilePath $log
    $exit = $LASTEXITCODE
    $sw.Stop()
    Write-Host "::endgroup::"
    $time = "{0:mm\:ss}" -f $sw.Elapsed
    if ($exit -ne 0) {
        $summary += "| $triple | FAILED (exit $exit) | $time |"
        Write-Host "::error::Build failed: $triple (exit $exit). Log: $log"
        $failed = $true
        break
    }
    $summary += "| $triple | ok | $time |"
}
Write-StepSummary -Lines ($summary + "")
if ($failed) { exit 1 }
Write-Host "All targets built."
