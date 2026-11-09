# Measured performance and verification

Measured locally on **17 October 2026**. These are engineering observations, not guarantees.

## Environment

- Windows 11 Version 25H2, x86_64, Intel Core Ultra 7 155U, 14 logical processors reported to Qt.
- Physical RAM amount, storage model and SSD status could not be reliably queried inside the sandbox; no storage-class claim is made.
- Release C++17 build, MinGW GCC 15.2.0, Ninja/CMake. Official Qt 6.8.3 dynamic Windows SDK (built by GCC 13.1.0); dynamic LibRaw 0.21.4 compiled locally, without OpenMP or optional external decoder integrations. This combination successfully built and ran here; use the recommended MSVC kit for the intended Windows release path.
- Test data on the local project drive; temporary directories redirected into `build/tmp` because the sandbox's default redirected temporary directory permitted directory creation but refused file writes.
- Native Windows GUI checked at the machine's 150% device scale, including a captured widget rendering. Multi-monitor DPI and other scales remain manual checks.

## Release microbenchmarks

`photocull_bench` creates one 6000×4000, quality-90 JPEG with a deterministic high-frequency RGB pattern (14,099,721 bytes). Scan-only sibling entries have supported extensions but contain no image data; scanning deliberately does not decode them. Ten decodes use a warm filesystem cache and a 1920×1080 physical target, producing 1620×1080 pixels. No manual disk-cache flush was performed. Other system activity was uncontrolled.

| Measurement | 1,000-entry run | 10,000-entry run |
| --- | ---: | ---: |
| Scan + grouping + natural sort | 166.313 ms | 1,250.9 ms |
| JPEG decode + scaling, minimum | 140.511 ms | 137.360 ms |
| JPEG decode + scaling, median (sorted sample 6 of 10) | 144.658 ms | 151.853 ms |
| JPEG decode + scaling, maximum | 165.743 ms | 164.199 ms |
| Cache-accounted image bytes | 6,998,400 | 6,998,400 |
| Mean lookup over 10,000 LRU hits | 0.02430 µs | 0.04622 µs |

An earlier run measured a 121.942 ms 1,000-entry scan and 136.270 ms median decode. Variation demonstrates why the ≤150 ms decode target is not a guarantee. Cache measurements are lookups and shared QImage assignments only; they do **not** establish one-frame keyboard-to-present latency.

No startup latency, process-wide peak RAM/CPU, real-camera RAW timing, or end-to-end navigation timing has been measured. The 256 MiB budget bounds retained cache image bytes, not total process memory. Decoder intermediates, current/detail images, queued results, Qt and GUI rendering require additional memory.

## Verification performed

- CMake configured and all application/library/test/benchmark targets compiled on Windows with the above MinGW environment. Initial configuration/build attempts exposed missing project files and local LibRaw build setup issues; these were resolved before the successful build. No MSVC verification is claimed.
- CTest: **1/1 suite passed**, containing **22 Qt Test results**, including setup/cleanup and eight EXIF rows. Full suite also passed using Qt's native `windows` platform plugin; 0 failures/skips, 2.232 seconds in the recorded native run.
- Automated coverage: filtering, case, natural ordering, pairing, stable IDs, empty/invalid folders, padded-row byte accounting/LRU, sRGB conversion, display-size limits, all eight generated JPEG EXIF orientations, thumbnail ranking, synthetic DNG JPEG/bitmap previews, explicit/missing embedded EXIF, wide Unicode RAW paths, largest-preview detail, no-preview half-size fallback, prefetch/detail sensor-decode prohibition, missing/corrupt files, companion fallback, bounded rapid navigation, latest generation delivery, folder-switch isolation, resize/detail cancellation, shutdown with active work, named shortcuts/hold chords, and widget smoke checks.
- `windeployqt` assembled `dist/PhotoCull`. The packaged executable launched and exited with **code 0** using `--smoke-test-ms 800` with Qt/LibRaw/MinGW development paths removed from PATH. This is a local deployment smoke test, not a clean-machine certification.
- Native rendering was inspected; no claim is made that every interaction in the manual checklist was exercised.

Reproduce timings after a release build:

```powershell
& .\build\windows-release\photocull_bench.exe
& .\build\windows-release\photocull_bench.exe 10000
```

The local MinGW verification uses `build/mingw-release` instead. Follow [README](../README.md) for dependency installation and the intended MSVC build. Windows 10, MSVC, Linux, real camera RAW files, inaccessible drives/ACLs and mixed-DPI monitor transitions remain unverified; use [MANUAL_TESTS.md](MANUAL_TESTS.md) before a release.

