# Sure Sort 0.1.6 validation

Validated on Windows 11 with Qt 6.8.3, MinGW GCC 13.1 and LibRaw 0.21.4.

- Release build passed.
- All nine automated suites passed: folder scanner, preview/core, sorting, watermark, editing, output, workflow, viewer tools and folder progress. The workflow suite passed after updating its old remapped-folder-key expectation for the reduced shortcut editor.
- Watermark regression coverage includes fresh folders, previously saved folders with different watermark settings, cached folder switches, enable/disable, app restart, embedded logo bytes and preservation of an earlier assignment's watermark snapshot.
- Settings preview checks assert opacity changes immediately on mouse/key press and restores in the release event for all three sliders, with no delayed assertion. Page switching and hiding Settings also restore opacity. These checks passed with the native Windows platform plugin.
- A 5000 × 4000 (20 MP) PNG logo imports and survives saving/reloading. Folder switching and immediate shutdown preserve the latest debounced watermark settings.
- Destination toast coverage requires the exact folder name with no image name. The footer check rejects the F Fit hint. Windows UI inspection confirmed the restored logo and updated footer.
- Windows UI inspection confirmed the shorter Quick editing page, updated watermark description and three shortcut tabs. Workflow checks verify hidden actions still trigger and visible shortcut customization persists after restart.
- Windows executable properties, About/CLI version sources and package defaults are updated to 0.1.6. The portable package is built from a clean directory; UI checks use separate test data.
- Inno Setup 6.7.4 compiled the Windows x64 installer from a clean portable package. The published installer and ZIP return HTTP 200; GitHub's SHA-256 asset digests match the local packages.

Other operating systems and Windows 10 were not validated in this run.
