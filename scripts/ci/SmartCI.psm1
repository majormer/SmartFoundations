# Shared helpers for Smart! CI scripts.

function Resolve-EngineRoot {
    param([string]$EngineRoot)
    if ($EngineRoot) { return $EngineRoot }
    if ($env:SF_ENGINE_ROOT) { return $env:SF_ENGINE_ROOT }
    # The runner may run under a service account whose HKCU has no engine registration.
    $registered = (Get-ItemProperty "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds" -ErrorAction SilentlyContinue)."5.6.1-CSS"
    if ($registered) { return ($registered -replace '/', '\') }
    return "C:\Program Files\Unreal Engine - CSS"
}

function Assert-CIWorkspace {
    param([Parameter(Mandatory)][string]$ProjectRoot)
    $uproject = Join-Path $ProjectRoot "FactoryGame.uproject"
    if (-not (Test-Path $uproject)) { throw "No FactoryGame.uproject in '$ProjectRoot'." }
    if (-not (Test-Path (Join-Path $ProjectRoot ".sfci-workspace"))) {
        throw "'$ProjectRoot' is not a CI workspace (missing .sfci-workspace). CI never builds in a working tree; create one with scripts/ci/New-CIWorkspace.ps1."
    }
    return $uproject
}

function Assert-NoCIEditorRunning {
    param([Parameter(Mandatory)][string]$ProjectRoot)
    # Editors on the maintainer's own project are fine (separate files); one on THIS workspace is not.
    $root = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\')
    $busy = Get-CimInstance Win32_Process -Filter "Name LIKE 'UnrealEditor%'" -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -and $_.CommandLine.IndexOf($root, [StringComparison]::OrdinalIgnoreCase) -ge 0 }
    if ($busy) {
        throw "An editor is already running on the CI workspace (PID $($busy.ProcessId -join ', ')). Refusing to run concurrently."
    }
}

function Write-StepSummary {
    param([string[]]$Lines)
    if ($env:GITHUB_STEP_SUMMARY) { $Lines | Add-Content -Path $env:GITHUB_STEP_SUMMARY -Encoding utf8 }
}

Export-ModuleMember -Function Resolve-EngineRoot, Assert-CIWorkspace, Assert-NoCIEditorRunning, Write-StepSummary