## Phase 2 verification — 17 October 2026

The same Windows 11/MinGW Release environment now includes Qt SQL/QSQLITE. Both suites passed: the original 22 Qt Test results and 20 sorting results (42 including setup/cleanup/data rows). The recorded CTest run passed 2/2 suites in 5.58 seconds; the native Windows sorting run passed all 20 results in 3.006 seconds. Native folder-bar/footer rendering was inspected at 150% DPI.

New checks cover 200 metadata-only assignments while a worker is deliberately blocked, one processing worker, immediate keyboard auto-advance, mapping editing/persistence, auto-repeat suppression, exact JPEG/PNG copying, collision preservation, retries/cancellation, queued/active/completed undo, changed/replaced outputs, persisted history/position, session exclusion and restart at prepared/renamed/interrupted states. Synthetic RAW+JPEG processing produced one 384×512 oriented sensor-derived JPEG with an sRGB profile rather than exporting the embedded 80×40 preview. Originals remained unchanged in those tests.

The packaged app launched with development SDK paths removed from PATH and exited 0. Its SQLite file header and schema version 1 were checked after closing, confirming sorting persistence initialized rather than only the viewer launching. `Qt6Sql.dll` and `sqldrivers/qsqlite.dll` were deployed. This remains a local smoke test, not clean-machine certification.

A fresh release viewer microbenchmark measured: 1,000-entry scan **122.529 ms**; JPEG minimum/median/maximum **140.816 / 147.849 / 166.944 ms**; cache-accounted bytes **6,998,400**; mean cache lookup **0.04512 µs**. Fixture and warm-cache conditions match the earlier description. These observations do not prove end-to-end sorting latency or real-camera conversion performance. No real-camera RAW throughput, sustained processing memory/CPU, startup time or power-loss testing is claimed.

Remaining limitations: real camera/compression/color quality and MSVC/Windows 10 require validation; RAW JPEG camera metadata transfer is not implemented. Unverified partial temps are preserved on restart. Modified or locked outputs are preserved during undo; POSIX cleanup always preserves outputs rather than risk concurrent modification. Output directory/configuration changes apply to future assignments, preserving existing task snapshots.

## Phase 3 verification — 17 October 2026

The same Release environment passed **3/3 CTest suites in 12.64 seconds**, containing **71 Qt Test results** (22 viewer, 20 sorting, 29 watermark; includes setup/cleanup/data rows). The watermark suite also passed all 29 results using the native Windows plugin in **6.736 seconds** with no failures/skips. Native viewer/dialog rendering was inspected at 150% DPI.

Checks cover all four corners on landscape/portrait/square photographs, relative scaling and extreme aspect constraints, PNG alpha × slider opacity, sRGB/grayscale conversion, source/cache immutability, viewer/output pixel agreement, zoom/pan/resize/photo changes, live controls/import, full-resolution JPEG/PNG and synthetic sensor DNG output, EXIF orientation, transparent PNG output, disabling/direct-copy behavior, queue snapshots, retry/undo, restart without external logo files, damaged assets and additive v1→v2 migration. A 40-image 1600×1200 JPEG batch completed with a GUI timer continuing to fire. A separate blocked-worker case journaled/advanced 200 assignments in **319 ms** in the native run, sharing one decoded logo and one PNG asset database row. These fixtures establish scheduling behavior, not sustained real-camera throughput.

The release benchmark was extended without new dependencies. A cached 1200×600 semi-transparent PNG at 20% size, 75% opacity and 3% padding measured **0.08759 ms mean** for 300 overlay-only paints on an already allocated 1920×1080 RGB32 surface. This excludes base-photo painting, widget presentation, PNG decode and file I/O. Ten 6000×4000 copy-on-write plus full-resolution composite trials measured **17.453 / 18.968 / 22.089 ms minimum/median/maximum**, excluding photograph decode and output encoding. It is not an end-to-end navigation or processing latency claim.

The same benchmark run recorded: 1,000-entry scan **131.446 ms**, warm JPEG decode minimum/median/maximum **138.666 / 151.734 / 177.393 ms**, cache-accounted bytes **6,998,400**, mean cache lookup **0.02462 µs**. Earlier fixture/environment qualifications still apply.

`dist/PhotoCull` was refreshed with the Phase 3 executable. Packaged launch with PATH restricted to Windows system directories exited **0** for both a fresh session and a copied Phase 2 session; both SQLite headers reported schema **2** after closing. The local extracted SDK's deployment helper needed its compiler runtime DLLs adjacent to Qt tools; that setup issue was resolved before successful packaging. The helper now pins `qtpaths` to the requested Qt kit. No clean-machine certification is claimed.

