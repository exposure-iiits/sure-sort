# Version 0.1.1 verification — 4 November 2026

- Release build: Qt 6.8.3, MinGW GCC 13.1, LibRaw 0.21.4, Windows x64.
- Final CTest run: 7/7 suites passed, 0 failed, 38.53 seconds. Covers import, preview/cache/scheduling, Copy/Move, editing, watermarking, output settings, ownership checks, restart recovery and UI workflows.
- Workflow suite: 50 results passed, including four same-name export cases (Copy/Move, original/edited), chained Move/rename/Undo and interrupted directory rename before/after filesystem change.
- Native Windows checks: shortcut editing/persistence, quick folder creation, Reset edits, double-click fullscreen and top-edge header reveal passed (4 Qt Test results including setup/cleanup). Native widget captures inspected for header, footer and categorized shortcut layout.
- Packaged executable: isolated-session launch with included dependencies exited successfully. FileVersion and ProductVersion both 0.1.1. Portable ZIP checked to contain zero personal session/database entries.
- Performance observations and limits are recorded in PERFORMANCE.md. These synthetic measurements do not establish real-camera throughput, color quality, Windows 10 behavior or multi-monitor DPI behavior.

## UI refinements — 9 November 2026

- Application version remains 0.1.1; session schema remains 7. Existing Copy/Move mappings and quarter-turn edits are preserved.
- New folders default to Move and the next available shortcut, checking alternate keys and application/editing conflicts. Footer creation and rename use an inline name field with Enter/Save and Esc/Cancel.
- Settings and Shortcuts are embedded in the main window. Shortcut editing stays in its panel, categories have a selected style, and the Editing shortcuts button selects Editing directly. The header uses text branding. Processing details expand inline.
- Rotation uses a −180° to +180° slider with one-degree steps. Regression cases cover nine positive/negative angles, preview/output agreement, smooth rotation, free-angle crop, PNG output and saved per-photo/task parameters across restart. Legacy JSON without the new optional fields still loads.
- Complete CTest run: 7/7 suites passed, 0 failures, 28.44 seconds. After the final spacing adjustment, the full workflow suite passed again in 27.48 seconds.
- Native Windows checks passed for inline folder creation/cancel, defaults, Settings, shortcut editing/conflict/cancel/persistence, direct Editing category selection, crop/undo and the edit panel. Final native rerun: 4 results passed including setup/cleanup. Captures in `build/ui-refinements` were visually inspected.
- Packaged app launched with only Windows directories on PATH and an isolated database: exit 0, SQLite schema 7 initialized. The portable ZIP contains zero session/database entries. App FileVersion and ProductVersion are 0.1.1; the installer reports the same version.
- Final installer: `dist/Sure-Sort-0.1.1-Setup-Windows-x64.exe`, SHA-256 `C3BB65B660BC0224BD4E3A214FF4B181EE31D894A04467620C46BA18431F7D30`.
- Portable ZIP: `dist/Sure-Sort-0.1.1-Windows-x64-Updated-Portable.zip`, SHA-256 `83217688FEEAEDDF0468D228C3AB173D4375ACE94A9269E49FE9F156C6E9D5AE`.
