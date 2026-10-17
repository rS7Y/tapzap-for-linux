#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
prototype_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
source_dir="$(cd -- "${1:?Usage: build-tapzap.sh PINNED_SOURCE/src/sway OUTPUT}" && pwd)"
output_dir="${2:?Specify an output directory}"
mkdir -p "$output_dir"
output_dir="$(cd -- "$output_dir" && pwd)"
case "$output_dir/" in
    "$source_dir/"*) echo "Use an output directory outside the pinned source" >&2; exit 2 ;;
esac
# Build from a separate copy so the prototype's autostart entry cannot replace
# an existing TAP ZAP installation's entry. Keep the byte-pinned input intact.
staged_source="$output_dir/source"
if [[ -e "$staged_source" ]]; then
    echo "Use a fresh output directory for a reproducible prototype build" >&2
    exit 2
fi
mkdir "$staged_source"
cp -R "$source_dir/." "$staged_source/"
patch --batch "$staged_source/platform.hpp" "$prototype_dir/tapzap-platform.patch"
patch --batch "$staged_source/ipc.hpp" "$prototype_dir/tapzap-ipc.patch"
source_dir="$staged_source"
compiler="${CXX:-c++}"
patch --batch --output="$output_dir/ui_cairo_cosmic.cpp" \
    "$source_dir/ui_cairo.cpp" "$prototype_dir/tapzap-ui.patch"
read -r -a gui_flags <<< "$(pkg-config --cflags --libs x11 xrandr xext xfixes cairo freetype2)"
"$compiler" -std=c++17 -O2 -Wall -Wextra -I "$source_dir" \
    -DHAVE_CAIRO -DHAVE_XRANDR -DHAVE_XFIXES \
    "$source_dir/main.cpp" "$output_dir/ui_cairo_cosmic.cpp" "$prototype_dir/gamma_cosmic.cpp" \
    -o "$output_dir/tapzap-cosmic" "${gui_flags[@]}"
"$compiler" -std=c++17 -O2 -Wall -Wextra -I "$source_dir" \
    "$prototype_dir/bridge_driver.cpp" "$prototype_dir/gamma_cosmic.cpp" \
    -o "$output_dir/tapzap-cosmic-driver"
"$output_dir/tapzap-cosmic" selftest
