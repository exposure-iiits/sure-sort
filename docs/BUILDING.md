# Building Sure Sort

## Windows prerequisites (MSVC 2022 x64)

1. Install Visual Studio 2022 Build Tools with MSVC v143 x64 tools and Windows SDK, CMake ≥3.24, Ninja and Git. In PowerShell:

   ```powershell
   winget install --exact --id Microsoft.VisualStudio.2022.BuildTools --override '--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
   winget install --exact --id Kitware.CMake
   winget install --exact --id Ninja-build.Ninja
   winget install --exact --id Git.Git
   ```

2. Download and run the [official Qt online installer](https://www.qt.io/download-qt-installer-oss). Select **Qt 6.8.3 → MSVC 2022 64-bit**, Qt base/Widgets/SQL, the SQLite driver and Qt Test development files. No WebEngine is required. Another available 6.8 LTS patch can be used; adjust `QT_ROOT` below. Later LTS patches may require a subscription; use open-source 6.8.3 if needed.

3. Start an **x64 Native Tools Command Prompt for VS 2022**, then run `powershell -NoExit` so `cl.exe` is configured. In that PowerShell:

   ```powershell
   $env:QT_ROOT = 'C:\Qt\6.8.3\msvc2022_64'
   $env:VCPKG_ROOT = 'C:\dev\vcpkg'
   git clone https://github.com/microsoft/vcpkg.git $env:VCPKG_ROOT
   & "$env:VCPKG_ROOT\bootstrap-vcpkg.bat" -disableMetrics
   Set-Location C:\Users\admin\Desktop\sure-sort
   cmake --preset windows-release
   cmake --build --preset windows-release --parallel 4
   ```

`vcpkg.json` pins a registry baseline and requires LibRaw ≥0.21.4. CMake installs it in manifest mode with `x64-windows` (dynamic linkage). Qt comes from the official kit. `libraw::raw_r` is selected from vcpkg; a system-library fallback is available on other platforms.

## Run, test, benchmark

```powershell
$env:PATH = "$env:QT_ROOT\bin;$PWD\build\windows-release\vcpkg_installed\x64-windows\bin;" + $env:PATH
& '.\build\windows-release\Sure Sort.exe' 'D:\Photos\Shoot' --cache-mb 256
ctest --preset windows-release
& .\build\windows-release\photocull_bench.exe
& .\build\windows-release\photocull_bench.exe 10000
```

Qt Test generates JPEG EXIF and DNG fixtures; no camera images are downloaded. The release benchmark creates a synthetic 24 MP JPEG and a 1,000-entry scan catalog, reporting scan/decode/cache timing. It does not measure full keyboard-to-display latency. See [performance](PERFORMANCE.md) and the [manual checklist](MANUAL_TESTS.md).

In restricted runners, point `TEMP` and `TMP` to a writable directory before testing. CTest uses the offscreen plugin. For native Windows UI/DPI checks:

```powershell
$env:QT_QPA_PLATFORM = 'windows'
& .\build\windows-release\photocull_tests.exe viewerAndWindowSmoke
Remove-Item Env:QT_QPA_PLATFORM
```

## Deploy with windeployqt

From the configured build shell, using a fresh destination:

```powershell
cmake --install build/windows-release --prefix 'dist/Sure Sort'
Get-ChildItem build/windows-release/vcpkg_installed/x64-windows/bin -Filter '*.dll' | Copy-Item -Destination 'dist/Sure Sort'
& "$env:QT_ROOT\bin\windeployqt.exe" --release --no-translations --no-opengl-sw --skip-plugin-types generic,networkinformation,tls --exclude-plugins qgif,qico,qsqlmimer,qsqlodbc,qsqlpsql --dir 'dist/Sure Sort' 'dist/Sure Sort/Sure Sort.exe'
```

Or use the helper, which also copies included license texts:

```powershell
.\tools\deploy-windows.ps1 -QtRoot $env:QT_ROOT
```

Keep the matching x64 MSVC runtime deployed by `windeployqt`, or install Microsoft's Visual C++ Redistributable 2015–2022 x64. Copy vcpkg `share/*/copyright` files for all shipped DLLs and the notices from the actual Qt kit; see [third-party notices](../THIRD_PARTY_NOTICES.md). Test the complete folder on a clean Windows machine without Qt or vcpkg on PATH before shipping. The application itself makes no network requests.

## Linux build

Install Qt **6.8 or newer**, CMake ≥3.24, Ninja, a C++17 compiler, and dynamic LibRaw ≥0.21 development packages. Older distro Qt packages may not qualify. Set `CMAKE_PREFIX_PATH` for a separate Qt kit if needed:

```sh
cmake --preset linux-release
cmake --build --preset linux-release --parallel 4
ctest --preset linux-release
"./build/linux-release/Sure Sort" /path/to/photos
./build/linux-release/photocull_bench
```

Linux is untested here; its Qt kit needs SQL/SQLite support. See [research](RESEARCH.md) and [architecture](DESIGN.md). Preview decoding remains separate from full-sensor processing.
