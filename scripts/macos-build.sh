#!/usr/bin/env bash
set -euo pipefail

scriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
projectDir="$(cd -- "${scriptDir}/.." && pwd)"
buildDir="${projectDir}/build-macos"
packageDir="${buildDir}/package"
version="${1:-1.0.0}"
arch="$(uname -m)"

if [[ -n "${Qt6_DIR:-}" ]]; then
    export PATH="${Qt6_DIR}/bin:${PATH}"
fi
command -v macdeployqt >/dev/null

cmake -S "$projectDir" -B "$buildDir" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DCMAKE_OSX_ARCHITECTURES="$arch" -DAPP_VERSION="$version"
cmake --build "$buildDir" --parallel --target qsnote

# Always deploy into a fresh install tree; keep the build bundle reusable.
rm -rf "$packageDir"
cmake --install "$buildDir" --config Release --prefix "$packageDir"
cd "$packageDir"
# macdeployqt resolves @rpath deps only against the bundle's own rpaths, so
# stage VTextEdit into Frameworks before deploying Qt.
mkdir -p qsnote.app/Contents/Frameworks
cp -a lib/libVTextEdit*.dylib qsnote.app/Contents/Frameworks/
# Deploy plugins explicitly. macdeployqt's automatic plugin deployment picks up
# every installed plugin, including qtvirtualkeyboard's input context, whose
# @rpath frameworks it cannot resolve with Homebrew's per-module Qt prefixes.
qtPluginDir="$(qtpaths6 --query QT_INSTALL_PLUGINS 2>/dev/null || qtpaths --query QT_INSTALL_PLUGINS)"
mkdir -p qsnote.app/Contents/PlugIns
for pluginGroup in platforms styles iconengines imageformats tls networkinformation position; do
    if [[ -d "${qtPluginDir}/${pluginGroup}" ]]; then
        cp -RL "${qtPluginDir}/${pluginGroup}" qsnote.app/Contents/PlugIns/
    fi
done
test -f qsnote.app/Contents/PlugIns/platforms/libqcocoa.dylib
pluginArgs=()
while IFS= read -r -d '' plugin; do
    pluginArgs+=("-executable=${plugin}")
done < <(find qsnote.app/Contents/PlugIns -name '*.dylib' -print0)
macdeployqt qsnote.app -always-overwrite -no-plugins "${pluginArgs[@]}"
# macdeployqt only writes qt.conf when it deploys plugins itself.
cat > qsnote.app/Contents/Resources/qt.conf <<'EOF'
[Paths]
Plugins = PlugIns
EOF

isMachO() { file -b "$1" | grep -q 'Mach-O'; }
# Dependencies that are not part of the OS and must be bundled.
externalDeps() {
    local id
    id="$(otool -D "$1" | sed -n 2p)"
    otool -L "$1" | awk -v id="$id" 'NR > 1 && $1 != id && $1 ~ /^\// && $1 !~ /^\/(System\/Library|usr\/lib)\// { print $1 }'
}

# macdeployqt leaves helpers such as QtWebEngineProcess linked against absolute
# Homebrew paths; redirect every such dependency to the bundled copy.
while IFS= read -r -d '' binary; do
    isMachO "$binary" || continue
    for dep in $(externalDeps "$binary"); do
        if [[ "$dep" == *.framework/* ]]; then
            target="$(sed -E 's|^.*/([^/]+\.framework/.*)$|\1|' <<<"$dep")"
        else
            target="$(basename "$dep")"
        fi
        test -e "qsnote.app/Contents/Frameworks/${target}" || { echo "Missing bundled dependency ${target} for ${binary}" >&2; exit 1; }
        # Existing rpaths of bundled binaries already point at Contents/Frameworks.
        install_name_tool -change "$dep" "@rpath/${target}" "$binary" 2>/dev/null
    done
done < <(find qsnote.app -type f -print0)

test -f qsnote.app/Contents/Resources/licenses/vtextedit/COPYING.LESSER
test -d qsnote.app/Contents/Frameworks/QtWebEngineCore.framework
test -f qsnote.app/Contents/Resources/qsnote.icns

while IFS= read -r -d '' binary; do
    isMachO "$binary" || continue
    lipo "$binary" -verify_arch "$arch"
    deps="$(externalDeps "$binary")"
    if [[ -n "$deps" ]]; then
        echo "Unbundled dependencies in ${binary}:" >&2
        echo "$deps" >&2
        exit 1
    fi
    for dep in $(otool -L "$binary" | awk 'NR > 1 && $1 ~ /^@rpath\// { print $1 }'); do
        test -e "qsnote.app/Contents/Frameworks/${dep#@rpath/}" || { echo "Unresolved ${dep} in ${binary}" >&2; exit 1; }
    done
done < <(find qsnote.app -type f -print0)

# install_name_tool invalidates code signatures; Apple Silicon refuses to run
# unsigned code, so re-sign the whole bundle ad hoc.
codesign --force --deep --sign - qsnote.app
codesign --verify --deep --strict qsnote.app

# Build the DMG with an Applications link so the app can be dragged to install.
dmgRoot="${buildDir}/dmg-root"
dmgPath="${buildDir}/qsnote-macos-${arch}.dmg"
rm -rf "$dmgRoot" "$dmgPath"
mkdir -p "$dmgRoot"
cp -a qsnote.app "$dmgRoot/"
ln -s /Applications "$dmgRoot/Applications"
hdiutil create -volname "QSNote" -srcfolder "$dmgRoot" -ov -format UDZO "$dmgPath"
rm -rf "$dmgRoot"
hdiutil verify "$dmgPath"
