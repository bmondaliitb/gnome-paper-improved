#!/bin/bash
set -euo pipefail

if [[ $# -ne 0 ]]; then
    echo 'Usage: ./remove.sh' >&2
    exit 2
fi
: "${HOME:?HOME must be set}"

apps="$HOME/.local/share/applications"
config="${XDG_CONFIG_HOME:-$HOME/.config}/papers-bold"
rm -f -- "$HOME/.local/bin/papers-bold" \
    "$apps/papers-bold.desktop" \
    "$HOME/.local/src/papers-bold/ft-pdf-bold.c" \
    "$HOME/.local/lib/papers-bold/ft-pdf-bold.so" \
    "$config/weight"

# Remove empty installation directories, preserving any extra user files.
for dir in "$HOME/.local/src/papers-bold" "$HOME/.local/lib/papers-bold" "$config"; do
    if [[ -d "$dir" && ! -L "$dir" ]]; then
        if ! rmdir -- "$dir"; then
            echo "Kept directory: $dir" >&2
        fi
    fi
done

if [[ -d "$apps" ]] && command -v update-desktop-database >/dev/null; then
    update-desktop-database "$apps"
fi
echo 'Removed Papers enhanced text. Close and reopen Papers to use normal rendering.'
