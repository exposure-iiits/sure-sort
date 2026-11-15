# Sure Sort 0.1.7 validation

Validated on 15 November 2026 on Windows 11 with Qt 6.8.3, MinGW GCC 13.1 and LibRaw 0.21.4.

- Release build passed. CMake/generated application version, Windows executable properties, installer metadata and current package references use 0.1.7.
- All nine automated suites passed in the final serial run: folder scanner, preview/core, sorting, watermark, editing, output, workflow, viewer tools and folder progress (110.95 seconds total).
- Native Windows checks passed for the settings window/shortcuts panel, shortcut customization and persistence, fresh-folder reuse/sibling output, and footer removal without losing files or copy records. Tests use isolated profiles and generated photo fixtures.
- Narrow-window checks cover simultaneous information/shortcut panels, long modifier combinations, dynamic row heights and the absence of horizontal overflow. Fallback-font checks exposed a minimum-width issue; wrapping the panel heading and allowing tabs to shrink fixed it.
- Folder coverage verifies that an existing sibling `_sorted` directory retains its files, destination names can be reused or replaced with a fresh list, removed buttons stay removed after restart, and saved copy ownership remains available.
- The portable ZIP contains the final executable byte-for-byte and no personal databases or `data` directory. Both package SHA-256 files match their final downloads. The packaged application launched with bundled dependencies and an isolated profile and exited successfully.
- Inno Setup 6.7.4 compiled the Windows x64 installer. Its input excludes `portable.ini`, personal databases and local session data, keeping installed and portable profile locations distinct.
- Startup update prompts are documented as planned work only. This release adds no update checker, downloader or automatic installer execution.

No new macOS binary, Windows 10/MSVC verification or clean-machine installer test was performed for this release.
