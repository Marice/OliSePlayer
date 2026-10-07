#!/usr/bin/env bash
# Fetches the MilkDrop preset pack and its textures into presets/.
#
# These are not part of this repository: they are the presets published with
# the last official MilkDrop release, kept by the projectM project, and their
# licence is not stated. `make gl` picks them up automatically once they are
# here, and skips the preset folder when they are not.
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
target="$root/presets"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

presets_url=https://github.com/projectM-visualizer/presets-milkdrop-original/archive/refs/heads/master.tar.gz
textures_url=https://github.com/projectM-visualizer/presets-milkdrop-texture-pack/archive/refs/heads/master.tar.gz

printf '==> [presets] Downloading the original MilkDrop preset pack\n'
curl -fsSL --max-time 300 -o "$work/presets.tar.gz" "$presets_url"
tar xzf "$work/presets.tar.gz" -C "$work"

printf '==> [presets] Downloading the shared texture pack\n'
curl -fsSL --max-time 300 -o "$work/textures.tar.gz" "$textures_url"
tar xzf "$work/textures.tar.gz" -C "$work"

mkdir -p "$target/textures"
find "$work" -name '*.milk' -exec cp -f {} "$target/" \;
find "$work" -type f \( -name '*.jpg' -o -name '*.png' -o -name '*.dds' -o -name '*.tga' \) \
    -path '*texture*' -exec cp -f {} "$target/textures/" \;

presets=$(find "$target" -maxdepth 1 -name '*.milk' | wc -l)
textures=$(find "$target/textures" -type f | wc -l)
printf '==> [presets] %s preset(s) and %s texture(s) in presets/\n' "$presets" "$textures"
[ "$presets" -gt 0 ] || { echo "no presets were fetched" >&2; exit 1; }
