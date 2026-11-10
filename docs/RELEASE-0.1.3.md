# Sure Sort 0.1.3 — local test build

Prepared 10 November 2026. Supersedes the local build labeled 0.2.0; this update continues the 0.1.x version series. Windows packages are built locally.

- Photo information and edited-preview RGB/luminance histogram use rounded translucent cards.
- Edit opens a matching translucent card over the photo, with the same font, colors and rounded treatment. Sliders, typed values, keyboard hints, rotation snapping, reset and crop behavior remain available.
- Fullscreen hides only the filename/Ready/count/dimensions/type/zoom status row. Sorting folders, Undo, Reset and the thumbnail strip stay visible.
- Thumbnails show images without filenames or visible scrollbars. Wheel and trackpad scrolling still work.
- A green thumbnail border means a Copy or Move has completed in at least one current destination. Pending/failed jobs do not count. The border updates after undo/clearing and survives restarting; selected completed photos also retain a blue selection accent.
- Settings → Viewing offers Left (default), Right, Top and Bottom positions. Visibility and position are remembered across restarts.
- Rotation snaps near 0°, ±90° and ±180° while dragging. Alt bypasses snapping; typing allows exact values.
- Settings and shortcuts remain modal windows. Double-click shortcut cells to edit.

Run `dist/Sure Sort 0.1.3/Sure Sort.exe` or extract the matching portable ZIP. Camera metadata depends on the file; the histogram uses the edited display preview. Thumbnail images use source previews.
Info and Histogram open on the left while Edit stays at the top right. Each column scrolls independently; opening a photo information card does not push the editing controls down. Saved thumbnail placement choices are preserved.
