# Sure Sort 0.1.3 validation — 10 November 2026

Local Windows 11 build with Qt 6.8.3, MinGW GCC 13.1 and LibRaw 0.21.4. No push or publication.

- All eight CTest suites passed (39.63 seconds).
- Final styling checked again with native Windows viewer-tools: 6 entries passed, 0 failed. Captures reviewed in build/ui-0.1.3-native.
- Fullscreen shows sorting folders, Undo and thumbnails while hiding only photoStatusRow.
- All four strip positions verified geometrically. Hidden horizontal/vertical scrollbar policies, wheel scrolling, thumbnail navigation and saved placement were checked. Bottom remains the default.
- Existing editing tests passed; typed values, rotation snapping, crop and shortcuts remain available in the translucent card.
- Portable package smoke launch passed with only Windows system directories on PATH. CLI, FileVersion and ProductVersion report 0.1.3.

The earlier local 0.2.0 label is superseded by 0.1.3. Camera metadata and preview histogram limitations remain documented in the release notes.

## Same-version card layout refinement

Edit, Info and Histogram now share a 336 px card width. A scrollable tool column places the cards in separate layout slots; edit input keeps the histogram open. Crop/ratio share one row, and Reset All/Editing shortcuts share another with equal button sizes.

Editing and workflow suites passed. Updated viewer-tools checks passed offscreen and on native Windows (6 entries), including editing exposure with Histogram open, non-overlapping geometry, equal widths/button sizes, and reaching lower controls at 640×480. Captures: `build/ui-0.1.3-card-layout`. The refined portable package smoke launch passed and still reports 0.1.3. Package: `dist/Sure-Sort-0.1.3-Windows-x64-Refined.zip`. No push or publication.

## Left information column and thumbnail default

Info/Histogram now use an independent left column while Edit stays at the top right. Both retain equal widths and narrow together when necessary to avoid overlap. Left is the thumbnail default for preferences without an explicit placement; saved selections are preserved.

All eight suites passed (37.80 seconds); native Windows viewer-tools passed all six entries. Tests verify the new default, persisted selections, independent left/right placement, unchanged Edit top position when Histogram opens, typing while Histogram stays visible, and narrow-window access. Captures reviewed in `build/ui-0.1.3-left-tools`. The new package launch check passed; version remains 0.1.3. Package: `dist/Sure-Sort-0.1.3-Windows-x64-Left-Layout.zip`. No push or publication.

## Completed thumbnail outline

Thumbnails have a green border when any current destination assignment has completed. The active photo retains a separate blue selection accent. Assignment changes repaint the strip; no extra photo decoding is required for status updates.

All eight CTest suites passed (38.04 seconds). Native Windows viewer-tools passed seven entries, including completed Copy, multiple destinations, undo with another destination remaining, clear placement, restart, completed Move and undo restoring the source. The initial and pending states remain unmarked. Capture reviewed: `build/ui-0.1.3-completion/completed-thumbnails.png`. Version remains 0.1.3. Local portable package: `dist/Sure-Sort-0.1.3-Windows-x64-Completion.zip`.
