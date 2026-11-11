# Sure Sort 0.1.4 validation — 11 November 2026

Built using the existing Windows MinGW 13.1 / Qt 6.8.3 / LibRaw 0.21.4 kit in `build/portable-release`.

- Final full CTest run: **9/9 suites passed**, 0 failures, 53.70 seconds.
- Native Windows footer toggle, animation/navigation and saved-destination cases: **5 passed**, including lifecycle checks, 0 failures.
- New coverage checks keyboard and click removal, green destination outlines after restart, Undo, cancellation during saving without auto-advance, removal retaining the selected photo, correct pre-navigation animation identity, bounded animation lifetime/concurrency and continued keyboard navigation.
- Saved copies remain accessible through footer controls after their shortcut mapping is removed. Existing source-byte, ownership, failure/retry and per-folder recovery tests passed.
- Visually inspected the green folder outlines and compact footer at 640/1280 logical widths, plus the copy-flight snapshot. Captures are in `build/footer-0.1.4-ui`.
- About, `--version`, FileVersion and ProductVersion report **0.1.4**.
- Packaged launch with development DLL/plugin paths removed: passed, exit 0. An isolated app profile and original-folder progress database were created. Packaged executable matches the tested build; no personal session database is included in the package.

App: `dist/Sure Sort 0.1.4/Sure Sort.exe`.
ZIP: `dist/Sure-Sort-0.1.4-Windows-x64-Portable.zip`.
ZIP SHA-256: `49FEBDD92977B3B0B3A66D24E44F7462AD852A62847D9447924E48FF72A06C27`.

This release retains the copy-only progress format. The previous packages remain separate. All automated checks used generated photographs and temporary profiles.
