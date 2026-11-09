# Real-camera and Windows release checklist

These are manual release gates, **not completed verification claims**. Generated DNG fixtures cover API behavior but cannot certify real cameras. Record camera model, firmware, compression/mode, LibRaw version/options, OS, Qt kit, display scale and outcome. LibRaw's [supported cameras](https://www.libraw.org/supported-cameras) describes upstream capabilities; this project has not verified those cameras.

- [ ] Launch the deployed folder on Windows 10 and 11 x64 without Qt/vcpkg on PATH. Check fonts, JPEG, bitmap previews and half-size fallback.
- [ ] Import empty, 1,000-photo and 10,000-photo folders; include mixed-case extensions, IMG_2/IMG_10, JPG/RAW pairs and separate PNGs. Verify all nested subfolders are included, even when the selected folder contains no photos directly. Matching names in different subfolders must remain separate; RAW/JPEG pairs must stay within their own directory.
- [ ] Test genuine ARW, CR2, CR3, NEF, DNG, RAF, RW2 and ORF from intended cameras, including compression variants explicitly supported by the LibRaw build. Unsupported files must report an error and allow navigation.
- [ ] Compare landscape, portrait, mirrored and already-oriented embedded previews with known-good renderings. Confirm no double rotation and multiple-preview selection at different window sizes/DPI.
- [ ] Test JPEG, bitmap and absent/corrupt RAW previews. Foreground fallback shows loading; prefetch and 100% never develop sensor data.
- [ ] Verify 100% and F, including RAW previews smaller than sensor dimensions. Preview remains visible during detail load; fit releases detail memory. Compare embedded sRGB, Display P3 and Adobe RGB profiles with reference sRGB conversions.
- [ ] Hold Right and alternate Right/Left during loading, zooming, resizing and folder switches. No stale images should flash. Check Home/End and autorepeat boundaries.
- [ ] Verify all shortcuts, wheel/trackpad cursor anchoring, drag pan, fullscreen/Esc, 100/125/150/200% DPI and mixed-DPI monitors. Open/close the native folder picker and ensure shortcuts resume.
- [ ] Remove files after import, deny folder access, disconnect external/network drives, corrupt JPEG/RAW files, and close during decoding/scanning. Short messages, continued navigation, no crash. Slow OS reads may delay final process shutdown.
- [ ] Monitor working set, private bytes, CPU and handles over a long 10,000-photo session. Cache-accounted bytes stay within budget; total process memory additionally includes current/detail images, decoder temporaries, libraries, queued results and GUI surfaces.
- [ ] Compare originals' hashes before/after viewing and sorting. All source photographs must stay byte-identical; outputs belong only in configured destinations.
- [ ] Measure release startup and keyboard-to-present latency with a profiler/capture. Record hardware and filesystem cache conditions. Cache lookup timings cannot establish one-frame UI latency.

## Phase 2 automatic processing

- [ ] Configure output, add/rename/remove folders with plus/bin icons, use digits and custom modifiers, check conflicts and restore configuration after restart. Include more than ten categories and use the horizontally scrollable button bar.
- [ ] Sort a 1,000-photo mixed shoot while RAW processing backs up. Assignment immediately advances; holding a destination key produces one assignment. Arrows repeat; Space skips without saving.
- [ ] Compare full-sensor RAW JPEG dimensions/orientation/color with a known-good development. RAW+JPEG produces one output. Verify JPEG/PNG copies are byte-identical. Check EXIF/IPTC limitations before production use.
- [ ] Undo, clear and reassign queued, active and completed jobs; change folders during work. Only the current assignment should eventually own an unchanged generated output.
- [ ] Modify, replace, move or lock completed outputs, then undo/reassign. Preserve user data and display a notice. Test filename collisions and a file created at the chosen destination while processing.
- [ ] Disconnect the output drive, fill the disk, deny access and corrupt/delete a RAW. Fail safely without partial final files; reconnect/fix during the 1/3-second automatic retries; confirm a visible error if both retries fail, then reassign.
- [ ] Undo queued/active work; verify pending retries do not resurrect undone assignments. Close during RAW decode, encoding and finalization; unfinished jobs resume without duplicate final outputs.
- [ ] Test crash recovery at the prepared/renamed/completed boundaries with disposable files. Preserve unverified interrupted `.part` files. Restore last input/photo and persistent undo; verify the session lock rejects a second writer.
- [ ] Validate the packaged SQLite driver and a new session on a clean machine. Test existing databases and backups while the application is closed.

## Phase 3 watermark

