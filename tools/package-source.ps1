param([string]$ZipPath = 'dist/SureSort-source-0.1.2.zip')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not [IO.Path]::IsPathRooted($ZipPath)) { $ZipPath = Join-Path $projectRoot $ZipPath }
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ZipPath) | Out-Null
$files = @(Get-ChildItem -LiteralPath $projectRoot -File -Force | Where-Object {
    $_.Name -in @('.gitignore','CMakeLists.txt','CMakePresets.json','vcpkg.json') -or $_.Extension -eq '.md'
})
foreach ($folder in @('src','tests','tools','cmake','assets','docs','third_party','.github')) {
    $files += Get-ChildItem -LiteralPath (Join-Path $projectRoot $folder) -File -Recurse
}
$archive = [IO.Compression.ZipFile]::Open($ZipPath, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in $files) {
        $entry = 'SureSort/' + $file.FullName.Substring($projectRoot.Length + 1).Replace('\','/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $entry, [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $archive.Dispose() }
Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256 | Format-List
