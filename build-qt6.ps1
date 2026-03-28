$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $root "src"
$build = Join-Path $root "build-qt6"
$qtBin = Join-Path $root ".qt\6.8.3\msvc2022_64\bin"
$vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

if (!(Test-Path $qtBin)) {
    throw "Qt 6.8.3 toolchain not found at $qtBin"
}

if (!(Test-Path $vcvars)) {
    throw "Visual Studio vcvars64.bat not found at $vcvars"
}

if (!(Test-Path $build)) {
    New-Item -ItemType Directory -Path $build | Out-Null
}

# Force a clean Qt 6 rebuild so version and resource changes are never
# skipped by incremental make state.
$releaseDir = Join-Path $build "release"
if (Test-Path $releaseDir) {
    Remove-Item $releaseDir -Recurse -Force
}
Get-ChildItem -Path $build -Filter "Makefile*" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

# Remove stale in-source generated UI headers so the Qt 6 build always
# uses the freshly generated headers from the build directory.
Get-ChildItem -Path $src -Filter "ui_*.h" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

# Force regeneration of the Windows icon resource when ngPost.ico changes.
$resourceRc = Join-Path $build "ngPost_resource.rc"
$resourceRes = Join-Path $build "release\ngPost_resource.res"
if (Test-Path $resourceRc) {
    Remove-Item $resourceRc -Force
}
if (Test-Path $resourceRes) {
    Remove-Item $resourceRes -Force
}

& (Join-Path $qtBin "lrelease.exe") (Join-Path $src "lang\ngPost_nl.ts") "-qm" (Join-Path $src "resources\lang\ngPost_nl.qm")
& (Join-Path $qtBin "lrelease.exe") (Join-Path $src "lang\ngPost_en.ts") "-qm" (Join-Path $src "resources\lang\ngPost_en.qm")
& (Join-Path $qtBin "lrelease.exe") (Join-Path $src "lang\ngPost_de.ts") "-qm" (Join-Path $src "resources\lang\ngPost_de.qm")

$cmd = "call `"$vcvars`" && cd /d `"$build`" && `"$qtBin\qmake.exe`" `"..\src\ngPost.pro`" && nmake /f Makefile.Release && `"$qtBin\windeployqt.exe`" --release --no-translations --no-system-d3d-compiler --no-opengl-sw `"$build\release\ngPost.exe`""
cmd /c $cmd

$release = Join-Path $build "release"
$dist = Join-Path $root "dist-qt6"
if (Test-Path $dist) {
    Remove-Item $dist -Recurse -Force
}
New-Item -ItemType Directory -Path $dist | Out-Null

$runtimeFiles = @(
    "ngPost.exe",
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Network.dll",
    "Qt6Svg.dll",
    "Qt6Widgets.dll",
    "dxcompiler.dll",
    "dxil.dll",
    "vc_redist.x64.exe"
)
foreach ($name in $runtimeFiles) {
    $source = Join-Path $release $name
    if (Test-Path $source) {
        Copy-Item $source $dist -Force
    }
}

$pluginDirs = @(
    "generic",
    "iconengines",
    "imageformats",
    "networkinformation",
    "platforms",
    "styles",
    "tls"
)
foreach ($dirName in $pluginDirs) {
    $sourceDir = Join-Path $release $dirName
    if (Test-Path $sourceDir) {
        Copy-Item $sourceDir (Join-Path $dist $dirName) -Recurse -Force
    }
}

$rarCandidates = @(
    (Join-Path $root "dist\rar.exe"),
    (Join-Path (Split-Path $root -Parent) "ngPost-master\dist\rar.exe")
)
foreach ($rarPath in $rarCandidates) {
    if (Test-Path $rarPath) {
        Copy-Item $rarPath (Join-Path $dist "rar.exe") -Force
        break
    }
}

$par2Candidates = @(
    (Join-Path $root "dist\par2.exe"),
    (Join-Path (Split-Path $root -Parent) "ngPost-master\dist\par2.exe"),
    "C:\Program Files\SABnzbd\win\par2\par2.exe",
    "C:\Program Files\Spotlite\sabnzbd\win\par2\par2.exe"
)
foreach ($par2Path in $par2Candidates) {
    if (Test-Path $par2Path) {
        Copy-Item $par2Path (Join-Path $dist "par2.exe") -Force
        break
    }
}

Write-Host ""
Write-Host "Qt 6 build ready:"
Write-Host "  $build\release\ngPost.exe"
Write-Host "  $dist\ngPost.exe"