- [ ] Select a transparent club PNG; try all four corners and size, opacity and padding sliders. The current photo updates immediately with no Apply/Export. Browse while settings remain open and reopen the app to verify restoration.
- [ ] Compare preview and saved output on landscape, portrait, square, mirrored and rotated camera images. Size/padding use the shorter photo side; logo aspect ratio/alpha remain correct. Large/tall logos must stay within photo margins.
- [ ] Zoom, pan, resize, fit, switch photos and move between 100/150/200% or mixed-DPI displays. The overlay follows the image boundaries, including when its corner is outside the viewport.
- [ ] Sort JPEG and real RAW/RAW+JPEG. Check full output dimensions, orientation and color with a reference viewer. RAW embedded previews can differ in crop/color from development. Review the EXIF/IPTC re-encoding limitation.
- [ ] Disable watermarking: overlay disappears immediately and future unedited JPEG/PNG copies with default output preferences stay byte-identical. Opacity zero is still an enabled re-encoding job.
- [ ] Queue a large mixed shoot, change settings/logo, delete/replace the imported PNG, close/restart, Retry and Undo. Existing assignments must retain their captured settings/PNG versions; navigation must stay responsive.
- [ ] Try corrupt/non-PNG/oversized logos and damaged saved asset data in a disposable session. Keep the prior valid logo on failed import; enabled jobs with missing assets fail safely without unwatermarked outputs.
- [ ] Upgrade a backed-up Phase 2 session: prior jobs remain unwatermarked and history/configuration survive. Current schema 4 includes editing and output preferences; keep a backup for older app versions.

## Phase 4 editing

- [ ] Check each tone slider independently and combined on real JPEG/RAW, including bright/dark areas, skin and saturated colors. Compare edited preview with final sRGB output; RAW development/embedded-preview differences remain possible.
- [ ] Hold every edit key with repeated arrows, release and navigate/sort. Customize every editing action, reset/crop bindings and disabled rows; test duplicate/navigation/destination conflicts and restored settings.
- [ ] Toggle the panel, double-click each slider, Reset All, switch photos/folders, close/restart and verify per-photo persistence. Originals must remain byte-identical and unedited copies must retain metadata.
- [ ] Draw, move and resize all crop ratios in landscape/portrait/square and all rotations. Zoom, pan, fit, resize and switch DPI; Enter/Esc must commit/cancel the intended original region. Check small selections and photo edges.
- [ ] Confirm watermark size/padding follows the final crop after rotation, both during selection and in output. Reject sorting/navigation during a draft; keep other controls usable.
- [ ] Adjust rapidly on 24 MP JPEG and intended camera RAW while sorting a large mixed batch. Monitor input latency, idle detail refinement, CPU and peak memory; no full sensor RAW work should run for slider changes.
- [ ] Edit already sorted photos repeatedly: settle to one updated generated file, retain destination, use current watermark, and avoid a second viewer advance. Change edits while an older job runs and confirm the latest state follows.
- [ ] Modify, replace, lock or move a sorted output during refresh. Preserve external files, choose safe free names or show a failure. Test disk-full/offline drive, missing/corrupt source, cancellation, Retry and Reset All back to exact copying.
- [ ] With disposable sessions, interrupt refresh before encoding, after prepared journal, after predecessor removal and after rename. Recover without duplicate app outputs; verify Undo/Clear use the new published identity.
- [ ] Upgrade backed-up Phase 2/3 sessions to current schema 4; preserve old snapshots, destinations, watermarks and history. Complete Windows 10/MSVC/clean-machine/mixed-DPI checks before release.


## Phase 5 release checks

- [ ] Compare full-resolution Sony ARW and representative camera RAW with a trusted developer at matching camera WB. Inspect skin, saturated colors, fine detail, noise, highlights, orientation and full sensor output dimensions. Compare embedded preview versus development without expecting identical camera tone/crop.
- [ ] Test JPEG 80/95/100, Original/75%/50%, metadata on/off, edited crops/rotations/watermarks and transparent PNG. Inspect retained camera/lens/exposure/date/GPS in an independent metadata viewer; verify normalized orientation/dimensions and omitted stale thumbnails/MakerNotes.
- [ ] Sort 500+ real mixed/RAW photographs while browsing/editing. Measure peak RAM, conversion times and UI latency; test interruption, disk-full/offline destinations, Retry, collisions, reassignment and Undo. Existing jobs retain captured output settings; originals remain byte-identical.
- [ ] Upgrade a backed-up Phase 4 session to schema 4, retry old jobs and confirm settings/history. Complete Windows 10/MSVC/clean-machine and mixed-DPI checks. Real camera samples were unavailable during automated Phase 5 verification.


## Sure Sort Copy/Move and interface

- [ ] Set different folder rows to Copy and Move. Copy a photo into two folders by returning to it; keep both copies. Move an edited/watermarked RAW+JPEG pair and verify the processed JPEG plus exact source files in Originals.
- [ ] Undo copies and chained moves rapidly, after restart and during processing. Verify original bytes/prior paths; preserve occupied or externally changed files and finish automatically when conflicts are resolved during retry, or through restart restoration recovery. Interrupt forward and reverse transfers on disposable shoots, including cross-volume/removable drives.
- [ ] Browse/reopen moved photos, zoom, edit and make further copies; inspect partially moved pairs during recovery. Compare timestamps and confirm there are no lost originals.
- [ ] Verify five-unit editing steps (0.5 EV exposure), Original crop ratio through rotation/photo changes, Z 100%/Fit, fullscreen and settings icons, Ctrl+Z with no navbar Undo button and the complete current-shortcuts reference. Inspect small/high-DPI windows, menus, controls and keyboard focus.

- [ ] Inspect Settings → Sorting destinations at 100%/150%/200% DPI: readable Copy/Move and shortcut controls, per-row bins, right-hand plus, no Up/Down/global Remove. Confirm two compact footer rows, horizontally scrolling folders and no Retry/Cancel controls; unresolved automatic retries show an error with clickable status details.
