param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [string]$BuildDir = 'build/windows-release',
    [string]$Destination = 'dist/Sure Sort',
    [string]$VcpkgBin = 'build/windows-release/vcpkg_installed/x64-windows/bin',
    [string]$CompilerBin = ''
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not [System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $repoRoot $BuildDir }
if (-not [System.IO.Path]::IsPathRooted($Destination)) { $Destination = Join-Path $repoRoot $Destination }
if (-not [System.IO.Path]::IsPathRooted($VcpkgBin)) { $VcpkgBin = Join-Path $repoRoot $VcpkgBin }
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
Copy-Item -LiteralPath (Join-Path $BuildDir 'Sure Sort.exe') -Destination $Destination
Get-ChildItem -LiteralPath $VcpkgBin -Filter '*.dll' | Copy-Item -Destination $Destination
if ($CompilerBin) {
    foreach ($dll in @('Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Sql.dll')) {
        Copy-Item -LiteralPath (Join-Path $QtRoot "bin/$dll") -Destination $Destination
    }
    foreach ($dll in @('libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
        Copy-Item -LiteralPath (Join-Path $CompilerBin $dll) -Destination $Destination
    }
    foreach ($plugin in @('platforms/qwindows.dll','imageformats/qjpeg.dll','imageformats/qico.dll','sqldrivers/qsqlite.dll','styles/qmodernwindowsstyle.dll')) {
        $target = Join-Path $Destination $plugin
        New-Item -ItemType Directory -Force (Split-Path -Parent $target) | Out-Null
        Copy-Item -LiteralPath (Join-Path $QtRoot "plugins/$plugin") -Destination $target
    }
} else {
& (Join-Path $QtRoot 'bin/windeployqt.exe') --qtpaths (Join-Path $QtRoot 'bin/qtpaths.exe') --release --no-translations --no-opengl-sw `
    --skip-plugin-types generic,networkinformation,tls --exclude-plugins qgif,qsqlmimer,qsqlodbc,qsqlpsql `
    --dir $Destination (Join-Path $Destination 'Sure Sort.exe')
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed' }
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'THIRD_PARTY_NOTICES.md') -Destination $Destination
Copy-Item -LiteralPath (Join-Path $repoRoot 'third_party/licenses') -Destination $Destination -Recurse -Force
Write-Output "Portable application: $(Join-Path $Destination 'Sure Sort.exe')"
