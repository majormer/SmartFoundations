#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Compiles SmartFoundations localization files (.po -> .locres).

.DESCRIPTION
    Runs Unreal Engine's GatherText commandlet in three phases:
    1. Gather source text and update .archive files.
    2. Sync .po translations into .archive files.
    3. Compile .archive files into .locres files.

    The script is repository-relative. Provide the Unreal commandlet and
    FactoryGame project path through parameters or environment variables.

.PARAMETER UnrealEditorCmd
    Path to UnrealEditor-Cmd.exe. Defaults to $env:UNREAL_EDITOR_CMD, then the
    common Coffee Stain Unreal Engine install path if present.

.PARAMETER ProjectFile
    Path to FactoryGame.uproject. Defaults to $env:FACTORYGAME_UPROJECT, then a
    parent SML workspace layout if present.

.PARAMETER Python
    Python executable used to run sync_po_to_archive.py. Defaults to
    $env:PYTHON, then python.

.PARAMETER LogDir
    Directory for temporary commandlet logs. Defaults to .local/logs under the
    repository root.

.PARAMETER ShowVerbose
    Show detailed output from the Unreal Engine commandlet.

.EXAMPLE
    $env:UNREAL_EDITOR_CMD = "C:/Program Files/Unreal Engine - CSS/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
    $env:FACTORYGAME_UPROJECT = "D:/SatisfactoryModLoader/FactoryGame.uproject"
    ./scripts/compile_localization.ps1
#>

[CmdletBinding()]
param(
    [string]$UnrealEditorCmd = $env:UNREAL_EDITOR_CMD,
    [string]$ProjectFile = $env:FACTORYGAME_UPROJECT,
    [string]$Python = $env:PYTHON,
    [string]$LogDir,
    [switch]$ShowVerbose
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..")

if (-not $Python) {
    $Python = "python"
}

if (-not $UnrealEditorCmd) {
    $DefaultUnrealEditorCmd = "C:\Program Files\Unreal Engine - CSS\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
    if (Test-Path -LiteralPath $DefaultUnrealEditorCmd) {
        $UnrealEditorCmd = $DefaultUnrealEditorCmd
    }
}

if (-not $ProjectFile) {
    $CandidateProjectFile = Join-Path $RepoRoot "..\..\FactoryGame.uproject"
    if (Test-Path -LiteralPath $CandidateProjectFile) {
        $ProjectFile = (Resolve-Path -LiteralPath $CandidateProjectFile).Path
    }
}

if (-not $LogDir) {
    $LogDir = Join-Path $RepoRoot ".local\logs"
}

$GatherConfig = Join-Path $RepoRoot "Config\Localization\SmartFoundations_Gather.ini"
$CompileConfig = Join-Path $RepoRoot "Config\Localization\SmartFoundations_Compile.ini"
$LocalizationRoot = Join-Path $RepoRoot "Content\Localization\SmartFoundations"

function Assert-PathExists {
    param(
        [string]$Path,
        [string]$Description
    )

    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) {
        throw "$Description not found. Provide it with a parameter or environment variable. Path: $Path"
    }
}

Write-Host "Validating localization pipeline paths..." -ForegroundColor Cyan
Assert-PathExists -Path $UnrealEditorCmd -Description "Unreal Engine commandlet"
Assert-PathExists -Path $ProjectFile -Description "FactoryGame project file"
Assert-PathExists -Path $GatherConfig -Description "Gather config"
Assert-PathExists -Path $CompileConfig -Description "Compile config"
Assert-PathExists -Path $LocalizationRoot -Description "Localization root"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
Write-Host "All paths validated." -ForegroundColor Green

