# Copy-only workflow validation — 11 November 2026

Built with the existing MinGW 13.1 / Qt 6.8.3 / LibRaw 0.21.4 Windows kit in `build/portable-release`.

- Final full CTest run: **9/9 suites passed**, 0 failures, 57.84 seconds. Temporary profiles and generated original-photo fixtures were confined to `build/tmp`.
- Native Windows `folder_progress_tests`: **17 passed**, 0 failures (including lifecycle cases). Covers portable full-session restoration, internal/external output paths, shared worker/folder switching, interrupted copy/removal, protected deletion and retry, source-byte preservation, read-only fallback and conflict preservation.
- Native Windows viewer panel/layout case: passed. Checked information, histogram alongside editing, navigation, fullscreen, thumbnail positions and explicit closing.
- Native Windows Sorting dialog case: passed. Verified three columns (name, shortcut, remove), with no Action chooser.
- Visually inspected native screenshots at 640 and 1280 logical window widths, the Manage copies dialog, Sorting settings, information and histogram/editing panels. Captures are in `build/copy-only-ui`.
- Packaged app launched with development DLL directories removed from PATH: exit **0**. The generated input folder received its progress database and the isolated app profile was created. No session database is included in the shareable package.
- Packaged and built executables have identical SHA-256 hashes.

Runnable app: `dist/Sure Sort Copy Only/Sure Sort.exe`.
Shareable archive: `dist/Sure-Sort-Copy-Only-Windows-x64.zip`.
Archive SHA-256: `0DF5AFAF35333BF19C318C0306CA0ED047B144A7824CE138B60236ED56CF375A`.

The application version remains 0.1.3. This uses a fresh copy-only progress format; earlier Move sessions are not imported. Original user photos and the previous Completion package were not modified. Synthetic RAW fixtures verify the exercised processing and original-byte behavior; real-camera compatibility is covered separately by the existing manual checklist.