Remaining release work: real club logos/cameras and preview/development crop/color comparison, sustained mixed RAW batches/peak memory, mixed-DPI monitors, MSVC, Windows 10 and clean-machine deployment. Original EXIF/IPTC is not transferred when re-encoding. Imported PNG versions remain in the session for persistent undo/retry history; they are deduplicated rather than garbage-collected. Decoded historical assets are released, and current/queued versions share immutable images. Schema 2 upgrades preserve older tasks as unwatermarked, and older app versions cannot reopen the upgraded session.

## Phase 4 verification — 17 October 2026

The final Windows 11/MinGW Release build passed **4/4 CTest suites in 19.08 seconds**, containing **113 Qt Test results**: 22 viewer, 20 sorting, 29 watermark and 42 editing (including setup/cleanup/data rows). The editing suite also passed all **42 native Windows results in 8.485 seconds**, with no failures/skips. Native edited-viewer/panel and active-crop screenshots were inspected at 150% DPI.

Editing checks cover all six tone controls independently and combined, every quarter turn, original-relative crops across zoom/resolution/rotation, five ratios, drawing/corner resize/move/pan, existing-crop ratio changes, reset/double-click, asynchronous latest-request delivery and idle refinement, stale refinement cancellation, image DPR, repeated default/custom chords, conflicts and legacy destination keys. Full-resolution JPEG/PNG and sensor-derived DNG output, preview/output color agreement, watermark-after-crop, original-byte preservation, per-photo/shortcut persistence and schema 2 migration passed.

Sorted-output checks cover coalescing rapid edits, reset back to exact copying, captured queued edits followed by automatic refresh, current watermark capture, no extra viewer advance, preserved external modifications with queue notices, cancelled/failed refreshes and Retry, immediate same-folder assignment before the debounce, prepared/renamed recovery and modification races. Sorting keys and destination buttons both wait while a crop draft is active. Recovery checks include Windows creation-time tunneling without relaxing full-identity deletion checks.

Release microbenchmark observations (10 warm trials; sorted sample 6 of 10 is the median):

| Operation | Minimum | Median | Maximum |
| --- | ---: | ---: | ---: |
| Six combined tone controls, 1920×1080 copy + render | 10.480 ms | 11.248 ms | 12.293 ms |
| Six combined tone controls, 6000×4000 copy + render | 130.644 ms | 135.159 ms | 136.752 ms |
| 24 MP copy + watermark composite | 18.059 ms | 19.126 ms | 20.251 ms |

Tone settings were +0.7 EV, brightness +5, contrast +15, highlights −10, shadows +20 and saturation +10. Inputs were already allocated RGB32 surfaces: the 1920×1080 canvas from the watermark benchmark and a gray 24 MP image. Timings include copy-on-write, curve construction and pixel processing, but exclude file decode, preview downscaling, geometry painting, encoding, publication and widget presentation. They do not establish end-to-end input latency or real-camera throughput. The UI bounds interactive work to two megapixels, coalesces/cancels requests, and requests full-detail refinement after 220 ms idle using the same decoded source. The GUI event-loop and refinement cancellation tests passed.

The same run measured a **131.674 ms** 1,000-entry scan, warm JPEG decode **138.662 / 214.725 / 239.820 ms** minimum/median/maximum, **6,998,400** cache-accounted bytes, **0.02375 µs** mean cache lookup and **0.09496 ms** mean overlay-only paint. System activity and CPU frequency were uncontrolled; earlier variation and fixture qualifications still apply. No whole-process peak-memory, sustained large-shoot or end-to-end latency guarantee is made.

The final `dist/PhotoCull/PhotoCull.exe` hash matches the tested executable. After deployment, launches with PATH restricted to Windows system directories exited **0** for a fresh database and copied schema 1 and 2 sessions. All three SQLite headers reported schema **3** after closing. This is local package/migration verification, not a clean-machine certification.

Remaining release gates are real cameras/club logos and color/crop comparison, sustained mixed shoots and peak memory, physical keyboard/layout and trackpad variations, mixed DPI, Windows 10, MSVC and clean-machine deployment. RAW previews may differ from full development. Re-encoded images still do not transfer original EXIF/IPTC. Full source/detail, edited display, active/refinement and output buffers add memory beyond the preview LRU budget. POSIX refresh/undo conservatively preserves preceding files and is unverified. Edits save after 350 ms and flush before assignment/normal shutdown; an abrupt process loss can discard the last unflushed slider changes, while committed job snapshots remain recoverable.


