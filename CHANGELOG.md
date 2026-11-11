# Changelog

## 0.1.5 — 11 November 2026

- Refined the output path into a compact, elided header control with a chevron and a menu matching the charcoal theme. Version remains 0.1.5.

- Click the output path to open Explorer or change the output folder.
- Removed sorting Undo, its button/shortcut and persistent history; existing copy progress stays available.
- Added a saved destination toast option under Viewing, shown only after successful copying.
- Verified crowded footers continue scrolling horizontally.


## 0.1.4 — 11 November 2026

- Footer folders now toggle copies with matching primary/alternate shortcuts and clicks. Green outlines identify the current photo’s destinations; removals keep the current photo selected.
- Replaced Manage copies and the extra status row with the existing folder controls.
- Added a full-photo genie animation that bends and narrows into the folder without delaying navigation or saving.

### Copy-only folder sessions

- Removed Move, original relocation, action controls, transfer journals and earlier-session compatibility.
- Per-original-folder progress database restores full sorting setup, edits, copies, Undo and last photo; read-only folders use a visible local fallback.
- Added verified per-destination removal. Folder sessions share one processing worker.
- Unified destination styling and kept information/histogram open until explicitly closed.
- Added folder-progress, removal and panel regression coverage.

## Experimental macOS work

- Prepare an experimental macOS app bundle, DMG packaging script and manually
  triggered Apple Silicon GitHub Actions build. Mac builds remain unverified;
  Automatic output removal still depends on Windows file locking.

## 0.1.3 — 10 November 2026 (local test build)

See [release notes](docs/RELEASE-0.1.3.md).

- Rounded translucent photo information and edited-preview histogram cards.
- Optional thumbnail strip with saved placement on any edge, hidden scrollbars, no filenames and bounded background decoding.
- Green thumbnail outlines for photos successfully copied or moved to any current destination, with live undo/clear updates and restored session state.
- Translucent edit card with editable adjustment values, current shortcut hints and rotation slider snapping.
- Boxed footer keys, elided long folder names and scrollable crowded destinations.
- Fullscreen hides only the status row; folder controls and thumbnails remain visible.
- Modal Settings/Keyboard shortcuts windows and a visible double-click editing hint.

## 0.1.2 — 9 November 2026

See [release notes](docs/RELEASE-0.1.2.md).

- Inline folder naming with Move and the next available key by default; missing output folders open Sorting settings.
- Double-click shortcut cells to rebind immediately; conflicts show a toast. Stronger active category styling and direct Editing category entry.
- Text-only header and native title bar; executable branding retained.
- Degree rotation slider from −180° to +180°.

## 0.1.1 — 4 November 2026

See [release notes](docs/RELEASE-0.1.1.md) for performance, folder controls, shortcut categories/alternates, fullscreen gestures, reset edits, duplicate-name verification, and schema 7 migration.

## Earlier changes

- Minimal photo/checkmark logo embedded in the Windows executable and application icon. Portable mode keeps settings and sessions beside the app when `portable.ini` is present or `--portable` is passed.
- Opening a folder scans all nested subfolders. Matching filenames in different directories remain separate, RAW/JPEG pairing stays within each directory, and moved photos from nested folders remain available after reopening.
- Shortcut editing uses accessible pencil icon buttons with tooltips.
- Circular footer badges count actual destination photos, update after copy/move/undo and external changes, and exclude Originals/subfolders. Directory scans run in a bounded background worker.
- Clean GitHub source handoff with START_HERE instructions; application version remains 0.1.0.

### 0.1.0 interface refinements — 23 October 2026

- Footer chips show only key and folder name, tinted blue for Copy and purple for Move.
- Grouped keyboard menu with per-binding Edit, conflict validation and persistent application, folder and editing shortcuts; removed the Shortcuts settings page.
- Settings → Editing has an automatically saved keyboard step slider (1–20 units, default 5; 0.1 EV per exposure unit).
- About shows version 0.1.0 and versioning rules. CMake supplies the version to both About and the application/CLI.
- Disabled watermark hides PNG selection and all subordinate controls, retaining their settings. First-time enabling exposes PNG selection.

### UI refresh — 22 October 2026

- Charcoal, photo-first layout inspired by the Claude version: slim toolbar, input/output paths, compact destination chips and a two-row status area.
- Unified non-modal settings window with Sorting, Watermark, Editing, Saving, Shortcuts and About pages; Ctrl+, opens it.
- Visible Undo button, contextual crop ratio/Apply/Cancel bar, matching portrait/landscape ratio choices, and keyboard-edit value toasts.
- Existing GPT processing, session recovery, output ownership checks and keyboard editing behavior retained; integrated settings/crop/sorting/undo tests added.

