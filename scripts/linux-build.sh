#!/usr/bin/env bash
set -euo pipefail

scriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
projectDir="$(cd -- "${scriptDir}/.." && pwd)"

if [[ -n "${1:-}" ]]; then
    version="$1"
elif version="$(git -C "$projectDir" describe --tags --abbrev=0 2>/dev/null)"; then
    version="${version#[vV]}"
else
    version="1.0.0"
fi
echo "Building version: $version"

cmake -S "${projectDir}" -B "${projectDir}/build" \
    -DCMAKE_BUILD_TYPE=Release -DAPP_VERSION="$version"
cmake --build "${projectDir}/build" --parallel --target qsnote
cpack --config "${projectDir}/build/CPackConfig.cmake" -B "${projectDir}/build"
