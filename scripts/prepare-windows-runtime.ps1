param(
    [string]$OutputDir = (Join-Path $PSScriptRoot '../build-runtime/windows')
)

$ErrorActionPreference = 'Stop'
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$cache = Join-Path (Split-Path $OutputDir -Parent) 'downloads'
New-Item -ItemType Directory -Force $cache, $OutputDir | Out-Null
$manifestPath = Join-Path $PSScriptRoot 'windows-runtime.json'
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$java = (Get-Command java.exe -ErrorAction SilentlyContinue).Source
if (!$java -and $env:JAVA_HOME -and (Test-Path "$env:JAVA_HOME/bin/java.exe")) {
    $java = Join-Path $env:JAVA_HOME 'bin/java.exe'
}
if (!$java) { throw 'Install Java and add java to PATH or set JAVA_HOME. Java is not bundled with QSNote.' }

foreach ($name in 'graphviz', 'plantuml') {
    $package = $manifest.$name
    $extension = if ($name -eq 'plantuml') { 'jar' } else { 'zip' }
    $archive = Join-Path $cache "$name-$($package.sha256.Substring(0, 12)).$extension"
    if (!(Test-Path -LiteralPath $archive)) {
        & curl.exe --fail --location --retry 3 --output "$archive.download" $package.url
        if ($LASTEXITCODE -ne 0) { throw "Download failed: $name" }
        if ((Get-FileHash "$archive.download" -Algorithm SHA256).Hash -ne $package.sha256) {
            throw "SHA256 mismatch: $name"
        }
        Move-Item -LiteralPath "$archive.download" -Destination $archive
    }
    if ((Get-FileHash $archive -Algorithm SHA256).Hash -ne $package.sha256) {
        throw "SHA256 mismatch in cache: $archive"
    }
    $destination = Join-Path $OutputDir $name
    New-Item -ItemType Directory -Force $destination | Out-Null
    if ($name -eq 'plantuml') {
        Copy-Item -LiteralPath $archive -Destination (Join-Path $destination 'plantuml.jar')
    } else {
        $expanded = Join-Path $cache "$name-$($package.sha256.Substring(0, 12))"
        Expand-Archive -LiteralPath $archive -DestinationPath $expanded -Force
        $binary = @(Get-ChildItem $expanded -Recurse -File -Filter 'dot.exe')
        if ($binary.Count -ne 1) { throw "Expected one dot.exe in $archive" }
        $root = Split-Path (Split-Path $binary[0].FullName -Parent) -Parent
        # Keep the complete runtime, including DLLs, plugins, fonts and licenses.
        Copy-Item -Path (Join-Path $root '*') -Destination $destination -Recurse -Force
    }
    Write-Output "Prepared $name $($package.version)"
}

& "$OutputDir/graphviz/bin/dot.exe" -c
if ($LASTEXITCODE -ne 0) { throw 'Graphviz plugin registration failed' }
$license = & $java '-Djava.awt.headless=true' -jar "$OutputDir/plantuml/plantuml.jar" -license
if ($LASTEXITCODE -ne 0) { throw 'PlantUML runtime verification failed' }
$license | Set-Content -Encoding utf8 (Join-Path $OutputDir 'plantuml/LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../packaging/licenses/graphviz-COPYING') -Destination (Join-Path $OutputDir 'graphviz/COPYING')
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $OutputDir 'manifest.json')
Write-Output "Windows diagram runtime: $OutputDir"
