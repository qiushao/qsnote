#!/usr/bin/env bash
set -euo pipefail

scriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
output="$1"
iconset="${output%.icns}.iconset"
mkdir -p "$iconset"

for size in 16 32 128 256 512; do
    sips -z "$size" "$size" "${scriptDir}/../src/resources/icons/qsnote.png" \
        --out "${iconset}/icon_${size}x${size}.png" >/dev/null
    sips -z "$((size * 2))" "$((size * 2))" "${scriptDir}/../src/resources/icons/qsnote.png" \
        --out "${iconset}/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$iconset" -o "$output"
rm -rf "$iconset"
