# Single-file Windows installer

Share `dist/Sure-Sort-0.1.7-Setup-Windows-x64.exe`. It installs the complete application, Qt/LibRaw dependencies and license notices, then offers to open Sure Sort. No separate ZIP extraction, SDK or administrator elevation is needed. A Start menu shortcut is created; the desktop shortcut is optional.

The default location is `%LOCALAPPDATA%\Programs\Sure Sort`. Installed app preferences live under `%LOCALAPPDATA%\SureSort\Sure Sort`; sorting progress lives in each original folder's `.suresort-progress.sqlite` file. The installer excludes the portable-mode marker and personal databases. Portable ZIPs keep app preferences in their own `data` folder. Uninstall removes installed program files and leaves user-created session files alone.

The EXE is unsigned. Packaging as an installer does not remove Windows SmartScreen warnings.

Build using the project-local Inno Setup 6.7.4 compiler:

```powershell
./tools/package-installer.ps1
```

Alternatively pass `-Compiler` with the path to ISCC.exe. The compiler was downloaded from the official jrsoftware GitHub release, its Authenticode signature verified as Valid (Pyrsys B.V.), and installed in portable mode under `.deps/`.

Verification on 5 November 2026: silent installation to an isolated project folder succeeded; all 97 installed payload files matched their source SHA-256 hashes; the installed application launched successfully with its bundled dependencies and an isolated test session; test uninstallation succeeded.
