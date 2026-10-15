param(
    [string]$Version = '1.0.0'
)

$ErrorActionPreference = 'Stop'
$projectDir = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectDir 'build-windows'
$packageDir = Join-Path $buildDir 'package'

if ($env:Qt6_DIR) { $env:PATH = "$env:Qt6_DIR/bin;$env:PATH" }
$null = Get-Command windeployqt.exe -ErrorAction Stop
& "$PSScriptRoot/prepare-windows-runtime.ps1"

cmake -S $projectDir -B $buildDir -G 'Visual Studio 17 2022' -A x64 "-DAPP_VERSION=$Version"
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
cmake --build $buildDir --config Release --parallel --target qsnote
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }

if (Test-Path $packageDir) { Remove-Item $packageDir -Recurse -Force }
cmake --install $buildDir --config Release --prefix $packageDir
if ($LASTEXITCODE -ne 0) { throw 'Install failed' }
$binDir = Join-Path $packageDir 'bin'
windeployqt --release --compiler-runtime "$binDir/qsnote.exe" "$binDir/VTextEdit.dll"
if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed' }

foreach ($file in @(
    'qsnote.exe', 'VTextEdit.dll', 'Qt6Core.dll', 'Qt6WebEngineCore.dll',
    'QtWebEngineProcess.exe', 'platforms/qwindows.dll',
    'resources/icudtl.dat', 'resources/qtwebengine_resources.pak',
    'translations/qtwebengine_locales/en-US.pak',
    'runtime/plantuml/plantuml.jar', 'runtime/graphviz/bin/dot.exe',
    'runtime/manifest.json', 'licenses/vtextedit/COPYING.LESSER'
)) {
    if (!(Test-Path "$binDir/$file")) { throw "Missing package file: $file" }
}

Copy-Item "$projectDir/BUILD.md" "$binDir/BUILD.md"
Compress-Archive -Path "$binDir/*" -DestinationPath "$buildDir/qsnote-$Version-windows-x64.zip" -Force
