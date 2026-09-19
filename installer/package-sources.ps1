$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$inputRoot = Join-Path $projectRoot 'artifacts\release-compliance'
$out = Join-Path $projectRoot 'artifacts\source-backups\5.1.2'
New-Item -ItemType Directory $out -Force | Out-Null
$stage = Join-Path $projectRoot ('artifacts\source-packaging\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory $stage -Force | Out-Null
$qtHashes = @{
    'qtbase-everywhere-src-6.8.3.tar.xz' = '56001b905601bb9023d399f3ba780d7fa940f3e4861e496a7c490331f49e0b80'
    'qtsvg-everywhere-src-6.8.3.tar.xz' = '35eb516460f00f264eb504baa253432384351cf23fb9980a5857190e8deef438'
}
foreach ($name in $qtHashes.Keys) {
    $file = Join-Path $inputRoot $name
    if ((Get-FileHash -LiteralPath $file).Hash -ne $qtHashes[$name]) { throw "Unexpected Qt source: $name" }
    Copy-Item -LiteralPath $file -Destination $stage
}
$par2Root = Join-Path $inputRoot 'par2-source'
$par2Commit = git -C $par2Root rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $par2Commit -ne 'f220e1f1a74796006ca01e717e411a11e7b69a07') { throw 'Unexpected par2 source revision' }
if ((Get-FileHash (Join-Path $projectRoot 'dist-qt6\par2.exe')).Hash -ne 'F582C368A07D4B0BBBEFC0B592BAC79077E16FD8BB1DD3C19F04BF18C444C176') { throw 'Unexpected par2 binary' }
git -C $par2Root archive --format=zip --prefix=par2cmdline-turbo-1.3.0/ -o (Join-Path $stage 'par2cmdline-turbo-1.3.0-source.zip') HEAD
if ($LASTEXITCODE -ne 0) { throw 'par2 source archive failed' }
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\RELEASE.md') -Destination (Join-Path $stage 'README.md')
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath (Join-Path $out 'ngPost-dependency-sources-v5.1.2.zip') -Force
git -C $projectRoot archive --format=zip --prefix=ngPost-5.1.2/ -o (Join-Path $out 'ngPost-source-v5.1.2.zip') v5.1.2
if ($LASTEXITCODE -ne 0) { throw 'ngPost source archive failed' }
$sourceCommit = git -C $projectRoot rev-parse 'v5.1.2^{commit}'
Set-Content -LiteralPath (Join-Path $out '00 - START HIER.md') -Value "`nBroncommit: $sourceCommit. Interne bronbackup; niet als release-asset uploaden."
Get-ChildItem -LiteralPath $out -File | Where-Object { $_.Extension -in '.exe','.zip' } | Get-FileHash -Algorithm SHA256 | ForEach-Object { '{0}  {1}' -f $_.Hash,(Split-Path $_.Path -Leaf) } | Set-Content (Join-Path $out 'SHA256SUMS.txt')
Write-Host "Source archives and checksums: $out"
