$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$qtBin = Join-Path $projectRoot '.qt\6.8.3\msvc2022_64\bin'
$evidence = Join-Path $projectRoot 'artifacts\session-workflow'
$testBuild = Join-Path $evidence 'build'
New-Item -ItemType Directory $testBuild -Force | Out-Null
$command = 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && cd /d "{0}" && "{1}\qmake.exe" "{2}\session-workflow.pro" && nmake /f Makefile.Release' -f $testBuild,$qtBin,$PSScriptRoot
cmd /c $command *> (Join-Path $evidence 'build.log')
if ($LASTEXITCODE -ne 0) { throw 'Session workflow build failed' }
$saved = @{}
foreach ($key in @('PATH','QT_QPA_PLATFORM','QT_PLUGIN_PATH','NGPOST_TEST_PREPARE_PACKING')) { $saved[$key]=[Environment]::GetEnvironmentVariable($key) }
try {
    Copy-Item -LiteralPath (Join-Path $projectRoot 'dist-qt6\rar.exe') -Destination (Join-Path $testBuild 'release\rar.exe') -Force
    $env:PATH="$qtBin;$env:PATH"
    $env:QT_QPA_PLATFORM='offscreen'
    $env:QT_PLUGIN_PATH=Join-Path $qtBin '..\plugins'
    foreach($mode in @('false','true')) {
        $env:NGPOST_TEST_PREPARE_PACKING=$mode
        $log=Join-Path $evidence "result-prepare-$mode.log"
        & (Join-Path $testBuild 'release\session-workflow.exe') *> $log
        if ($LASTEXITCODE -ne 0) { throw "Session workflow failed: $log" }
        Get-Content $log -Tail 4
    }
} finally { foreach($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key,$saved[$key]) } }
