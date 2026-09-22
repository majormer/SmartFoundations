<#
.SYNOPSIS
    Creates or refreshes the isolated project copy that CI builds and tests in.

.DESCRIPTION
    Smart! only builds inside a full Satisfactory mod project (Mods/GameFeatures/SmartFoundations).
    CI must never build in the maintainer's working tree: it would overwrite their binaries and
    fight their editor for files. This script mirrors the engine-side parts of the dev project
    (Source, Content, Config, Plugins, SML) into a separate root and clones Smart into it.

    Excluded from the copy:
      - Intermediate/, Saved/, DerivedDataCache/  (UBT makefiles store absolute paths; rebuilt per workspace)
      - Mods/GameFeatures/*                       (the maintainer's other mods; CI clones Smart itself)
      - Mods/SmartFoundations                     (pre-GameFeatures leftover; would duplicate the plugin name)
      - Plugins/AdaMCP                            (editor MCP server; would bind a port during CI editor runs)

    Re-running refreshes the engine-side copy (robocopy only copies changed files) and leaves the
    Smart clone and all build outputs in place.

.PARAMETER DevRoot
    The maintainer's project root (contains FactoryGame.uproject).

.PARAMETER CiRoot
    Destination root for the CI workspace. Keep it short: UE paths are long and Windows MAX_PATH bites.

.PARAMETER RepoUrl
    Smart's repository. HTTPS so the runner account needs no SSH key (the repo is public).
#>
[CmdletBinding()]
param(
    [string]$DevRoot = "L:\SatisfactoryDevEnvironment\SatisfactoryModLoader",
    [string]$CiRoot = "L:\SFCI\SML",
    [string]$RepoUrl = "https://github.com/majormer/SmartFoundations.git"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path (Join-Path $DevRoot "FactoryGame.uproject"))) {
    throw "DevRoot '$DevRoot' does not contain FactoryGame.uproject."
}
if ([IO.Path]::GetFullPath($DevRoot).TrimEnd('\') -ieq [IO.Path]::GetFullPath($CiRoot).TrimEnd('\')) {
    throw "CiRoot must differ from DevRoot."
}

New-Item -ItemType Directory -Force -Path $CiRoot | Out-Null

$excludeDirs = @(
    "Intermediate", "Saved", "DerivedDataCache", ".git", ".vs",
    (Join-Path $DevRoot "Mods\GameFeatures"),
    (Join-Path $DevRoot "Mods\SmartFoundations"),
    (Join-Path $DevRoot "Plugins\AdaMCP"),
    (Join-Path $DevRoot 'Plugins\$mod')
)
Write-Host "Mirroring engine-side project files: $DevRoot -> $CiRoot"
$robocopyArgs = @($DevRoot, $CiRoot, "/E", "/XD") + $excludeDirs + @("/XF", "*.sln", "/R:1", "/W:1", "/MT:16", "/NFL", "/NDL", "/NP", "/NJH")
& robocopy @robocopyArgs
# robocopy exit codes 0-7 are success variants; 8+ means at least one failure.
if ($LASTEXITCODE -ge 8) { throw "robocopy failed with exit code $LASTEXITCODE" }

$smartDir = Join-Path $CiRoot "Mods\GameFeatures\SmartFoundations"
if (-not (Test-Path (Join-Path $smartDir ".git"))) {
    New-Item -ItemType Directory -Force -Path (Split-Path $smartDir) | Out-Null
    Write-Host "Cloning $RepoUrl -> $smartDir"
    git clone --no-tags $RepoUrl $smartDir
    if ($LASTEXITCODE -ne 0) { throw "git clone failed" }
} else {
    Write-Host "Smart clone already present: $smartDir"
}

# Marker the build/test scripts require, so they can never run against a working tree by mistake.
Set-Content -Path (Join-Path $CiRoot ".sfci-workspace") -Value "Smart! CI workspace. Mirrored from $DevRoot on $(Get-Date -Format s)." -Encoding utf8
Write-Host "CI workspace ready: $CiRoot"