## Phase 5 verification — 17 October 2026

The final Windows 11/MinGW Release build passed **5/5 CTest suites in 39.45 seconds**, containing **147 Qt Test results** (22 viewer, 20 sorting, 30 watermark, 42 editing, 33 output; includes setup/cleanup/data rows). The output suite also passed all **33 native Windows results in 21.362 seconds**, with no failures/skips; its settings dialog was inspected at 150% DPI. Final focused native checks for changed-global-settings Undo and repairing a reimported watermark version also passed (3 results each).

Output checks cover 80/100 JPEG quality with Original/75%/50% for JPEG, PNG and synthetic sensor DNG, distinct actual encodings at 80/95/100, full sensor dimensions rather than embedded preview export, sRGB profiles and transfer curve, Display-P3 conversion, orientation, metadata on/off, PNG alpha, TIFF endian variants, retained camera/date/exposure/GPS, removed stale thumbnails/MakerNotes, malformed metadata and exact shared edit/crop/rotation/watermark-before-resize pixels. A direct LibRaw reference verifies the output is closer to the configured sRGB curve than the former BT.709 default. Persistence, captured retry/undo/refresh/recovery settings and unchanged originals passed alongside all previous-phase regression suites.

The native **510-file** queue actually developed/decoded, resized, encoded, synchronized, fingerprinted and published **10 synthetic 512×384 sensor DNGs, 250 JPEGs and 250 PNGs**. JPEG/PNG sources were 120×80; all used 50% resolution. It completed in **15,887 ms**, with **1,495 GUI timer ticks**, a **27 ms maximum timer gap**, **one peak processing worker**, zero failures and 510 outputs. Every source was byte-checked afterward. These are small synthetic fixtures, not a throughput claim for a camera shoot; another automated test process was running concurrently. The complete native output-test process had a sampled peak working set of **101,883,904 bytes (~97.2 MiB)**, polled at 100 ms; this includes tests/Qt/fixtures and is not a bound for large-RAW processing. Buffer ownership now avoids one extra full RGB888 bitmap copy (72,000,000 bytes for a hypothetical 24 MP RGB image); no measured large-camera peak-RAM reduction is claimed.

The packaged executable matches the final build SHA-256. Launching with PATH restricted to Windows system directories exited **0** for a fresh session and copied schema 1, 2 and 3 sessions; all initialized/upgraded to schema **4**. Existing database rows/columns were compared before/after migration. The extracted SDK deployment helper's child `qtpaths` query failed despite direct queries working; the refreshed package reused the existing identical-version Qt/LibRaw/runtime DLLs (no new dependencies) and passed these standalone runtime checks. This is a local package smoke test, not clean-machine certification.

**Still unverified:** real camera RAW/Sony ARW color, fine detail/noise/highlight comparisons; 500+ real large-RAW throughput and peak memory; actual power-loss/disk-full/offline-drive behavior; MSVC, Windows 10, clean machines and mixed-DPI monitor transitions. The user had no real RAW samples available. Supported EXIF preservation is intentionally partial: MakerNotes, thumbnails, unknown/vendor tags and IPTC/XMP are omitted on re-encoding; non-TIFF RAW containers use basic LibRaw camera/exposure fallback fields. Exact-copy jobs retain all original bytes. Older PhotoCull versions cannot reopen schema 4; keep backups with the app closed.


## Sure Sort workflow verification — 18 October 2026

The final Windows 11/MinGW Release build passed **6/6 CTest suites in 54.24 seconds**, with **177 Qt Test results** (22 viewer, 20 sorting, 30 watermark, 42 editing, 33 output, 30 workflow; includes setup/cleanup/data rows). The workflow suite also passed all **30 native Windows results in 10.455 seconds**, without failures/skips. The refreshed header, editing panel, folder action labels, settings/fullscreen icons and footer were visually inspected at 150% DPI.

New checks cover multiple folder copies with persistent action-specific Undo, edited original relocation, continued copying from moved sources, exact RAW+JPEG original restoration, chained/rapid moves and Undo, Clear/Undo replay, archive filename collisions, occupied original paths, changed moved files, restore Retry, edit-refresh ownership and cancellation, and Undo arriving during cancellation cleanup. Recovery cases exercise planned/prepared/published transfers, publication before its journal update, source deletion before completion, prepared/published reverse transfers and chained restoration order after restart. A populated simulated schema-4 session verifies legacy Ctrl+Z history survives migration. Interface checks cover all six five-unit shortcuts (0.5 EV exposure), Original crop ratio in six orientation/aspect combinations, 100%/Fit toggling, fullscreen, the current-shortcuts reference, settings actions and the Undo button. Earlier output-quality/metadata and 510-file responsiveness tests remain passing.

