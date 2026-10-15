#!/usr/bin/env bash
set -euo pipefail

scriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
projectDir="$(cd -- "${scriptDir}/.." && pwd)"
buildDir="${projectDir}/build-macos"
packageDir="${buildDir}/package"
version="${1:-1.0.0}"

if [[ -n "${Qt6_DIR:-}" ]]; then
    export PATH="${Qt6_DIR}/bin:${PATH}"
fi
command -v macdeployqt >/dev/null

cmake -S "$projectDir" -B "$buildDir" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DAPP_VERSION="$version"
cmake --build "$buildDir" --parallel --target qsnote

# Always deploy into a fresh install tree; keep the build bundle reusable.
rm -rf "$packageDir"
cmake --install "$buildDir" --config Release --prefix "$packageDir"
cd "$packageDir"
# macdeployqt resolves @rpath deps only against the bundle's own rpaths, so
# stage VTextEdit into Frameworks before deploying Qt.
mkdir -p qsnote.app/Contents/Frameworks
cp -a lib/libVTextEdit*.dylib qsnote.app/Contents/Frameworks/
macdeployqt qsnote.app -always-overwrite -dmg
test -f qsnote.app/Contents/Resources/licenses/vtextedit/COPYING.LESSER
test -d qsnote.app/Contents/Frameworks/QtWebEngineCore.framework
test -f qsnote.app/Contents/Resources/qsnote.icns

for binary in qsnote.app/Contents/MacOS/qsnote qsnote.app/Contents/Frameworks/libVTextEdit*.dylib; do
    otool -L "$binary" | awk '
        NR > 1 && $1 ~ /^\// && $1 !~ /^\/(System\/Library|usr\/lib)\// { print; failed = 1 }
        END { exit failed }
    '
done

mv qsnote.dmg "${buildDir}/qsnote-macos-$(uname -m).dmg"
