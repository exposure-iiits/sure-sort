# Testing guide

## Automated suite

Build using the [README](../README.md) and set runtime PATH for that build. Run:

```powershell
ctest --preset windows-release
```

`folder_scanner_tests` verifies recursive folder scanning with only Qt Core and Qt Test at runtime. It covers nested/hidden/Unicode directories, filenames shared by different subfolders, RAW/JPEG pairing, stable IDs across parent/direct imports, cancellation and asynchronous folder changes. Run it alone with `ctest --preset windows-release -R folder_scanner_tests`.

CTest runs `photocull_tests` with `QT_QPA_PLATFORM=offscreen` and a 90-second timeout. To run a focused case directly:

```powershell
$env:QT_QPA_PLATFORM = 'offscreen'
& .\build\windows-release\photocull_tests.exe latestRequestWinsAndBoundedWork
& .\build\windows-release\photocull_tests.exe syntheticRawPreviewAndFallback
& .\build\windows-release\photocull_tests.exe 'orientation:6'
Remove-Item Env:QT_QPA_PLATFORM
```

CTest also runs `sorting_tests` (120-second timeout). Focused examples, using the same runtime environment:

```powershell
$env:QT_QPA_PLATFORM = 'offscreen'
& .\build\windows-release\sorting_tests.exe rawSensorDevelopmentAndPair
& .\build\windows-release\sorting_tests.exe recovery
& .\build\windows-release\sorting_tests.exe windowKeyboardWorkflow
Remove-Item Env:QT_QPA_PLATFORM
```

The sorting suite covers configuration/shortcut conflicts and auto-repeat, exact copying/collisions, full-sensor RAW+JPEG conversion, 200 assignments while a processing worker is blocked, queued/running/completed undo, modified/replaced output preservation, retries, locked sessions, prepared and published crash windows, partial temps, close/restart recovery, folder mapping persistence, keyboard auto-advance/skip/clear/undo and dialog add/remove/reorder/rename. Its DNG assertion checks the oriented 384×512 sensor output rather than the 80×40 embedded preview, including the sRGB profile.

Every window test passes a temporary session path. Native Phase 2 screenshots can be saved with `PHOTOCULL_PHASE2_SCREENSHOT` while running `sorting_tests windowKeyboardWorkflow` with `QT_QPA_PLATFORM=windows`.

CTest also runs `watermark_tests` (120-second timeout). It includes twelve landscape/portrait/square corner rows, relative scaling/fit constraints, PNG alpha × opacity, sRGB/grayscale, original/cache immutability, viewer/output pixel comparison, resize/zoom/pan/navigation, live controls/import, full-resolution JPEG/PNG/sensor DNG processing, EXIF orientation, transparent PNG output, immediate advance, queued logo versions, retry/undo snapshots, a 40-output batch with a live GUI timer, 200 shared-logo assignments, persistence/recovery, damaged assets. Run it with the same runtime PATH:

```powershell
$env:QT_QPA_PLATFORM = 'offscreen'
& .\build\windows-release\watermark_tests.exe
Remove-Item Env:QT_QPA_PLATFORM
```

Use `QT_QPA_PLATFORM=windows` for native rendering. Set `PHOTOCULL_PHASE3_SCREENSHOT` to an absolute PNG path to capture the window and a second `.dialog.png` during `watermark_tests windowKeyboardWorkflow`. Inspect both for clipping and control usability. The fixture logo/photo are generated solid colors so exact bounds and output pixels can be checked without private images.

CTest runs `editing_tests` with a 120-second timeout. Coverage includes individual/combined tone adjustments, all quarter turns, original-relative crop at multiple zoom levels, five ratios, corner resizing/moving/panning, edit reset, latest-request rendering, idle refinement/cancellation, image DPR, all default repeated chords and customizable/conflicting bindings, full-resolution JPEG/PNG/synthetic RAW processing, edited preview/output agreement, per-photo/shortcut persistence, automatic refresh/coalescing, reset-to-copy, external modifications, failed/cancelled refresh/retry, publication races/recovery, saved folder keys and a complete keyboard/edit/crop/sort window workflow.

```powershell
$env:QT_QPA_PLATFORM = 'offscreen'
& .\build\windows-release\editing_tests.exe
& .\build\windows-release\editing_tests.exe preparedRefreshRecoveryAndRace
Remove-Item Env:QT_QPA_PLATFORM
```

For native UI inspection, set `QT_QPA_PLATFORM=windows` and `PHOTOCULL_PHASE4_SCREENSHOT` to an absolute PNG path. `editing_tests windowWorkflowAndNavigation` captures the edited viewer/panel and a `.crop.png` with the active draft. Keep tests isolated using their temporary sessions. `photocull_bench` now also reports combined tone render timings at 1920×1080 and 24 MP, excluding decode/encoding/presentation.

Substitute `build/mingw-release` for the existing local verification kit. For Linux, use `ctest --preset linux-release` after the matching build. Historical results and their actual platforms are in [PERFORMANCE](PERFORMANCE.md); these commands do not imply other platforms have passed.

If the runner's default temporary directory refuses file writes:

```powershell
New-Item -ItemType Directory -Force build/tmp | Out-Null
$env:TEMP = "$PWD\build\tmp"
$env:TMP = $env:TEMP
```

These variables apply to the current shell and its child processes.

## Coverage and regression checks

