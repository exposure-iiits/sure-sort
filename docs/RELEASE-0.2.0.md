# Sure Sort 0.2.0 — local test build

Prepared 10 November 2026. This build has not been pushed or published.

- **Photo information:** the circled i in the header opens a rounded translucent card over the photo. It shows filename, format, file size, dimensions, camera, lens, ISO, aperture, shutter speed, focal length, capture date/time, modified time and location when available. Camera information comes from EXIF or RAW headers. Missing fields are omitted; capture time is not inferred from the file's modified time.
- **Histogram:** the adjacent chart icon opens a matching card with red, green, blue and luminance channels for the edited preview. It updates after edits and navigation. This is a preview histogram, not a RAW sensor histogram. Click the same icon, the close button, outside the card, or press Esc to dismiss it.
- **Editing:** click an adjustment's numeric value and type, then press Enter or move focus to apply it. Exposure uses EV; the other tone values and rotation use whole units/degrees. Subtle shortcut hints follow the current bindings. Rotation dragging snaps within 4° of 0°, ±90° and ±180°. Hold Alt to bypass snapping, or type an exact angle. Double-click a slider to reset it.
- **Thumbnail strip:** a small scrollable strip below the viewer shows the folder's photos and highlights the current selection. Click a thumbnail to navigate. Settings → Viewing → Show photo thumbnail strip enables/disables it and remembers your preference. Only visible thumbnails are decoded, in one background worker with a bounded cache; RAW thumbnails never develop sensor data. Thumbnails show source previews.
- **Footer:** folder shortcuts have a thin rounded key box. Long folder names are elided with the full destination in a tooltip. Crowded destinations scroll horizontally using the scrollbar, mouse wheel or trackpad, while Undo and Reset remain available. The status area reduces secondary details at narrow widths.
- **Fullscreen:** both the footer and thumbnail strip hide. Moving to the top reveals the header; double-click, F11 or Esc leaves fullscreen. Your thumbnail preference returns afterward.
- **Settings and shortcuts:** these now open as modal windows. Close the window or press Esc before returning to the main app. The shortcuts window includes a visible “Double-click to edit” hint.

Run `dist/Sure Sort 0.2.0/Sure Sort.exe`, or extract the 0.2.0 portable ZIP. Keep the DLLs and plugin folders beside the executable. Settings → About, `--version` and Windows file properties use version 0.2.0.
