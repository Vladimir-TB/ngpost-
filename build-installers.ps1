$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$iss = Join-Path $root "installer\ngPost.iss"

$iscc = $env:ISCC_PATH
if (-not $iscc) {
    $candidates = @(
        (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe")
    ) | Where-Object { $_ -and (Test-Path $_) }

    if ($candidates.Count -gt 0) {
        $iscc = $candidates[0]
    }
}

if (-not $iscc -or -not (Test-Path $iscc)) {
    throw "ISCC.exe not found. Set ISCC_PATH or install Inno Setup 6."
}

& $iscc $iss

$legacyNoRuntime = Join-Path $root "dist-qt6\installer\ngPost-setup-v5.1.1-no-runtime.exe"
if (Test-Path $legacyNoRuntime) {
    Remove-Item $legacyNoRuntime -Force
}

Write-Host ""
Write-Host "Installer ready:"
Write-Host "  $root\dist-qt6\installer\ngPost-setup-v5.1.1.exe"
