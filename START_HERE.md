# Sure Sort — source and Windows app

Current application version: **0.1.4**.

## Work from this project

The `SureSort` folder inside `SureSort-source.zip` is the complete source project for future work. Open this folder in your editor. Keep one working checkout and use it for subsequent changes.

It contains the C++/Qt sources, generated test-fixture code, CMake configuration, build/deployment tools, documentation and third-party license notices. Qt, LibRaw, compiler toolchains and other generated dependencies are installed separately; see the [Windows build instructions](README.md#windows-prerequisites-msvc-2022-x64) and [development guide](docs/DEVELOPMENT.md).

From a configured development shell:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release --parallel 4
ctest --preset windows-release
```

## Put the source on GitHub

Extract `SureSort-source.zip` and upload the **contents of the SureSort folder** to the repository root. `CMakeLists.txt`, `README.md`, `src/` and `tests/` should appear directly at the repository root. The ZIP itself is a transport file, not the editable source layout GitHub should display.

If using Git, create an empty GitHub repository and run the following from the extracted source folder, replacing the placeholder with your repository URL:

```powershell
git init -b main
git add .
git commit -m "Import Sure Sort 0.1.4"
git remote add origin YOUR_GITHUB_REPOSITORY_URL
git push -u origin main
```

If your repository already has commits, copy these source files into its existing checkout and commit there to preserve its history. The supplied `.gitignore` excludes build outputs, dependency kits, app packages and local session databases.

## Run or share the Windows app

Run `dist/Sure Sort 0.1.4 Genie/Sure Sort.exe`, or extract `dist/Sure-Sort-0.1.4-Windows-x64-Genie.zip` and run the executable inside. This is the latest local build with footer copy toggles and animation. Keep all adjacent DLLs and plugin folders with it. No compiler or SDK is needed. Original photos remain unchanged and each original folder saves its session in `.suresort-progress.sqlite`; earlier Move sessions are not imported. The previous Completion package is retained separately.

Future build packages can be created with [tools/deploy-windows.ps1](tools/deploy-windows.ps1). Version changes are made only when intended, in `project(PhotoCull VERSION ...)` in `CMakeLists.txt`; [versioning](docs/VERSIONING.md) describes how About and `--version` receive it.
