#!/usr/bin/env bash
set -euo pipefail

run_app=0
clean_build=0

for arg in "$@"; do
    case "$arg" in
        --run)
            run_app=1
            ;;
        --clean)
            clean_build=1
            ;;
        *)
            echo "Unknown option: $arg" >&2
            echo "Usage: $0 [--clean] [--run]" >&2
            exit 2
            ;;
    esac
done

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_file="$repo_root/src/ngPost.pro"
build_dir="$repo_root/build-linux-qt6"

if [[ ! -f "$project_file" ]]; then
    echo "Project file not found: $project_file" >&2
    exit 1
fi

for tool in qmake6 make; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing required tool: $tool" >&2
        exit 1
    fi
done

if [[ "$clean_build" -eq 1 ]]; then
    rm -rf "$build_dir"
fi

mkdir -p "$build_dir"
cd "$build_dir"

qmake6 ../src/ngPost.pro CONFIG+=release
make -j"$(nproc)"

printf '\nLinux build ready:\n  %s\n' "$build_dir/ngPost"

if [[ "$run_app" -eq 1 ]]; then
    export DISPLAY="${DISPLAY:-:0}"
    export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
    export PULSE_SERVER="${PULSE_SERVER:-/mnt/wslg/PulseServer}"
    export LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}"
    export XDG_RUNTIME_DIR="$HOME/.wslg-runtime"

    mkdir -p "$XDG_RUNTIME_DIR"
    chmod 700 "$XDG_RUNTIME_DIR"
    ln -sfn /mnt/wslg/runtime-dir/wayland-0 "$XDG_RUNTIME_DIR/wayland-0"
    ln -sfn /mnt/wslg/runtime-dir/wayland-0.lock "$XDG_RUNTIME_DIR/wayland-0.lock"

    printf 'Launching ngPost. Close the app window to return to the terminal.\n'
    exec ./ngPost
fi
