# Sure Sort 0.1.5 validation — 11 November 2026

- Release build succeeded with MinGW 13.1 / Qt 6.8.3 / LibRaw 0.21.4.
- Full CTest run: **9/9 suites passed**, 0 failures, 55.71 seconds. Tests for the removed Undo feature were retired; copy toggles, safe deletion, processing, rendering and recovery remain covered.
- Added checks for the output-path menu, optional destination toast and saved setting, horizontal footer overflow with 32 folders, absent Undo controls and Ctrl+Z having no effect on copies.
- Confirmed schema-1 copy progress still loads after its old history table is discarded by schema 2, preserving assignments, selection and source bytes.
- Native Windows UI case passed; inspected the Viewing checkbox, destination toast and horizontally scrolled footer at narrow width. Captures: `build/output-controls-ui`.
- Packaged app launched successfully without development DLL/plugin paths and created an isolated folder progress file. Windows FileVersion and ProductVersion both report 0.1.5.

App: `dist/Sure Sort 0.1.5/Sure Sort.exe`.
ZIP: `dist/Sure-Sort-0.1.5-Windows-x64-Portable.zip`.
SHA-256: `A1B6214B521DF90E2AFADF24069E88F71CCE9D5ABC633005FDDF9FBB4AE43756`.

UI refinement: verified the output button is at most 360 × 28 logical pixels, does not stretch into spare header space, and elides long paths. Native menu capture uses the shared charcoal theme, icons, rounded border and blue selection color. Captures: `build/refined-output-ui`. Older packages were removed from dist after preserving their data under `build/preserved-version-data`; the current data/profile.sqlite was hash-verified unchanged during replacement.
