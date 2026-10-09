#!/usr/bin/env bash
set -euo pipefail

scriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
projectDir="$(cd -- "${scriptDir}/.." && pwd)"

cmake -S "${projectDir}" -B "${projectDir}/build" \
    -DCMAKE_BUILD_TYPE=Release -DAPP_VERSION="${1:-1.0.0}"
cmake --build "${projectDir}/build" --parallel