The new **dist/Sure Sort/Sure Sort.exe** matches the final build SHA-256. With PATH restricted to Windows system directories, packaged launch exited **0** for a fresh session and copies of schemas **1, 2, 3 and 4**; every database initialized/upgraded to schema **5**. Existing rows/columns in those fixtures compared equal before/after migration. No new runtime dependencies were added; the existing verified Qt/LibRaw/compiler runtime bundle was reused. The application retains the preceding default session location despite the name change.

Original transfer buffers are limited to 1 MiB per streaming copy; no full-resolution image cache was added. Original data and supported modification times are copied before verified removal. Real-camera RAW quality comparisons, sustained large-RAW/cross-volume throughput and RAM, removable/offline/full drives, physical power loss, Windows 10/MSVC and clean-machine/mixed-DPI deployment remain manual checks. No timings for those scenarios are claimed.


## UI cleanup and automatic retry verification — 18 October 2026

The Windows 11/MinGW Release build passed **6/6 CTest suites in 70.63 seconds**, with **184 Qt Test results** (22 viewer, 20 sorting, 30 watermark, 42 editing, 33 output, 37 workflow, including setup/cleanup/data rows). All **37 native Windows workflow results passed in 22.783 seconds**. These runs overlapped; they are regression timings, not a photo throughput benchmark. No new peak-memory measurements were taken.

Native screenshots and geometry checks verified the destination editor, main footer and terminal error display at **100%, 150% and 200% effective scaling** (the 100% and 200% checks used Qt scale-factor overrides on the 150% desktop). Folder action controls fit their rows; plus/bin controls work after row deletions; Ctrl+Z works without a navbar button. Retry tests cover captured settings and unchanged history, two-retry limits across restart, terminal error visibility, cancellation/Undo suppression, publication collisions without another decode, original move/restore failures and recovery from interrupted automatic cleanup. The existing 510-file processing/responsiveness and original-byte regression tests passed.

The refreshed **dist/Sure Sort/Sure Sort.exe** matches build SHA-256 **54CF006942F418AAD6DD0C8863B2959D2289876266777A8D7CD0CBAC0D03A1F5**. With PATH restricted to Windows system directories, fresh and copied schema **1–5** synthetic session launches all exited **0** and initialized/upgraded to **schema 6**. Existing rows/columns compared equal after those fixture migrations; a populated legacy migration is also exercised by Qt Test. The same runtime DLL bundle was reused. Real drive failures, physical power loss, real-camera quality/large-RAW performance and clean-machine/mixed-monitor deployment remain the manual checks described above.

## 0.1.1 navigation check — 4 November 2026

The same comparison harness and 100 synthetic 6000×4000 JPEG files were used before and after this change, on this Windows machine with the same Qt 6.8.3/MinGW 13.1/LibRaw 0.21.4 release libraries. The target was 1920×1080 physical pixels, the cache 256 MiB, and 39 navigation steps used 150 ms dwell. Images were delivered to and repainted by the real PhotoViewer using Qt's offscreen platform. Other system activity was uncontrolled and the filesystem cache was not flushed.

| Single-run measurement | Previous scheduler | 0.1.1 scheduler |
| --- | ---: | ---: |
| First preview | 151.73 ms | 115.62 ms |
| Navigation median | 16.08 ms | 2.03 ms |
| Navigation p95 | 153.63 ms | 2.96 ms |
| Final settle after 60 steps at ~30/s | 110.56 ms | 111.37 ms |
| Navigation CPU time | 7531 ms | 5938 ms |
| Peak working set | 300.75 MiB | 302.73 MiB |

This verifies improved steady browsing on this fixture. Rapid held-key settling did not improve here. Real-camera workloads, saving-load p95 and other hardware remain unmeasured. No claim is made that output-copy throughput improved: SHA-256 verification, synchronization, no-overwrite publication, and verified Undo remain enabled.

The scheduler retains useful neighbors, follows direction, allows two background previews and reserves one slot for current/detail work. Catalog sorting caches each display name/path once; per-photo assignment queries now use an index. Unit checks cover bounded work, retained neighbors, latest-generation delivery, tiny cache budgets, import/export collisions and existing recovery/ownership protections.
