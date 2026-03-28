#!/usr/bin/env bash
set -euo pipefail

run_app=0
clean_dist=0

for arg in "$@"; do
    case "$arg" in
        --run)
            run_app=1
            ;;
        --clean)
            clean_dist=1
            ;;
        *)
            echo "Unknown option: $arg" >&2
            echo "Usage: $0 [--clean] [--run]" >&2
            exit 2
            ;;
    esac
done

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="$repo_root/build-linux-qt6"
dist_dir="$repo_root/dist-linux-qt6"
binary="$build_dir/ngPost"
qt_plugin_dir="$(qmake6 -query QT_INSTALL_PLUGINS)"

for tool in qmake6 ldd install cp find awk sed; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing required tool: $tool" >&2
        exit 1
    fi
done

if [[ ! -x "$binary" ]]; then
    echo "Linux build not found yet. Running build-linux-wsl.sh first."
    "$repo_root/build-linux-wsl.sh"
fi

if [[ "$clean_dist" -eq 1 ]]; then
    rm -rf "$dist_dir"
fi

mkdir -p "$dist_dir/lib" "$dist_dir/plugins"

install -m 755 "$binary" "$dist_dir/ngPost"

if [[ -x /usr/bin/rar ]]; then
    install -m 755 /usr/bin/rar "$dist_dir/rar"
fi

if [[ -x /usr/bin/par2 ]]; then
    install -m 755 /usr/bin/par2 "$dist_dir/par2"
fi

for extra_file in ngPost.conf release.md release_notes.txt; do
    if [[ -f "$repo_root/$extra_file" ]]; then
        install -m 644 "$repo_root/$extra_file" "$dist_dir/$extra_file"
    fi
done

if [[ -f "$repo_root/src/resources/icons/ngPost.png" ]]; then
    install -m 644 "$repo_root/src/resources/icons/ngPost.png" "$dist_dir/ngPost.png"
fi

cat > "$dist_dir/qt.conf" <<'EOF'
[Paths]
Plugins = plugins
EOF

cat > "$dist_dir/ngPost.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=ngPost
Exec=ngPost.sh %F
Icon=ngPost
Comment=ngPost local Linux dist
Terminal=false
Categories=Network;
EOF

cat > "$dist_dir/README-local.txt" <<'EOF'
This local Linux dist is intended for your own WSL/Linux testing.

Contents:
- ngPost: Linux Qt binary
- rar: Linux RAR binary copied from /usr/bin/rar
- par2: Linux PAR2 binary copied from /usr/bin/par2
- lib/: local runtime libraries used by ngPost
- plugins/: Qt platform and image plugins
- ngPost.sh: launcher script for WSL/Linux

For public redistribution, review the RAR license first. This folder is a
local convenience runtime built from your current Ubuntu environment.
EOF

cat > "$dist_dir/ngPost.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export LD_LIBRARY_PATH="$script_dir/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$script_dir/plugins"
export DISPLAY="${DISPLAY:-:0}"

if [[ -d /mnt/wslg/runtime-dir ]]; then
    export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
    export PULSE_SERVER="${PULSE_SERVER:-/mnt/wslg/PulseServer}"
    export LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}"
    export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-$HOME/.wslg-runtime}"

    if [[ "${XDG_RUNTIME_DIR%/}" != "/mnt/wslg/runtime-dir" ]]; then
        mkdir -p "$XDG_RUNTIME_DIR"
        chmod 700 "$XDG_RUNTIME_DIR"

        if [[ -e /mnt/wslg/runtime-dir/wayland-0 ]]; then
            ln -sfn /mnt/wslg/runtime-dir/wayland-0 "$XDG_RUNTIME_DIR/wayland-0"
        fi
        if [[ -e /mnt/wslg/runtime-dir/wayland-0.lock ]]; then
            ln -sfn /mnt/wslg/runtime-dir/wayland-0.lock "$XDG_RUNTIME_DIR/wayland-0.lock"
        fi
    fi
fi

exec "$script_dir/ngPost" "$@"
EOF
chmod +x "$dist_dir/ngPost.sh"

plugin_dirs=(
    "platforms"
    "xcbglintegrations"
    "imageformats"
    "iconengines"
    "tls"
    "networkinformation"
    "wayland-decoration-client"
    "wayland-graphics-integration-client"
    "wayland-shell-integration"
)

for dir_name in "${plugin_dirs[@]}"; do
    if [[ -d "$qt_plugin_dir/$dir_name" ]]; then
        mkdir -p "$dist_dir/plugins/$dir_name"
        cp -a "$qt_plugin_dir/$dir_name/." "$dist_dir/plugins/$dir_name/"
    fi
done

should_skip_dep() {
    case "$(basename "$1")" in
        linux-vdso.so.*|ld-linux*.so.*|libc.so.*|libm.so.*|libdl.so.*|libpthread.so.*|librt.so.*|libresolv.so.*|libutil.so.*|libnsl.so.*|libanl.so.*|libBrokenLocale.so.*)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

declare -A seen_deps=()
pending=("$dist_dir/ngPost")

while IFS= read -r plugin_file; do
    pending+=("$plugin_file")
done < <(find "$dist_dir/plugins" -type f -name '*.so')

copy_dep() {
    local dep_path="$1"
    local dep_name
    dep_name="$(basename "$dep_path")"

    if [[ -n "${seen_deps[$dep_name]:-}" ]]; then
        return 0
    fi

    seen_deps[$dep_name]=1
    cp -L "$dep_path" "$dist_dir/lib/$dep_name"
    chmod 755 "$dist_dir/lib/$dep_name" 2>/dev/null || true
    pending+=("$dist_dir/lib/$dep_name")
}

while [[ "${#pending[@]}" -gt 0 ]]; do
    target="${pending[0]}"
    pending=("${pending[@]:1}")

    while IFS= read -r dep_path; do
        [[ -z "$dep_path" ]] && continue
        if should_skip_dep "$dep_path"; then
            continue
        fi
        copy_dep "$dep_path"
    done < <(ldd "$target" | awk '/=> \// {print $3} /^[[:space:]]*\/.* \(/ {print $1}')
done

printf '\nLinux dist ready:\n  %s\n' "$dist_dir"

if [[ "$run_app" -eq 1 ]]; then
    printf 'Launching dist/ngPost.sh. Close the app window to return to the terminal.\n'
    exec "$dist_dir/ngPost.sh"
fi
