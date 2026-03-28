param(
    [string]$Distro = "Ubuntu-24.04",
    [switch]$Run,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Split-Path -Parent $MyInvocation.MyCommand.Path)).Path
$repoName = Split-Path $root -Leaf

function Invoke-WslBash {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command
    )

    & wsl.exe -d $Distro -- bash -lc $Command
    if ($LASTEXITCODE -ne 0) {
        throw "WSL command failed with exit code $LASTEXITCODE"
    }
}

$linuxHome = (& wsl.exe -d $Distro -- bash -lc 'printf %s "$HOME"').Trim()
if (!$linuxHome) {
    throw "Could not determine the Linux home directory in distro '$Distro'."
}

$linuxSource = (& wsl.exe -d $Distro -- bash -lc "wslpath -a '$root'").Trim()
if (!$linuxSource) {
    throw "Could not convert Windows repo path to a Linux path."
}

$linuxRepo = "$linuxHome/src/$repoName"

$syncCommand = @"
mkdir -p '$linuxHome/src'
rsync -a --delete \
  --exclude '.git' \
  --exclude '.qt' \
  --exclude '.tools' \
  --exclude 'build-qt6' \
  --exclude 'dist-qt6' \
  --exclude 'build-linux-qt6' \
  --exclude 'dist-linux-qt6' \
  '$linuxSource/' '$linuxRepo/'
chmod +x '$linuxRepo/build-linux-wsl.sh'
chmod +x '$linuxRepo/package-linux-local.sh'
"@

Invoke-WslBash -Command $syncCommand

$args = @()
if ($Clean) {
    $args += "--clean"
}
if ($Run) {
    $args += "--run"
}

$argString = if ($args.Count -gt 0) { " " + ($args -join " ") } else { "" }
$packageCommand = "cd '$linuxRepo' && ./package-linux-local.sh$argString"

Invoke-WslBash -Command $packageCommand
