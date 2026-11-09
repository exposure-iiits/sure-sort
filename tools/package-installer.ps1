param(
    [string]$AppSource = 'dist/Sure Sort 0.1.2',
    [string]$Compiler = '.deps/inno-6.7.4/ISCC.exe'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not [IO.Path]::IsPathRooted($AppSource)) { $AppSource = Join-Path $projectRoot $AppSource }
if (-not [IO.Path]::IsPathRooted($Compiler)) { $Compiler = Join-Path $projectRoot $Compiler }
if (-not (Test-Path -LiteralPath $Compiler)) { throw 'Install the Inno Setup compiler or provide its path with -Compiler.' }
if (Get-ChildItem -LiteralPath $AppSource -Recurse -File | Where-Object { $_.Name -match '\.sqlite($|-)' -or $_.FullName -match '[\\/]data[\\/]' }) {
    throw 'Use a fresh application package without personal session data.'
}
& $Compiler "/DAppSource=$AppSource" (Join-Path $PSScriptRoot 'suresort-setup.iss')
if ($LASTEXITCODE -ne 0) { throw "Installer build failed: $LASTEXITCODE" }
$version = (Get-Item -LiteralPath (Join-Path $AppSource 'Sure Sort.exe')).VersionInfo.FileVersion
$output = Join-Path $projectRoot "dist/Sure-Sort-$version-Setup-Windows-x64.exe"
$hash = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
"$hash  $([IO.Path]::GetFileName($output))" | Set-Content -LiteralPath ($output + '.sha256') -Encoding ascii
Get-Item -LiteralPath $output | Select-Object FullName,Length
