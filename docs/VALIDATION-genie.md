# Genie animation validation — 11 November 2026

- Release build succeeded with the existing Windows MinGW 13.1 / Qt 6.8.3 / LibRaw 0.21.4 kit.
- Full CTest run: **9/9 suites passed**, 0 failures, 56.03 seconds.
- Focused native Windows checks: **5 passed**, including lifecycle checks. Verified the full-photo starting bounds, transparent completion, immediate navigation, one active animation, cancellation and footer toggles.
- Visually inspected native start, bend, funnel and landing frames using a generated gradient/grid fixture, plus the animation in the actual photo viewer. Captures are in `build/genie-ui`.
- The overlay uses a bounded snapshot of the visible photo and a 64-band curved mesh. It lasts 520 ms, is transparent to mouse input, and fades at the folder. The viewer and animation share photo rendering, including edits, crop/rotation, pan and watermark placement.
- Packaged launch passed with development DLL/plugin directories removed from PATH; an isolated original-folder progress database was created.

Version remains **0.1.4**. App: `dist/Sure Sort 0.1.4 Genie/Sure Sort.exe`.
ZIP: `dist/Sure-Sort-0.1.4-Windows-x64-Genie.zip`.
ZIP SHA-256: `201F8F8F7EBD64633175E9BE233BA66EC28B0539AEEABE03A1C0A226B86D85D6`.

The source commit also contains the preceding copy-only sessions and footer toggles, which had not yet been pushed from the exported source folder. Generated builds, archives, screenshots, dependencies and session databases are excluded from Git.
