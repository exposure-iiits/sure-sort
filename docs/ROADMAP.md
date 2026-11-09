# Development roadmap

## Phase 1 — viewer implemented, release validation pending

Implemented: asynchronous recursive import, natural sort, RAW+JPEG grouping within each directory, JPEG/PNG/RAW previews, orientation and sRGB handling, bounded workers and byte-accounted cache, adjacent prefetch, keyboard navigation, zoom/pan, 100% detail and fullscreen. Research, design, automated tests, benchmark tooling and Windows deployment instructions are present.

Windows 11/MinGW verification and synthetic fixtures are recorded in [PERFORMANCE](PERFORMANCE.md). Before calling the viewer release-ready, complete the applicable [manual checks](MANUAL_TESTS.md), especially real camera/compression variants, MSVC, Windows 10, clean deployment, mixed DPI and sustained memory/latency behavior. Linux remains unverified.

## Future phases — not implemented

| Phase | Scope | Extension point / boundary |
| --- | --- | --- |
| Further processing | Advanced development controls and further camera/performance validation | Extend the separate processing pipeline. Full-sensor RAW-to-JPEG is already implemented in Phase 2. |

## Phase 2 — implemented, release validation pending

Ordered destinations/custom shortcuts, automatic copying/full-sensor RAW-to-JPEG, auto-advance, separate processing workers, retries/cancellation, undo/clear/reassignment, collision-safe publication and SQLite session recovery are implemented. RAW+JPEG produces one JPEG. Copy preserves originals; the Sure Sort update adds journaled Move and restoration.

Real-camera quality, disk interruptions, large mixed shoots, MSVC, Windows 10 and clean deployment remain release checks. Linux cleanup conservatively preserves outputs on undo. See [PERFORMANCE](PERFORMANCE.md) for verified results.

## Phase 3 — implemented, release validation pending

Transparent PNG import, enable toggle, four corners, relative size/padding and opacity are implemented with automatic settings persistence. The existing photo viewer shows the live overlay through navigation, zoom, pan and resize. Sorting captures the PNG/settings snapshot and automatically composites at final output resolution after full-image decode/RAW development. There is no Export/Apply action. Phase 3 introduced SQLite schema 2; current schema 6 retains queued logo versions and also adds editing.

Synthetic output, alpha/color/orientation, snapshot/undo, batch and native Windows checks are described in [PERFORMANCE](PERFORMANCE.md). Real club logos on target cameras, sustained large shoots, mixed DPI, Windows 10 and MSVC remain release checks. Phase 5 implements a supported EXIF subset for re-encoded images; vendor metadata/IPTC/XMP remains outside that subset.

## Phase 4 — implemented, release validation pending

Per-photo exposure, brightness, contrast, highlights, shadows, saturation, rotation and crop are implemented non-destructively, with a collapsible panel and customizable repeated hold-key shortcuts. Crops use original-relative coordinates, aspect ratios and viewer zoom/pan transforms. A bounded asynchronous tone queue reuses previews and refines full detail after idle. Edits persist in schema 3 and are captured for automatic processing before watermarking.

Editing sorted photos automatically refreshes verified application outputs, retaining the old file through rendering and prepared journaling. External modifications, cancellation/failure, retry and recovery are covered by generated tests. Full camera/color quality, long batches/peak memory, mixed DPI, MSVC, Windows 10 and clean deployment remain release gates. See [EDITING](EDITING.md), [TESTING](TESTING.md) and [PERFORMANCE](PERFORMANCE.md).

The workflow is **Import → Preview / Edit / Watermark → Assign Folder → Automatic Background Processing**. There is no Export action. No delivery dates are committed.


## Phase 5 — implemented, camera validation pending

Compact persisted output settings, immutable output snapshots, explicit sRGB RAW development, lower-copy buffer ownership, supported EXIF transfer/stripping and assignment processing counts are implemented using existing workers and recovery. Automated tests include a 510-image actual processing queue. Real camera color/detail and sustained large-RAW memory checks remain manual; see PERFORMANCE and MANUAL_TESTS.

## Sure Sort workflow update — implemented

Per-folder Copy/Move, multiple copies, exact original restoration with persistent Ctrl+Z, refreshed icon controls/settings menu, all-shortcuts reference, Original crop ratio, five-unit keyboard adjustments and Z 100%/Fit toggling are implemented. Original relocations use the existing bounded processing worker and a durable transfer journal. Cross-volume/removable-drive failure validation and clean-machine/high-DPI checks remain release work.
