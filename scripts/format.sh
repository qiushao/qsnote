#!/usr/bin/env bash
set -euo pipefail

scriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd -- "${scriptDir}/.."

find src -type f \( -name '*.h' -o -name '*.cpp' \) -print0 \
    | xargs -0 -r clang-format -i