### Added

- Compact two-row footer and destination editor with per-row bin icons and a right-aligned plus button. Removed Up/Down, global Remove, navbar Undo and queue Retry/Cancel controls; Ctrl+Z remains available.
- Two delayed automatic retries with persistent attempt/operation state (schema 6), captured settings, no extra decode on publication collisions, and an inline error after exhaustion.
- Sure Sort branding and executable, refreshed dark/teal controls, fullscreen/settings icons and a complete shortcuts reference. Settings now contains Sorting destinations, Output, Watermark and editing shortcut configuration.
- Per-destination Copy/Move, multiple folder placements and action-specific persistent Ctrl+Z. Move journals verified original-byte transfers into the destination’s Originals subfolder; Undo restores prior paths without overwriting external files.
- Five-unit editing keyboard steps (0.5 EV exposure), Original crop ratio and Z toggling 100%/Fit.
- Schema 5 migration, relocated-source browsing/processing and move/undo interruption/conflict tests.

- Phase 5 output preferences: JPEG quality 80–100, original/75%/50% dimensions and supported EXIF preservation; captured per assignment, including retry/undo/recovery.
- Correct sRGB RAW transfer curve, conservative auto-bright threshold, earlier sensor-buffer release and ownership transfer avoiding a full image copy.
- Bounded EXIF subset reconstruction, final orientation/dimensions, streaming JPEG/PNG metadata insertion, metadata stripping and repaired reimported watermark assets.
- Completed/Processing/Pending/Failed counts, schema 4 migration and 510-image background processing verification.

- Phase 4 collapsible editing panel with exposure, brightness, contrast, highlights, shadows, saturation, 90° rotation and Reset All; double-click adjustment reset.
- Customizable repeated hold-key editing chords, conflict detection and preservation of existing destination keys.
- Viewer crop drawing, corner resizing, repositioning, aspect ratios, zoom/pan and original-relative coordinates, confirmed with Enter or cancelled with Esc.
- Per-photo edit persistence, bounded asynchronous interactive previews and idle full-detail refinement, sharing the output adjustment engine.
- Automatic tone → rotation/crop → watermark processing, immutable job edit snapshots and safe refresh of sorted outputs without changing originals.
- Schema 3 migration, refresh failure/cancellation/recovery/race checks, native UI coverage and tone-processing benchmarks.

- Phase 3 PNG watermark settings: four corners, relative size/padding, opacity, enable toggle and automatic persistence.
- Instant overlay in the existing viewer, with shared preview/output geometry through zoom, pan and resize.
- Automatic full-resolution JPEG/RAW watermarking on assignment, with unchanged direct copies when disabled and PNG output support.
- Immutable per-job settings/PNG snapshots, deduplicated assets, restart/retry/undo preservation and additive SQLite v1→v2 migration.
- Watermark geometry, alpha, UI, output, snapshot, batch and migration tests; overlay/compositing benchmark.

- Phase 2 destination configuration, custom shortcuts and compact folder bar.
- One-key auto-advance with automatic JPEG/PNG copying or full-sensor RAW-to-JPEG; RAW+JPEG produces one output.
- Serial processing queue, retry/cancel, collision-safe temporary publication and verified undo/clear/reassignment.
- SQLite configuration, assignment/output journal, persistent undo history, browsing position and job recovery.
- Phase 2 sensor, collision, recovery, safety and keyboard/configuration UI tests.

- Native C++17/Qt Widgets Phase 1 photo viewer, displayed as PhotoCull.
- Asynchronous folder import, natural sorting and RAW+JPEG grouping.
- Display-sized JPEG/PNG and embedded RAW previews, orientation handling and sRGB conversion.
- Bounded loading, cancellation, stale-result protection, adjacent prefetch and byte-accounted LRU cache.
- Keyboard navigation, fit/100%, wheel zoom, drag pan and fullscreen.
- Generated-fixture Qt tests, Release benchmark and Windows deployment helper.
- Research, architecture, performance, development, testing and future-phase documentation.

### Validation status

Windows 11/MinGW build, tests and local packaged launch have been verified. See [PERFORMANCE](docs/PERFORMANCE.md) for evidence and remaining validation. Advanced RAW development and vendor metadata/IPTC/XMP transfer for re-encoded images remain future work. This entry does not announce a published release.