| Area | Existing test cases |
| --- | --- |
| Import/catalog | `folder_scanner_tests` covers nested/hidden/Unicode folders, natural sort, per-directory pairing, stable IDs, cancellation and asynchronous folder switching; `filteringPairingAndSort`, `emptyInvalidAndCancelledScan`, `asynchronousScanSwitchAndShutdown` also run in `photocull_tests` |
| Cache | `byteAccurateLru` — padded scanlines, eviction, budget changes and oversize rejection |
| Standard decode | `scaledDecodeAndColor`, `orientation` — generated EXIF 1–8, physical target limits and ICC conversion |
| RAW | `previewRankingAndMalformedMetadata`, `syntheticRawPreviewAndFallback` — ranking, JPEG/bitmap, Unicode, EXIF/fallback and detail policies |
| Errors | `missingCorruptAndCompanionFallback` |
| Async scheduling | `latestRequestWinsAndBoundedWork`, `folderSwitchResizeAndDetail`, `asynchronousScanSwitchAndShutdown` |
| Input/view | `shortcutsAndHoldChords`, `viewerAndWindowSmoke` |

`tests/raw_fixture.h` writes synthetic DNGs; JPEG EXIF data is generated in the test itself. Delayed injected decoders exercise concurrency limits and stale delivery without depending on disk timing. Keep camera-model compatibility claims separate from these synthetic tests.

For a defect, add a test that reproduces the observable failure when practical. Check both fit and detail requests for decoder changes, rapid navigation and folder switching for scheduling changes, and byte accounting for cache changes. Avoid assertions that merely duplicate the implementation.

## Native UI and deployment checks

On Windows, with the matching runtime PATH:

```powershell
$env:QT_QPA_PLATFORM = 'windows'
$env:PHOTOCULL_TEST_SCREENSHOT = "$PWD\build\viewer-native.png"
& .\build\windows-release\photocull_tests.exe viewerAndWindowSmoke
Remove-Item Env:PHOTOCULL_TEST_SCREENSHOT
Remove-Item Env:QT_QPA_PLATFORM
```

Inspect the saved rendering for labels, clipping, aspect ratio and DPI scaling. Run the full [manual checklist](MANUAL_TESTS.md) for gestures, real cameras, drives, permissions and monitor transitions.

After packaging, the launch smoke command is:

```powershell
$launch = Start-Process -FilePath "$PWD\dist\PhotoCull\PhotoCull.exe" -ArgumentList @('--session-db',"$PWD\build\smoke.sqlite",'--smoke-test-ms','800') -WindowStyle Hidden -Wait -PassThru
$launch.ExitCode
```

An exit code of 0 verifies this launch only. Also test on a clean Windows machine without development SDK paths; confirm actual images load and expected plugins/DLLs are present.

Quote the session-path argument when passing paths containing spaces through PowerShell's Start-Process argument list. Confirm `Qt6Sql.dll`/`sqldrivers/qsqlite.dll` are present and the app actually initializes a copy-only database; viewer-only startup can still succeed if sorting initialization fails. Do not use or modify a real session database during automated verification.

## Performance and reporting

Run `photocull_bench` from a Release build with no argument for 1,000 entries, or `10000` for the larger scan. Record timings only after executing the benchmark. Cache lookups are not end-to-end navigation measurements.

For a test or bug report, include the source revision (and local changes), OS, compiler/configuration, Qt and LibRaw versions, camera/mode if relevant, display scale, reproduction steps, expected/actual behavior and relevant output. Store measured conditions/results in [PERFORMANCE](PERFORMANCE.md); mark unexecuted manual checks as pending. Do not add private photos or identifying metadata to reports without permission.


## Phase 5 checks

`output_tests` (240-second timeout) verifies JPEG/PNG/sensor-DNG dimensions at 100/75/50%, quality 80/100 and real encoding differences at 80/95/100, sRGB/orientation, original-byte preservation, endian-independent EXIF camera/date/exposure/GPS retention, stale thumbnail/MakerNote removal, metadata disabling, PNG alpha/eXIf round trips, malformed metadata, exact tone/crop/rotation/watermark-before-resize pixels, settings persistence and captured retry/undo/refresh/recovery settings. A 510-file batch (10 synthetic sensor DNG, 250 JPEG, 250 PNG) actually processes/encodes/publishes each file, checks unchanged originals and asserts one worker while a GUI timer keeps firing. Native output dialog changes and app integration are also exercised. See PERFORMANCE for measured run results and limits. Synthetic fixtures cannot validate real Sony ARW color/detail quality.


## Copy-only workflow and folder progress

workflow_tests retains copy/Undo/retry/recovery, folder controls, shortcuts, crop/fullscreen and actual destination-count checks. viewer_tools_tests checks completed outlines and explicit info/histogram dismissal. folder_progress_tests covers full setup restoration from a renamed original folder and a fresh app profile, independent folder sessions sharing one worker, removal during refresh, modified-output preservation, interrupted copying/removal, missing outputs, nested output exclusions, original PNG/RAW/JPEG bytes, read-only local fallback/synchronization footer copy toggles, cancellation, the photo-to-folder animation and responsive navigation.

Run the full CTest suite in build/portable-release with the matching .deps and MinGW DLL paths. For native screenshots, run folder_progress_tests footerFolderTogglesAndPersistentPanels with QT_QPA_PLATFORM=windows and SURESORT_UI_CAPTURE set to an absolute output directory. It captures the green folder outlines at 1280/640 widths. The copyFlightUsesPreviousPhotoAndAllowsNavigation case captures the short copy animation and checks that it uses the assigned photo before auto-advance. Existing SURESORT_UI_CAPTURE viewer/workflow tests capture the panel, settings and thumbnail layouts.

All tests use temporary profiles and original-photo folders. They must never use a real user session. Earlier session-format migration fixtures are removed because the new format intentionally starts fresh.
