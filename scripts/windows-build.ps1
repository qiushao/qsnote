param(
    [string]$Version = '1.0.0'
)

$ErrorActionPreference = 'Stop'
$projectDir = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectDir 'build-windows'
$packageDir = Join-Path $buildDir 'package'
$originalPath = $env:PATH
$originalPluginPath = $env:QT_PLUGIN_PATH
$originalPlatform = $env:QT_QPA_PLATFORM
$originalMessageBoxes = $env:QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES

try {
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

    # The installed application must start without Qt or build libraries on PATH.
    $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
    $env:QT_PLUGIN_PATH = $null
    $env:QT_QPA_PLATFORM = 'offscreen'
    $env:QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES = '1'
    $versionLog = Join-Path $buildDir 'package-version.txt'
    $process = Start-Process -FilePath "$binDir/qsnote.exe" -ArgumentList '--version' `
        -WorkingDirectory $binDir -Wait -PassThru -NoNewWindow -RedirectStandardOutput $versionLog
    if ($process.ExitCode -ne 0) { throw 'Packaged application failed to start' }
    if ((Get-Content $versionLog -Raw).Trim() -ne "qsnote $Version") { throw 'Package version mismatch' }

    Copy-Item "$projectDir/BUILD.md" "$binDir/BUILD.md"
    Compress-Archive -Path "$binDir/*" -DestinationPath "$buildDir/qsnote-$Version-windows-x64.zip" -Force
} finally {
    $env:PATH = $originalPath
    $env:QT_PLUGIN_PATH = $originalPluginPath
    $env:QT_QPA_PLATFORM = $originalPlatform
    $env:QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES = $originalMessageBoxes
}
