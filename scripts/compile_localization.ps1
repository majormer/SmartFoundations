<# Compatibility entry point. Uses the shared source/asset gather and validated sync pipeline. #>
[CmdletBinding()]
param([string]$UnrealEditorCmd=$env:UNREAL_EDITOR_CMD, [string]$ProjectFile=$env:FACTORYGAME_UPROJECT,
      [string]$Python=$env:PYTHON, [string]$LogDir, [switch]$ShowVerbose)
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/..").Path
if (-not $UnrealEditorCmd) { $UnrealEditorCmd='C:/Program Files/Unreal Engine - CSS/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' }
if (-not $ProjectFile) { $ProjectFile=(Resolve-Path "$root/../../../FactoryGame.uproject").Path }
if (-not $Python) { $Python='python' }
if (-not $LogDir) { $LogDir=Join-Path $root ('.local/localization-'+(Get-Date -Format 'yyyyMMdd-HHmmss')) }
& "$PSScriptRoot/localization/refresh.ps1" -Root $root -Target SmartFoundations -ProjectFile $ProjectFile -UnrealEditorCmd $UnrealEditorCmd -Python $Python -ReportDirectory $LogDir
