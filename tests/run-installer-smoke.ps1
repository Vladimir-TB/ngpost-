param([string]$IsccPath = 'D:\Dashboard-wdw\artifacts\tools\inno-setup-7.1.0-x64\ISCC.exe')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$evidence=Join-Path $projectRoot ('artifacts\installer-smoke\'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory $evidence -Force | Out-Null
$identity=[guid]::NewGuid().ToString()
# Same installer source and payload, with a separate identity so personal installations remain untouched.
& $IsccPath "-dMyAppId={{${identity}}}" "-dMyAppName=ngPost Verification $($identity.Substring(0,8))" "-dMyOutputDir=$evidence" "-dMyPortableBase=$evidence" (Join-Path $projectRoot 'installer\ngPost.iss') *> (Join-Path $evidence 'compile.log')
if($LASTEXITCODE -ne 0) { throw 'Verification installer compile failed' }
$setup=Join-Path $evidence 'ngPost-setup-v5.1.2-UNSIGNED.exe'
$productHash=(Get-FileHash (Join-Path $projectRoot 'dist-qt6\ngPost.exe')).Hash
function Install-Fixture([string]$label,[string]$directory,[string]$options,[string]$language) {
    $log=Join-Path $evidence "$label.log"
    $args='/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /NOICONS /DIR="{0}" /LOG="{1}" {2}' -f $directory,$log,$options
    $process=Start-Process -FilePath $setup -ArgumentList $args -WindowStyle Hidden -PassThru -Wait
    if($process.ExitCode -ne 0) { throw "Install failed: $label ($($process.ExitCode))" }
    if(-not (Select-String -LiteralPath $log -SimpleMatch "Selected setup language: $language")) { throw "Unexpected language: $label" }
    if((Get-FileHash (Join-Path $directory 'ngPost.exe')).Hash -ne $productHash) { throw 'Installed executable differs' }
    foreach($notice in @('LICENSE','notices\README.txt','notices\RAR-EULA-en.html','notices\Qt-6.8.3\qtbase\LICENSES\LGPL-3.0-only.txt','notices\par2cmdline-turbo-1.3.0\COPYING')) {
        if(-not (Test-Path (Join-Path $directory $notice))) { throw "Missing packaged notice: $notice" }
    }
}
function Uninstall-Fixture([string]$directory) {
    $resolved=(Resolve-Path -LiteralPath $directory).Path
    if(-not $resolved.StartsWith($evidence+'\')) { throw 'Uninstall outside test workspace refused' }
    $exe=Join-Path $resolved 'unins000.exe'
    if(Test-Path -LiteralPath $exe) {
        $process=Start-Process -FilePath $exe -ArgumentList '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART' -WindowStyle Hidden -Wait -PassThru
        if($process.ExitCode -ne 0) { throw 'Test uninstall failed' }
    }
}
$standard=Join-Path $evidence 'standard'
$portable=Join-Path $evidence 'portable'
try {
    Install-Fixture 'fresh-default' $standard '' 'english'
    if(Test-Path (Join-Path $standard 'portable.mode')) { throw 'Normal install has portable marker' }
    Set-Content (Join-Path $standard 'ngPost.conf') 'TEST-CONFIG-PRESERVE' -Encoding ascii
    $configHash=(Get-FileHash (Join-Path $standard 'ngPost.conf')).Hash
    Install-Fixture 'explicit-dutch' $standard '/LANG=dutch' 'dutch'
    Install-Fixture 'upgrade-preserves-dutch' $standard '' 'dutch'
    if((Get-FileHash (Join-Path $standard 'ngPost.conf')).Hash -ne $configHash) { throw 'Upgrade changed existing config' }
    Uninstall-Fixture $standard
    Install-Fixture 'portable-english' $portable '/MODE=portable /LANG=english' 'english'
    if(-not (Test-Path (Join-Path $portable 'portable.mode'))) { throw 'Portable marker missing' }
    Set-Content (Join-Path $portable 'ngPost.conf') 'TEST-PORTABLE-PRESERVE' -Encoding ascii
    $configHash=(Get-FileHash (Join-Path $portable 'ngPost.conf')).Hash
    Install-Fixture 'portable-upgrade' $portable '' 'english'
    if(-not (Test-Path (Join-Path $portable 'portable.mode'))) { throw 'Upgrade lost portable mode' }
    if((Get-FileHash (Join-Path $portable 'ngPost.conf')).Hash -ne $configHash) { throw 'Portable upgrade changed config' }
    Uninstall-Fixture $portable
    $unpacked=Join-Path $evidence 'portable-zip'
    Expand-Archive -LiteralPath (Join-Path $projectRoot 'release\5.1.2-UNSIGNED\ngPost-portable-v5.1.2-UNSIGNED.zip') -DestinationPath $unpacked
    if(Test-Path (Join-Path $unpacked 'ngPost.conf')) { throw 'Personal configuration in ZIP' }
    if(-not (Test-Path (Join-Path $unpacked 'portable.mode'))) { throw 'Portable ZIP marker missing' }
    if((Get-FileHash (Join-Path $unpacked 'ngPost.exe')).Hash -ne $productHash) { throw 'Portable executable differs' }
    if((Get-Item (Join-Path $unpacked 'ngPost.exe')).VersionInfo.FileVersion -ne '5.1.2.0') { throw 'Portable has wrong file version' }
    if(-not (Test-Path (Join-Path $unpacked 'notices\README.txt'))) { throw 'Portable licenses missing' }
    # Without Qt on PATH, the portable CLI must load solely with its packaged runtime.
    $savedPath=$env:PATH
    try {
        $env:PATH="$env:SystemRoot\System32;$env:SystemRoot"
        & (Join-Path $unpacked 'ngPost.exe') --version *> (Join-Path $evidence 'portable-runtime.log')
        if($LASTEXITCODE -ne 0) { throw 'Portable runtime startup failed' }
    } finally { $env:PATH=$savedPath }
    'PASS: fresh English, explicit Dutch, remembered upgrade language, normal/portable marker, config preservation, matching ZIP payload and portable runtime.' | Set-Content (Join-Path $evidence 'result.txt')
    Get-Content (Join-Path $evidence 'result.txt')
    Write-Host "Evidence: $evidence"
} finally {
    foreach($uninstaller in @(Get-ChildItem -LiteralPath $evidence -Filter unins000.exe -Recurse)) { Uninstall-Fixture $uninstaller.DirectoryName }
}
