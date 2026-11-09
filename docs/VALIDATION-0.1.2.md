# Sure Sort 0.1.2 validation

Verified on Windows 11 x64 on 9 November 2026 with Qt 6.8.3, MinGW GCC 13.1 and dynamic LibRaw 0.21.4.

- All seven CTest suites passed (70.25 seconds): scanner, viewer/core, sorting, watermark, editing, output and workflow.
- After final UI changes, viewer/core, editing and workflow suites passed again (38.13 seconds).
- Final native Windows checks passed for inline folder creation, missing-output settings entry, Move/next-key defaults, focus-loss saving, Escape cancellation, fullscreen transitions, direct shortcut capture, conflict toasts, alternate/chord bindings, restart persistence and Editing category entry.
- Native screenshots were inspected for the shortcut categories, inline folder control and settings. The final packaged application was inspected using Computer Use: header and native title bar display Sure Sort without a visible logo.
- The extracted portable application launched with only the Windows system directories on PATH, opened a local image folder and exited normally. Its isolated SQLite session was created successfully.
- CLI reports `Sure Sort 0.1.2`. Application and installer Windows version resources report 0.1.2.
- Built the portable ZIP and Inno Setup installer. The ZIP contains no session/data files. The executable in the build, distribution folder and installer source has the same SHA-256 hash.

Follow-up: the header Settings and Shortcuts buttons now toggle back to the photo screen. Workflow coverage includes repeated clicks and switching between the two panels. The application version remains 0.1.2; release binaries and the complete source archive were rebuilt.

Artifacts:

- `dist/Sure-Sort-0.1.2-Setup-Windows-x64.exe`
- `dist/Sure-Sort-0.1.2-Windows-x64-Portable.zip`
- `dist/SureSort-source-0.1.2-final.zip`
- Adjacent SHA-256 checksum files.

Installer compile succeeded. Installing this release on a clean machine, Windows 10, MSVC builds, real-camera RAW compatibility and additional monitor/DPI configurations were not tested in this pass. The executable is unsigned.
