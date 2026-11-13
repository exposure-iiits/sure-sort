# Development guide

## Setup and daily workflow

Use the [build guide](BUILDING.md) for the full dependency installation procedure. The intended Windows toolchain is MSVC 2022 x64, official Qt 6.8, CMake/Ninja and vcpkg manifest-mode LibRaw. Qt and LibRaw must use the same architecture and compatible compiler/runtime. Keep their linkage dynamic.

From a configured MSVC x64 PowerShell, with `QT_ROOT` and `VCPKG_ROOT` set:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release --parallel 4
$env:PATH = "$env:QT_ROOT\bin;$PWD\build\windows-release\vcpkg_installed\x64-windows\bin;" + $env:PATH
ctest --preset windows-release
& .\build\windows-release\PhotoCull.exe 'D:\Photos\TestCollection' --cache-mb 256
```

Use disposable copies when exercising filesystem failure cases. Original photo bytes remain read-only; assignments write outputs and a per-folder .suresort-progress.sqlite progress database.

For a separate Debug build, use a Qt kit containing debug libraries:

```powershell
cmake --preset windows-release -B build/windows-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/windows-debug --parallel 4
$env:PATH = "$env:QT_ROOT\bin;$PWD\build\windows-debug\vcpkg_installed\x64-windows\debug\bin;" + $env:PATH
ctest --test-dir build/windows-debug --output-on-failure
```

Open the root `CMakeLists.txt` in a CMake-aware IDE and select the same toolchain/build directory for debugging. Debug builds are for diagnosis; benchmark Release builds only. The existing local verification build is `build/mingw-release`; it is a separate MinGW environment, not the MSVC preset. See [recorded verification](PERFORMANCE.md).

## Source map

| Files | Responsibility |
| --- | --- |
| `src/photo.h`, `src/folder_scanner.*` | Stable photo IDs, source lists, extension filtering, pairing, sorting and asynchronous import |
| `src/image_decoder.*` | Standard image decode, RAW preview extraction, orientation, sRGB conversion and foreground fallback |
| `src/image_cache.*` | Target-size keys, actual byte accounting and LRU eviction |
| `src/preview_controller.*` | Catalog selection, generations/epochs, cancellation, foreground/detail/prefetch scheduling |
| `src/photo_viewer.*` | Image painting, fit/100%, zoom, pan and viewport/DPI notifications |
| `src/shortcut_manager.*` | Named action bindings, held-key tracking and chord dispatch |
| `src/sorting_types.*`, `src/destination_dialog.*` | Destination validation, ordered mappings and shortcut editing |
| `src/output_settings.*`, `src/output_dialog.*` | Validated output preferences and compact automatic settings UI |
| `src/exif_metadata.*` | Bounded standard EXIF parsing/rebuild and streaming metadata insertion |
| `tests/test_output.cpp` | Output quality/resolution/metadata, snapshots, restart and 510-file queue |
| `src/ui_icons.h` | Lightweight painted toolbar icons |
| `tests/test_workflow.cpp` | Copy/removal/Undo, recovery, crop ratio, shortcuts and refreshed UI |
| `src/session_store.*` | SQLite schema, lock, task/assignment journal and undo history |
| `src/sorting_controller.*` | Auto-advance, serial processing queue, retry/cancel, undo and recovery |
| `src/processing_pipeline.*` | Full-sensor development, copies, temporary encoding, atomic publication and verified cleanup |
| `src/watermark.*` | Immutable PNG assets, validation, shared relative geometry, sRGB/alpha composition |
| `src/watermark_dialog.*` | Modeless live controls, bounded asynchronous PNG import, no Apply action |
| `src/photo_edits.*` | Edit value model, strict serialization, tone engine and original-relative crop/rotation helpers |
| `src/edit_panel.*`, `src/edit_shortcuts_dialog.*` | Collapsible controls, slider reset, crop ratios and editing binding configuration |
| `src/main_window.*`, `src/main.cpp` | Window composition, application startup and CLI options |
| `tests/test_photocull.cpp`, `tests/raw_fixture.h` | Qt Test coverage and generated DNG fixtures |
| `tests/test_sorting.cpp` | Sorting, conversion, safety, restart and keyboard/dialog integration |
| `tests/test_watermark.cpp` | Geometry, live controls/transforms, full-resolution output, snapshots, batches and copy-only persistence |
| `tests/test_editing.cpp` | Tone, keyboard/crop interactions, preview/refinement, persistence, output refresh and recovery |
| `tools/benchmark.cpp` | Repeatable release scan/decode/cache measurements |
| `tools/deploy-windows.ps1` | Assemble executable, dependency DLLs, Qt plugins and included notices |
| `cmake/FindLibRaw.cmake` | Adapt vcpkg/system LibRaw into one imported target |

## Implementation constraints

- Keep UI, filesystem scanning and decoders separate. Workers produce QImage; UI operations and any future QPixmap creation belong on the GUI thread.
- Preserve latest-request-wins behavior. A result must match the active folder and navigation request before display; adopting matching in-flight work must be explicit.
- Bound running and pending work. Keep foreground ahead of detail/prefetch, with one background prefetch at a time after the foreground result.
- Admit only display-target-sized previews into the LRU, charged by `QImage::sizeInBytes()`. Keep full detail outside the prefetch cache and release it on fit/navigation.
- Own one LibRaw instance per job and release resources through RAII. Browse embedded previews first; only a foreground fit request may use half-size sensor fallback. RAW detail/prefetch must not develop sensor data.
- Preserve Unicode paths and orientation handling, including explicit embedded orientation 1 and EXIF synthesized by LibRaw.
- Folder changes cancel/invalidate without waiting on the GUI thread. Final shutdown cancels and drains workers after the event loop; workers never wait for GUI delivery.
- Destination keys may use digits; validate conflicts and ignore assignment auto-repeat. Do not copy GPL reference application code.
- Keep processing, preview decoding and live tone rendering in separate pools. Only the processing pipeline develops full sensor data; unedited JPEG/PNG with watermark disabled and default output preferences remain byte copies.
- Share watermark geometry/composition between viewer and output. Never bake overlays into cached previews. Capture both settings and PNG bytes/version at assignment; retry/undo must preserve that snapshot.
- Keep decoded PNG assets immutable and shared; release historical decoded images. Persist asset bytes once per SHA-256 version and bound PNG import size. Use the fresh copy-only schema and fail safely on damaged saved snapshots.
- Reuse the shared tone/geometry helpers for preview and output. Keep crop in original normalized coordinates; apply watermark after rotation/crop. Capture edits in each task and preserve original preview cache entries.
- Bound live tone work and prefer the latest request; refine large images after idle without blocking navigation. Clear refinement timers/generations on photo change and cancel/drain on destruction.
- Refresh sorted outputs only after rendering/journaling a replacement. Preserve predecessor ownership, cancellation/retry and prepared/renamed recovery. Never overwrite an externally modified image.
- Commit prepared metadata before publication. Never overwrite an output path or delete an output without proving ownership/content under the Windows handle lock. Persist undo/reassignment before starting new work.
- Folder sessions share a single processing pool. App viewing preferences and the recent-folder index stay in the profile; relative source identities and all sorting state stay in each original folder’s progress file. Qt SQL/QSQLITE are required. Tests must use temporary session paths; use `--session-db` for isolated app launches rather than a developer's normal database.

See [DESIGN](DESIGN.md) for the reasoning and [TESTING](TESTING.md) for checks affected by each area.

## Generated files and troubleshooting

`build/`, `.deps/`, `.reference/`, `dist/`, IDE `*.user` files and `CMakeUserPresets.json` are ignored. Keep personal paths in environment variables or user presets. Do not add private camera images, SDK binaries or benchmark output directories to source control.

If configuration cannot find Qt, check that `QT_ROOT` points at the kit containing `lib/cmake/Qt6`. For LibRaw, check `VCPKG_ROOT`, the x64 triplet and the manifest restore output. A runtime plugin/DLL error usually requires the matching Qt/dependency directories on PATH or a complete deployed folder; do not mix MSVC and MinGW DLLs.

For temporary-file permission failures, use the writable temp directory instructions in [TESTING](TESTING.md). A font warning from an offscreen test is not proof of a native Windows font problem; inspect the `windows` platform rendering separately. For unsupported RAW files, record the exact camera, compression and LibRaw build rather than inferring support from the extension.
