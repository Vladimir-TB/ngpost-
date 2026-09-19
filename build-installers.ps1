param([string]$IsccPath = $env:ISCC_PATH)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
if (-not $IsccPath) {
    $onPath = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    $candidates = @(
        $(if($onPath) { $onPath.Source }),
        'D:\Dashboard-wdw\artifacts\tools\inno-setup-7.1.0-x64\ISCC.exe',
        (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe')
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }
    $IsccPath = $candidates | Select-Object -First 1
}
if (-not $IsccPath -or -not (Test-Path -LiteralPath $IsccPath)) { throw 'Set -IsccPath or ISCC_PATH to your existing ISCC.exe.' }
$out = Join-Path $projectRoot 'release\5.1.2-rc1-INTERN-UNSIGNED'
New-Item -ItemType Directory $out -Force | Out-Null
& $IsccPath (Join-Path $projectRoot 'installer\ngPost.iss')
if ($LASTEXITCODE -ne 0) { throw 'Inno compilation failed' }
$installer = Join-Path $out 'ngPost-setup-v5.1.2-rc1-UNSIGNED.exe'
if (-not (Test-Path -LiteralPath $installer)) { throw 'Expected installer missing' }
$portable = Join-Path $out 'ngPost-portable-v5.1.2-rc1-UNSIGNED.zip'
$stage = Join-Path $projectRoot ('artifacts\portable-packaging\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory $stage -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $projectRoot 'dist-qt6') | Where-Object { $_.Name -notin @('ngPost.conf','installer','tmp_test') } | Copy-Item -Destination $stage -Recurse
Set-Content -LiteralPath (Join-Path $stage 'portable.mode') -Value 'portable=1' -Encoding ascii
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $portable -Force
$guide = @(
    '# ngPost+ 5.1.2-rc1 - INTERN / UNSIGNED',
    '',
    'Deze kandidaat is bedoeld voor lokale beoordeling; nog niet publiek uitgeven.',
    '',
    '- Installeren: ngPost-setup-v5.1.2-rc1-UNSIGNED.exe (standaard Engels; Nederlands selecteerbaar).',
    '- Portable: pak ngPost-portable-v5.1.2-rc1-UNSIGNED.zip helemaal uit en start ngPost.exe.',
    '- Bewaar bij bijwerken je bestaande ngPost.conf. De ZIP bevat geen persoonlijke configuratie.',
    '- Beide pakketten bevatten dezelfde executable en runtime. De app is niet ondertekend.',
    '- Nieuw: hele UI kleurt mee, kopieerknoppen, Alle sessies starten, responsieve mapbatches en melding over uploadverantwoordelijkheid.',
    '- Raadpleeg ../../STATUS.md en ../../docs/LEGAL-REVIEW.md voor controlebewijs en openstaande uitgiftevoorwaarden.'
)
$guide | Set-Content -LiteralPath (Join-Path $out '00 - START HIER.md') -Encoding utf8
Get-FileHash -LiteralPath $installer,$portable -Algorithm SHA256 | ForEach-Object { '{0}  {1}' -f $_.Hash,(Split-Path $_.Path -Leaf) } | Set-Content (Join-Path $out 'SHA256SUMS.txt')
Write-Host "Installer en portable: $out"
