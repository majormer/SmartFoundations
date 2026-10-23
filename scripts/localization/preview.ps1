[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Pages, [Parameter(Mandatory=$true)][string]$Output,
      [string]$Images, [string]$CameraImages)
$ErrorActionPreference='Stop'
$policy = Get-Content "$PSScriptRoot/policy.json" -Raw | ConvertFrom-Json -AsHashtable
New-Item -ItemType Directory -Force $Output | Out-Null
if ($Images) {
    New-Item -ItemType Directory -Force "$Output/images" | Out-Null
    Get-ChildItem -LiteralPath $Images | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination "$Output/images" -Recurse -Force
    }
}
if ($CameraImages) {
    New-Item -ItemType Directory -Force "$Output/images/camera" | Out-Null
    Copy-Item -Path "$CameraImages/*" -Destination "$Output/images/camera" -Force
}
foreach ($page in Get-ChildItem -LiteralPath $Pages -Filter '*.md') {
    $culture='en'
    foreach ($candidate in ($policy.cultures.Keys | Sort-Object Length -Descending)) {
        if ($page.BaseName.StartsWith("$candidate-")) { $culture=$candidate; break }
    }
    $direction = if ($culture -in $policy.rtl) {'rtl'} else {'ltr'}
    $text=Get-Content -LiteralPath $page.FullName -Raw
    $text=$text.Replace('https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/','')
    $text=$text.Replace('https://raw.githubusercontent.com/majormer/SmartCamera/master/docs/images/2.0/','images/camera/')
    $text=[regex]::Replace($text,'\]\(([A-Za-z_][A-Za-z0-9_-]*)(#[^)]*)?\)', {param($m) ']('+$m.Groups[1].Value+'.html'+$m.Groups[2].Value+')'})
    $body=(ConvertFrom-Markdown -InputObject $text).Html
    $html="<!doctype html><html lang=`"$culture`" dir=`"$direction`"><meta charset=`"utf-8`"><meta name=`"viewport`" content=`"width=device-width`"><style>body{font:17px/1.6 system-ui,sans-serif;max-width:1100px;margin:auto;padding:24px;overflow-wrap:anywhere}img{max-width:100%}pre,code{direction:ltr;unicode-bidi:isolate}table{border-collapse:collapse}td,th{border:1px solid #bbb;padding:8px}blockquote{background:#fff3d9;padding:12px}</style><body>$body</body></html>"
    [IO.File]::WriteAllText((Join-Path $Output ($page.BaseName+'.html')), $html, [Text.UTF8Encoding]::new($false))
}
Write-Output 'Localized preview generated. Fallback notices and review status remain visible.'
