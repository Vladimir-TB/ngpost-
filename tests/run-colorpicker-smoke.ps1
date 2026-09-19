$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$qtBin = Join-Path $projectRoot '.qt\6.8.3\msvc2022_64\bin'
$vcvars = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
$evidence = Join-Path $projectRoot 'artifacts\colorpicker'
$smokeBuild = Join-Path $evidence 'smoke-build'
New-Item -ItemType Directory -Path $smokeBuild -Force | Out-Null

# Build in place; do not run packaging, delete distribution files or load personal settings.
$buildCommand = 'call "{0}" && cd /d "{1}\build-qt6" && "{2}\qmake.exe" "..\src\ngPost.pro" && nmake /f Makefile.Release' -f $vcvars, $projectRoot, $qtBin
cmd /c $buildCommand *> (Join-Path $evidence 'build.log')
if ($LASTEXITCODE -ne 0) { throw 'Product build failed; see artifacts/colorpicker/build.log.' }
$testCommand = 'call "{0}" && cd /d "{1}" && "{2}\qmake.exe" "{3}\colorpicker-smoke.pro" && nmake /f Makefile.Release' -f $vcvars, $smokeBuild, $qtBin, $PSScriptRoot
cmd /c $testCommand *> (Join-Path $evidence 'smoke-build.log')
if ($LASTEXITCODE -ne 0) { throw 'Smoke build failed; see artifacts/colorpicker/smoke-build.log.' }

$originalPath = $env:PATH
$originalPlatform = $env:QT_QPA_PLATFORM
$originalPlugins = $env:QT_PLUGIN_PATH
$originalRestart = $env:COLORPICKER_SMOKE_RESTART
try {
    $env:PATH = "$qtBin;$originalPath"
    $env:QT_QPA_PLATFORM = 'offscreen'
    $env:QT_PLUGIN_PATH = Join-Path $qtBin '..\plugins'
    $env:COLORPICKER_SMOKE_RESTART = $null
    $executable = Join-Path $smokeBuild 'release\colorpicker-smoke.exe'
    & $executable *> (Join-Path $evidence 'smoke.log')
    if ($LASTEXITCODE -ne 0) { throw 'UI smoke failed; see artifacts/colorpicker/smoke.log.' }
    $env:COLORPICKER_SMOKE_RESTART = '1'
    & $executable *> (Join-Path $evidence 'restart.log')
    if ($LASTEXITCODE -ne 0) { throw 'Restart smoke failed; see artifacts/colorpicker/restart.log.' }
    Write-Host 'PASS: colorpicker UI, persistence, restart and reset. Evidence: artifacts/colorpicker.'
}
finally {
    $env:PATH = $originalPath
    $env:QT_QPA_PLATFORM = $originalPlatform
    $env:QT_PLUGIN_PATH = $originalPlugins
    $env:COLORPICKER_SMOKE_RESTART = $originalRestart
}