function Invoke-GatherStep {
    param(
        [string]$StepName,
        [string]$ConfigPath,
        [string]$LogPrefix
    )

    Write-Host "Running $StepName..." -ForegroundColor Gray

    $CommandletArgs = @(
        "`"$ProjectFile`"",
        "-run=GatherText",
        "-config=`"$ConfigPath`"",
        "-unattended",
        "-NoLogTimes",
        "-UTF8Output"
    )

    if (-not $ShowVerbose) {
        $CommandletArgs += "-LogCmds=`"Global None`""
    }

    $OutLog = Join-Path $LogDir "${LogPrefix}_output.log"
    $ErrLog = Join-Path $LogDir "${LogPrefix}_error.log"

    $Process = Start-Process -FilePath $UnrealEditorCmd `
        -ArgumentList $CommandletArgs `
        -NoNewWindow `
        -Wait `
        -PassThru `
        -RedirectStandardOutput $OutLog `
        -RedirectStandardError $ErrLog

    if ($Process.ExitCode -ne 0) {
        Write-Host ""
        Write-Error "$StepName failed with exit code: $($Process.ExitCode)"
        if (Test-Path -LiteralPath $ErrLog) {
            $ErrorContent = Get-Content -LiteralPath $ErrLog -Raw
            if ($ErrorContent.Trim()) {
                Write-Host "Error output:" -ForegroundColor Red
                Write-Host $ErrorContent -ForegroundColor Red
            }
        }
        Write-Host "Check $OutLog for details." -ForegroundColor Yellow
        return $false
    }

    if (-not $ShowVerbose) {
        if (Test-Path -LiteralPath $OutLog) { Remove-Item -LiteralPath $OutLog -Force }
        if (Test-Path -LiteralPath $ErrLog) { Remove-Item -LiteralPath $ErrLog -Force }
    }

    return $true
}

$StartTime = Get-Date

Write-Host ""
Write-Host "Phase 1: Gathering source text..." -ForegroundColor Cyan
$GatherOk = Invoke-GatherStep -StepName "Gather" -ConfigPath $GatherConfig -LogPrefix "loc_gather"
if (-not $GatherOk) { exit 1 }
Write-Host "Gather complete." -ForegroundColor Green

Write-Host ""
Write-Host "Phase 2: Syncing .po translations into .archive files..." -ForegroundColor Cyan
$SyncScript = Join-Path $ScriptDir "sync_po_to_archive.py"
Assert-PathExists -Path $SyncScript -Description "sync_po_to_archive.py"

& $Python $SyncScript
if ($LASTEXITCODE -ne 0) {
    Write-Error "sync_po_to_archive.py failed with exit code: $LASTEXITCODE"
    exit $LASTEXITCODE
}
Write-Host ".po translations synced into .archive files." -ForegroundColor Green

Write-Host ""
Write-Host "Phase 3: Compiling .archive files to .locres..." -ForegroundColor Cyan
$CompileOk = Invoke-GatherStep -StepName "Compile" -ConfigPath $CompileConfig -LogPrefix "loc_compile"
if (-not $CompileOk) { exit 1 }

Write-Host ""
Write-Host "Phase 4: Removing .locres files for disabled languages..." -ForegroundColor Cyan
$DisabledLanguages = @("ar", "fa", "th")

foreach ($Language in $DisabledLanguages) {
    $LocresFile = Join-Path $LocalizationRoot "$Language\SmartFoundations.locres"
    if (Test-Path -LiteralPath $LocresFile) {
        Remove-Item -LiteralPath $LocresFile -Force
        Write-Host "Removed $Language\SmartFoundations.locres" -ForegroundColor Yellow
    } else {
        Write-Host "$Language\SmartFoundations.locres already absent" -ForegroundColor Gray
    }
}

$Duration = (Get-Date) - $StartTime
Write-Host ""
Write-Host "Localization pipeline completed successfully." -ForegroundColor Green
Write-Host "Duration: $($Duration.TotalSeconds.ToString('F2')) seconds" -ForegroundColor Gray

$LocresFiles = Get-ChildItem -Path $LocalizationRoot -Recurse -Filter "*.locres" -File
Write-Host ""
Write-Host "Generated .locres files:" -ForegroundColor Cyan
foreach ($File in $LocresFiles) {
    $RelativePath = $File.FullName.Replace($LocalizationRoot + "\", "")
    $SizeKB = [math]::Round($File.Length / 1KB, 2)
    Write-Host "  $RelativePath ($SizeKB KB)" -ForegroundColor Gray
}

Write-Host ""
Write-Host "Next step: package the mod with Alpakit when ready." -ForegroundColor Yellow
exit 0
