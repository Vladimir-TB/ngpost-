$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$qtBin = Join-Path $projectRoot '.qt\6.8.3\msvc2022_64\bin'
$build = Join-Path $projectRoot 'build-qt6'
$dist = Join-Path $projectRoot 'dist-qt6'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$staging = Join-Path $projectRoot "artifacts\distribution-staging\$stamp"
New-Item -ItemType Directory $build,$staging -Force | Out-Null
foreach($language in @('nl','en','de')) {
    & (Join-Path $qtBin 'lrelease.exe') (Join-Path $projectRoot "src\lang\ngPost_$language.ts") '-qm' (Join-Path $projectRoot "src\resources\lang\ngPost_$language.qm")
    if ($LASTEXITCODE -ne 0) { throw "Translation failed: $language" }
}
$command = 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && cd /d "{0}" && "{1}\qmake.exe" "..\src\ngPost.pro" && nmake /f Makefile.Release' -f $build,$qtBin
cmd /c $command
if ($LASTEXITCODE -ne 0) { throw 'Qt build failed' }
Copy-Item -LiteralPath (Join-Path $build 'release\ngPost.exe') -Destination $staging
& (Join-Path $qtBin 'windeployqt.exe') --release --no-translations --no-system-d3d-compiler --no-opengl-sw --dir $staging (Join-Path $staging 'ngPost.exe')
if ($LASTEXITCODE -ne 0) { throw 'Qt runtime deployment failed' }
# QtConcurrent's template-only call sites may not appear in the PE import table.
Copy-Item -LiteralPath (Join-Path $qtBin 'Qt6Concurrent.dll') -Destination $staging
foreach ($tool in @('rar.exe','par2.exe')) {
    $candidate = @((Join-Path $dist $tool),(Join-Path $projectRoot "dist\$tool")) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $candidate) { throw "Bundled tool missing: $tool. Supply an authorized binary in dist-qt6." }
    Copy-Item -LiteralPath $candidate -Destination $staging
}
if (-not (Test-Path (Join-Path $staging 'vc_redist.x64.exe'))) { Copy-Item -LiteralPath (Join-Path $dist 'vc_redist.x64.exe') -Destination $staging }
foreach($required in @('ngPost.exe','Qt6Concurrent.dll','Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Network.dll','platforms\qwindows.dll')) {
    if (-not (Test-Path (Join-Path $staging $required))) { throw "Incomplete runtime: $required" }
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE') -Destination $staging
$notices = Join-Path $staging 'notices'
New-Item -ItemType Directory $notices -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'installer\voorwaarden.txt'),(Join-Path $projectRoot 'installer\terms-en.txt') -Destination $notices
Copy-Item -Path (Join-Path $projectRoot 'installer\notices\*') -Destination $notices -Recurse
if (-not (Test-Path (Join-Path $notices 'Qt-6.8.3\qtbase\LICENSES\LGPL-3.0-only.txt'))) { throw 'Qt license notices missing; run installer/prepare-notices.py.' }
Set-Content -LiteralPath (Join-Path $staging 'portable.mode') -Value 'portable=1' -Encoding ascii
# Preserve configuration and archive the complete former distribution before replacement.
if (Test-Path -LiteralPath (Join-Path $dist 'ngPost.conf')) { Copy-Item -LiteralPath (Join-Path $dist 'ngPost.conf') -Destination $staging }
if (Test-Path -LiteralPath $dist) {
    $resolved = (Resolve-Path -LiteralPath $dist).Path
    if ($resolved -ne (Join-Path $projectRoot 'dist-qt6')) { throw 'Unexpected distribution path' }
    $backup = Join-Path $projectRoot "artifacts\distribution-backups\$stamp"
    New-Item -ItemType Directory $backup -Force | Out-Null
    Move-Item -LiteralPath $resolved -Destination (Join-Path $backup 'dist-qt6')
}
$resolvedStaging = (Resolve-Path -LiteralPath $staging).Path
if (-not $resolvedStaging.StartsWith((Join-Path $projectRoot 'artifacts\distribution-staging\'))) { throw 'Unexpected staging path' }
Move-Item -LiteralPath $resolvedStaging -Destination $dist
Write-Host "Portable bijgewerkt: $dist\ngPost.exe"
