param([switch]$Baseline)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$qtBin = Join-Path $projectRoot '.qt\6.8.3\msvc2022_64\bin'
$vcvars = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
$evidence = Join-Path $projectRoot 'artifacts\issue-validation'
$testBuild = Join-Path $evidence 'build'
New-Item -ItemType Directory -Path $testBuild -Force | Out-Null
$command = 'call "{0}" && cd /d "{1}" && "{2}\qmake.exe" "{3}\issue-validation.pro" && nmake /f Makefile.Release' -f $vcvars, $testBuild, $qtBin, $PSScriptRoot
cmd /c $command *> (Join-Path $evidence 'test-build.log')
if ($LASTEXITCODE -ne 0) { throw 'Issue test build failed' }
$saved = @{}
foreach ($key in @('PATH','QT_QPA_PLATFORM','QT_PLUGIN_PATH','NGPOST_TEST_COUNT','NGPOST_ENFORCE_RESPONSIVENESS')) { $saved[$key] = [Environment]::GetEnvironmentVariable($key) }
try {
    $env:PATH = "$qtBin;$env:PATH"
    $env:QT_QPA_PLATFORM = 'offscreen'
    $env:QT_PLUGIN_PATH = Join-Path $qtBin '..\plugins'
    $env:NGPOST_ENFORCE_RESPONSIVENESS = if ($Baseline) { $null } else { '1' }
    foreach ($count in @(10,100,500)) {
        $env:NGPOST_TEST_COUNT = "$count"
        $prefix = if ($Baseline) { 'baseline' } else { 'result' }
        & (Join-Path $testBuild 'release\issue-validation.exe') *> (Join-Path $evidence "$prefix-$count.log")
        if ($LASTEXITCODE -ne 0) { throw "Issue validation failed: $prefix-$count.log" }
        Get-Content (Join-Path $evidence "$prefix-$count.log") -Tail 1
    }
} finally { foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key]) } }
