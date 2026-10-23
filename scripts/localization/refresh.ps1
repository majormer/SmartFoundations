<# Gather both source and assets, reconcile translator catalogs, validate and compile.
   Does not build/cook/install product packages. Preserve the report folder for recovery. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Root,
    [Parameter(Mandatory=$true)][string]$Target,
    [Parameter(Mandatory=$true)][string]$ProjectFile,
    [Parameter(Mandatory=$true)][string]$UnrealEditorCmd,
    [Parameter(Mandatory=$true)][string]$Python,
    [Parameter(Mandatory=$true)][string]$ReportDirectory,
    [switch]$ResumeAfterGather
)
$ErrorActionPreference = 'Stop'
if (Get-Process FactoryGame*,FactoryServer*,UnrealEditor*,UnrealBuildTool*,RunUAT* -ErrorAction SilentlyContinue) {
    throw 'Game, editor or build is active. Preserve it and retry after it has finished.'
}
$Root = (Resolve-Path -LiteralPath $Root).Path
if ((Test-Path -LiteralPath $ReportDirectory) -and -not $ResumeAfterGather) { throw 'Use a fresh report directory; prior recovery evidence must remain intact.' }
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
$ReportDirectory = (Resolve-Path -LiteralPath $ReportDirectory).Path
$loc = Join-Path $Root "Content/Localization/$Target"
if (-not $ResumeAfterGather -and (Test-Path -LiteralPath $loc)) { Copy-Item -LiteralPath $loc -Destination "$ReportDirectory/before" -Recurse }
$manage = Join-Path $PSScriptRoot 'manage.py'
function Get-SourceIdentity {
    $items = foreach ($folder in @('Source','Content')) {
        Get-ChildItem -LiteralPath (Join-Path $Root $folder) -File -Recurse | Where-Object {
            $_.Extension -in @('.h','.cpp','.uasset','.umap')
        } | Sort-Object FullName | ForEach-Object {
            $_.FullName.Substring($Root.Length) + ':' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        }
    }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($items -join "`n"))
    return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes))
}
function Run-GatherStep([string]$phase) {
    $config = Join-Path $Root "Config/Localization/${Target}_${phase}.ini"
    $logName=$phase
    $attempt=1
    while (Test-Path -LiteralPath "$ReportDirectory/$logName.log") { $attempt++; $logName="$phase-$attempt" }
    $process = Start-Process -FilePath $UnrealEditorCmd -ArgumentList @(
        "`"$ProjectFile`"", '-run=GatherText', "-config=`"$config`"", '-NullRHI', '-unattended', '-nosound',
        '-nosplash', '-NoAutoLoadSession', '-UTF8Output', "-abslog=`"$ReportDirectory/$logName.log`""
    ) -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportDirectory/$logName-stdout.log" -RedirectStandardError "$ReportDirectory/$logName-stderr.log"
    $process.WaitForExit()
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "$phase failed: $($process.ExitCode). Inspect retained logs." }
    # The completion marker is required as well as process exit, including hosts with null ExitCode.
    $log = Get-Content -LiteralPath "$ReportDirectory/$logName.log" -Raw
    if ($log -notmatch 'GatherText completed with exit code 0|Success - 0 error') { throw "$phase did not confirm successful completion" }
}
if ($ResumeAfterGather) {
    if ((Get-Content "$ReportDirectory/Gather.log" -Raw) -notmatch 'GatherText completed with exit code 0') {
        throw 'Cannot resume: no successful gather receipt.'
    }
    $context = Get-Content "$ReportDirectory/source-identity.json" -Raw | ConvertFrom-Json
    if ($context.Root -ne $Root -or $context.Target -ne $Target -or $context.Hash -ne (Get-SourceIdentity)) {
        throw 'Gather belongs to different or changed source. Start a fresh report directory.'
    }
} else {
    Run-GatherStep 'Gather'
    @{Root=$Root; Target=$Target; Hash=(Get-SourceIdentity)} | ConvertTo-Json | Set-Content "$ReportDirectory/source-identity.json" -Encoding utf8
}
& $Python $manage reconcile --root $Root --target $Target --report "$ReportDirectory/reconcile.json"
if ($LASTEXITCODE -ne 0) { throw 'Catalog reconciliation failed' }
& $Python $manage sync --root $Root --target $Target --report "$ReportDirectory/sync.json"
if ($LASTEXITCODE -ne 0) { throw 'Translation validation/sync failed' }
Run-GatherStep 'Compile'
& $Python $manage audit --root $Root --target $Target --report "$ReportDirectory/audit.json"
if ($LASTEXITCODE -ne 0) { throw 'Post-compile audit failed' }
Write-Output 'Localization compiled locally. Product packaging and language review are separate steps.'
