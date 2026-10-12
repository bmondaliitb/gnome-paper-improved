#!/bin/bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
src="$HOME/.local/src/papers-bold"
lib="$HOME/.local/lib/papers-bold"
bin="$HOME/.local/bin"
apps="$HOME/.local/share/applications"
config="${XDG_CONFIG_HOME:-$HOME/.config}/papers-bold"
reuse=false
weight=''

usage() {
    echo 'Usage: ./install.sh [--weight 0..256] [--reuse-existing]'
    echo 'Default weight: saved setting, or 24 on first installation.'
}
while [[ $# -gt 0 ]]; do
    case "$1" in
    --weight)
        [[ $# -ge 2 ]] || { usage >&2; exit 2; }
        weight=$2
        [[ "$weight" =~ ^[0-9]{1,3}$ ]] && (( 10#$weight <= 256 )) || {
            echo 'Weight must be an integer from 0 to 256.' >&2; exit 2;
        }
        weight=$((10#$weight))
        shift 2
        ;;
    --reuse-existing)
        reuse=true
        shift
        ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
    esac
done
if [[ -z "$weight" ]]; then
    weight=24
    if [[ -f "$config/weight" ]]; then
        weight=$(cat "$config/weight")
    fi
fi
[[ "$weight" =~ ^[0-9]{1,3}$ ]] && (( 10#$weight <= 256 )) || {
    echo 'Invalid saved weight. Use --weight with an integer from 0 to 256.' >&2; exit 2;
}
weight=$((10#$weight))
if [[ "$reuse" == true ]]; then
    [[ -r "$lib/ft-pdf-bold.so" ]] || { echo 'No existing shim to reuse.' >&2; exit 1; }
else
        if ! command -v gcc >/dev/null || ! command -v pkg-config >/dev/null || ! pkg-config --exists freetype2; then
            if [[ -r "$lib/ft-pdf-bold.so" ]]; then
                echo 'Build tools unavailable; reusing the installed shim.'
                reuse=true
            else
                echo 'Install GCC, pkg-config, and FreeType headers first.' >&2
                echo 'Ubuntu/Debian: sudo apt install build-essential pkg-config libfreetype-dev' >&2
                exit 1
            fi
        fi
fi
[[ -x /usr/bin/papers ]] || { echo '/usr/bin/papers is missing.' >&2; exit 1; }
mkdir -p "$src" "$lib" "$bin" "$apps" "$config"
if [[ "$reuse" == false ]]; then
    build=$(mktemp -d)
    trap 'rm -rf -- "$build"' EXIT
    gcc -shared -fPIC $(pkg-config --cflags freetype2) \
        "$repo/ft-pdf-bold.c" -o "$build/ft-pdf-bold.so" -ldl -lfreetype -pthread
    echo 'Compiled a new font shim.'
    install -m 755 "$build/ft-pdf-bold.so" "$lib/ft-pdf-bold.so"
fi
install -m 644 "$repo/ft-pdf-bold.c" "$src/ft-pdf-bold.c"
install -m 755 "$repo/papers-bold" "$bin/papers-bold"
printf '%s\n' "$weight" > "$config/weight"
# Escape characters special inside a quoted Desktop Entry Exec argument.
exec_path="$bin/papers-bold"
exec_path=${exec_path//\\/\\\\\\\\}
exec_path=${exec_path//\"/\\\"}
exec_path=${exec_path//\$/\\\$}
exec_path=${exec_path//\`/\\\`}
exec_path=${exec_path//%/%%}
cat > "$apps/papers-bold.desktop" <<EOF
[Desktop Entry]
Name=Document Viewer — Enhanced Text
Comment=View PDFs with enhanced text rendering
Exec="$exec_path" %U
Icon=org.gnome.Papers
Terminal=false
Type=Application
DBusActivatable=false
MimeType=application/pdf;
Categories=GNOME;GTK;Office;Viewer;
StartupNotify=true
EOF
if command -v update-desktop-database >/dev/null; then
    update-desktop-database "$apps"
fi
echo "Installed with font weight $weight (saved in $config/weight)."
echo 'Close Papers, then open Document Viewer — Enhanced Text.'
