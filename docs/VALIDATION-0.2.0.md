# Sure Sort 0.2.0 local validation

Validated on Windows 11, 10 November 2026. Local changes only; no GitHub push or publication.

## Build and automated checks

- Release build: Qt 6.8.3, MinGW GCC 13.1, LibRaw 0.21.4.
- All eight CTest suites passed: folder scanner, photo viewer, sorting, watermark, editing, output, workflow and viewer tools.
- Native Windows viewer-tools run: six Qt Test entries passed, including setup/cleanup. Captures inspected in `build/ui-0.2.0-native/`.
- New metadata checks cover JPEG EXIF, missing EXIF, synthetic RAW headers, dimensions, file bytes, cancellation and both TIFF byte orders for date/time and shutter speed.
- Histogram checks verify channel/luminance counts, transparent-pixel exclusion and edited-preview changes.
- Numeric input checks cover typed EV, negative tone values, exact rotation, drag snapping and leaving snapped angles.
- Layout/navigation checks cover 24 photos, 30 long destination names, the 640×480 minimum window, horizontal footer overflow and access to the final Add button, thumbnail selection, info updates across navigation, popup placement, dismissal, fullscreen footer/strip hiding and restored visibility.
- Settings checks verify modal behavior, suppression of photo shortcuts while open, thumbnail preference persistence and custom shortcut/step persistence across restart.
- Final text-wrapping/hint refinements rechecked with viewer-tools, workflow and output suites, plus another native Windows viewer-tools run.
- Fresh portable package launched successfully with only Windows system directories on PATH and an isolated session database. CLI version and Windows FileVersion/ProductVersion all report 0.2.0. No session databases are included in the portable folder/ZIP.

## Scope

The histogram is calculated from the edited display preview, not RAW sensor values. Camera metadata availability depends on the file. Thumbnails are source previews. Real-camera EXIF variations, very large folders and multi-monitor DPI transitions remain useful checks during user testing. Existing output, copy/move, undo and recovery coverage passed. No macOS validation was performed.
