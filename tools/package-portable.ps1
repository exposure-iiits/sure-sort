param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [string]$BuildDir = 'build/portable-release',
    [string]$RuntimeBin = '.deps/libraw-build/out',
    [string]$CompilerBin = '',
    [string]$QtNotices = '.deps/qt-notices/qtbase-everywhere-src-6.8.3',
    [string]$Destination = 'dist/Sure Sort',
    [string]$ZipPath = 'dist/Sure-Sort-0.1.3-Windows-x64-Portable.zip'
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
function Resolve-ProjectPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return $Path }
    return Join-Path $repoRoot $Path
}
$Destination = Resolve-ProjectPath $Destination
$ZipPath = Resolve-ProjectPath $ZipPath
if (Test-Path -LiteralPath $Destination) { throw 'Use a fresh destination so no personal session data is packaged.' }
& "$PSScriptRoot/deploy-windows.ps1" -QtRoot $QtRoot -BuildDir $BuildDir -VcpkgBin $RuntimeBin -Destination $Destination -CompilerBin $CompilerBin
"[Portable]`nEnabled=true" | Set-Content -LiteralPath (Join-Path $Destination 'portable.ini') -Encoding utf8
"[Paths]`nPlugins=." | Set-Content -LiteralPath (Join-Path $Destination 'qt.conf') -Encoding ascii
@'
Sure Sort - Windows x64 portable

Extract the entire ZIP to a writable folder, then open Sure Sort.exe.
Keep the executable, DLLs and plugin folders together. No installation is required.
Open Folder includes photos in all nested subfolders.

Settings and session history are created in the data folder beside the app.
To share a fresh copy, send the original ZIP, not your used app folder.
Your photos remain in the locations you choose; photo files are not in this ZIP.

See THIRD_PARTY_NOTICES.md and licenses for dependency licenses and source links.
'@ | Set-Content -LiteralPath (Join-Path $Destination 'READ ME.txt') -Encoding utf8
$noticesPath = Resolve-ProjectPath $QtNotices
if (Test-Path -LiteralPath $noticesPath) {
    Copy-Item -LiteralPath $noticesPath -Destination (Join-Path $Destination 'licenses/Qt-6.8.3') -Recurse
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'assets/suresort.png') -Destination $Destination
Compress-Archive -LiteralPath $Destination -DestinationPath $ZipPath -CompressionLevel Optimal
Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256 | Format-List
Write-Output "Share this ZIP: $ZipPath"
