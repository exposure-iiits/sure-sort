# Sure Sort

A native, offline C++17/Qt Widgets viewer and keyboard sorting tool for Windows 10/11 x64. Sure Sort combines photo browsing, editing, watermarking and automatic keyboard sorting with copy-only folder sorting. Browsing, keyboard sorting, automatic RAW development and watermarking remain intact. Original photos stay unchanged. Sorting automatically saves the final image without an Export or Apply action.

Current version: **0.1.5**, shown in **Settings → About**. See [versioning](docs/VERSIONING.md).

For MacBook packaging, see the [experimental macOS cloud build](docs/MACOS.md).
It prepares a macOS `.dmg` on a Mac/GitHub runner; no Mac installer is included
in the Windows distribution. Verified output removal needs a macOS port.

For the GitHub source package and ongoing development, start with [START_HERE.md](START_HERE.md). Prebuilt Windows files are distributed separately from the source.

See [0.1.5 changes and controls](docs/RELEASE-0.1.5.md). The portable package is in `dist/Sure-Sort-0.1.5-Windows-x64-Portable.zip`.

## Run the local verification build

Portable builds include a `portable.ini` marker and keep the app profile and local progress fallback in `data/` beside the executable. Folder progress stays beside the original photos. Extract the complete ZIP to a writable folder and launch **Sure Sort.exe**. Share the original ZIP so your personal session history is excluded. `--portable` also enables this mode; an explicit `--session-db` takes precedence. The executable includes the minimal folder/checkmark app logo; editable artwork is in `assets/suresort.svg`.

Open `dist\Sure Sort 0.1.5\Sure Sort.exe`. Keep its adjacent DLLs and plugin folders together. The portable package uses Qt 6.8.3, MinGW GCC 13.1 and dynamic LibRaw 0.21.4. It is an unsigned local verification build. MSVC and Windows 10 have not been tested here. Generated files in `build/`, `.deps/`, and `dist/` are ignored by Git.

```powershell
& '.\dist\Sure Sort\Sure Sort.exe'
& '.\dist\Sure Sort\Sure Sort.exe' 'D:\Photos\Shoot' --cache-mb 256
```

Choose **Open Folder** or pass a folder on the command line. Scanning includes the selected folder and all its nested subfolders. Extension filters: JPG/JPEG, PNG, ARW, CR2, CR3, NEF, DNG, RAF, RW2, ORF. Actual RAW/camera decoding depends on the installed LibRaw build. Same-stem RAW/JPEG sources in the same directory form one photo, with JPEG preferred; PNGs and matching filenames in different subfolders remain separate. Source paths appear in the filename tooltip. RAW dimensions describe the selected oriented preview/fallback, not guaranteed sensor dimensions.

| Key / gesture | Action |
| --- | --- |
| Right / D, Left / A | Next, previous |
| Home, End | First, last |
| Ctrl+O | Open folder |
| Configured digits / custom keys | Toggle the folder’s copy; copying advances, removal stays on the photo |
| Space | Skip without assignment |
| Backspace | Clear the latest folder placement |
| F | Fit |
| Z | Toggle 100% (one image pixel per device pixel) and Fit |
| + / − | Zoom in/out (`=` also zooms in) |
| Wheel / trackpad scroll | Cursor-centered zoom |
| Left-drag while zoomed | Pan |
| F11, Esc | Toggle fullscreen, leave fullscreen |

Digits 0–9 can be mapped to destinations. Sorting, skip, undo and clear ignore key auto-repeat; navigation still repeats. Editing bindings are configurable in the Edit panel; destination keys remain configurable in Destinations. Cache targets follow viewport × DPI. Zoom retains one separate full-resolution source image; for RAW this is the largest usable embedded preview. Editing adds current render buffers outside the cache. No-preview RAW files use asynchronous half-size development only for foreground fit browsing. Qt's 256 MiB per-image allocation limit may refuse unusually large full-detail images with a readable error.

## Sort and save automatically

1. Open the input folder, then click the **settings gear → Sorting**, or press **Ctrl+,**.
2. Select an output directory. Use the right-hand **+** to add subfolders and edit names/shortcuts, then click **Save folders**. Each row has a **bin icon** to remove that mapping. Duplicate names/keys and conflicts with viewer shortcuts are rejected. Blank shortcuts leave additional folders accessible by clicking their buttons.
3. Save the configuration, then press a folder key or click its button below the photo. The next photo appears immediately while processing runs in the background.

New folders use the next available number shortcut. The footer's **+** adds a name field directly beside the folder buttons; type the name and press Enter or leave the field to save, or Esc to cancel. If no output directory is set, Plus opens Settings → Sorting so you can choose and save it. Rename also happens inline. Settings can customize each folder’s shortcuts.

With no edits, watermarking disabled and default output settings, JPEG/PNG are copied byte-for-byte, including metadata. Otherwise the worker decodes the original, applies tone → rotation/crop → watermark → configured resize → encoding. RAW uses full-sensor development, camera white balance (automatic fallback), AHD where appropriate, an sRGB matrix and transfer curve, and JPEG encoding. A RAW+JPEG pair produces one JPEG from the RAW; a bad RAW fails rather than exporting its companion/embedded preview. Orientation is baked into processed pixels. Camera previews can differ from developed RAW in color, tone and crop.

Choose **Settings → Saving** for JPEG quality **80–100** (default **95**), **Original / 75% / 50%** dimensions and **Preserve EXIF where possible** (default on). RAW produces JPEG; JPEG/PNG retain their format. Quality changes affect JPEG; PNG remains lossless with alpha. Non-default JPEG quality, reduced dimensions or disabling metadata causes processing instead of an exact copy. Saving preferences save automatically and apply to future assignments; queued jobs, automatic retries and Undo retain their captured output settings. Reassign to the same folder to regenerate with new settings.

Supported EXIF includes camera/lens, exposure, capture dates, author/copyright and GPS when available in TIFF-based RAW, JPEG EXIF or PNG eXIf. Other RAW containers retain basic camera/exposure fields exposed by LibRaw. Processed metadata gets orientation 1, final dimensions and sRGB; stale thumbnails, MakerNotes, IPTC/XMP and unknown/vendor fields are not transferred. Exact-copy jobs retain all original bytes. No metadata library or new runtime dependency is added.

The compact footer groups folder buttons with processing counts, then photo/assignment details with dimensions and zoom. Failed processing and undo cleanup retry automatically after **1 second**, then **3 seconds**, using the same captured settings and verified file ownership. After two unsuccessful retries, an inline error appears; click processing status for full details. Fix the cause and reassign a failed photo to try again. Pending original restorations are also checked during restart recovery. There are no Retry or Cancel queue buttons. One low-priority processing worker limits simultaneous RAW conversions; preview workers remain separate. Existing names get `_2`, `_3`, etc.; publication never overwrites an occupied path.

Removal/reassignment cancel stale work and remove only verified, unchanged application outputs. Modified, replaced or locked files are preserved with a notice. Windows verifies/deletes using the same handle while denying concurrent writes/deletion. Other platforms conservatively preserve outputs on undo; Linux is unverified. Configuration changes affect future assignments; prior assignments retain their saved destination snapshots. Subfolders are created when first used.

## Copying, progress and removal

Click a footer folder or press its shortcut to copy the current photo there. A green outline on that folder means the selected photo has a copy there. Click or press the same shortcut again to remove that copy. Copying advances immediately; removing keeps the current photo selected. Pressing again while a copy is still saving cancels that pending copy.

Sure Sort automatically saves each opened folder’s session in **.suresort-progress.sqlite** beside its original photos. Reopening restores destination names, shortcuts, output location, edits, watermark/output settings, copy progress and the last viewed photo. Source paths are stored relative to the opened folder so its progress follows it. External output locations keep their absolute paths. Pending jobs remain attached to their folder while you browse another folder, with one shared processing worker.

Completed photos have a green thumbnail outline, and each footer folder containing that photo has a matching green outline. Folder clicks and keys toggle only that destination’s copy; other copies and the original stay intact. Saved destinations without a current shortcut mapping remain clickable in the footer. The thumbnail outline disappears after the last completed copy is removed. Modified, replaced or locked outputs are preserved with an error.  The visible photo bends and narrows into the folder with a short genie animation, while navigation and background saving continue.

If the original folder is unwritable, the notice **Progress saved on this computer only.** identifies a local fallback. On reopening after the folder becomes writable, local progress synchronizes back. Conflicting independent sessions are both preserved and a chooser selects which to resume. Folder progress uses the new copy-only format; earlier session formats are not imported.

The charcoal interface has a slim header, editing controls and consistently blue destination buttons. Circular badges count actual supported photos directly inside each destination, including pre-existing photos; subfolders are excluded and RAW/JPEG pairs count as one. Counts refresh after copies, removal and external changes. Destination folders inside the original tree are excluded from its photo catalog.

Photo information and Histogram stay open while clicking, browsing, editing and changing folders. Close with **×** or press the active toolbar button again. The other toolbar button switches between information and histogram. Escape retains crop/fullscreen behavior and does not dismiss these panels.

## Edit in the viewer

Choose **Edit** to open or collapse the compact side panel. Exposure, Brightness, Contrast, Highlights, Shadows and Saturation update the viewer immediately. Values appear beside each slider; double-click a slider to reset that adjustment, or use **Reset All**. The rotation slider runs from −180° to +180° in one-degree steps without changing the source file.

| Default editing shortcut | Action |
| --- | --- |
| Hold E + Right/Left | Exposure ±0.5 EV |
| Hold B / C / H / S / T + Right/Left | Brightness / contrast / highlights / shadows / saturation ±5 |
| Hold R + Right/Left | Rotate 90° clockwise/counterclockwise |
| Ctrl+Shift+R | Reset all edits |
| K | Start/cancel crop |
| Enter / Esc during crop | Confirm / cancel draft |

**Editing shortcuts** in the panel opens the Editing category directly. Double-click a Primary or Alternate shortcut and press its replacement key or chord to save immediately. Esc cancels capture; Delete clears a binding. Conflicts show a toast and keep the previous key. Settings and keyboard shortcuts stay inside the main window, and the active shortcut category is highlighted. Repeated arrow presses adjust while the hold key is active; normal navigation resumes on release. Existing destination keys take priority if an older session uses a new default hold key.

Choose **Crop** or press K, then drag a rectangle on the photograph. Drag inside to move it and drag a corner to resize. Select Free, Original ratio, 1:1, 4:3, 3:2 or 16:9. Original ratio follows the full oriented image, including rotation. The outside area dims and the watermark previews within the crop. Enter commits; Esc discards the draft. Wheel zoom and middle-drag (or Alt+left-drag) pan while cropping. Browsing/sorting waits for crop confirmation or cancellation.

Edits belong to each Photo ID, survive navigation/restarts and never re-encode previews. A separate worker reuses decoded images; large images get a bounded interactive preview followed by full detail after a short pause. Sorting captures edits and watermark settings, advances immediately, and applies them at full resolution in the background.

Editing an already sorted photo refreshes its output automatically after adjustments settle, using its saved destination and the current watermark. On Windows an unchanged generated file keeps the same filename; it remains in place until its replacement is rendered and journaled. Externally changed files are preserved and a free filename is used, with a queue notice. Rendering failures and early cancellation retain the previous completed output. Queued assignments keep their snapshots; if edits change while one runs, the latest edits are rendered afterward. Cancelled jobs stay stopped until reassigned. See [editing details](docs/EDITING.md).

## Watermark while sorting

1. Choose **Settings → Watermark… → Select PNG…** and select your transparent club logo. Selecting a valid PNG enables it.
2. Choose Top Left, Top Right, Bottom Left or Bottom Right. Adjust Size, Opacity and Padding while watching the photograph itself. The settings window stays open while you browse.
3. Press your destination shortcut, such as `1`. The next photo appears immediately; the background worker renders the watermark at original output resolution and saves the result.

Size is the logo's longest edge as a percentage of the photo's shorter side (1–100%). Padding uses the same photo dimension (0–25%). Aspect ratio is preserved and the logo is constrained to fit inside the photograph. Opacity is 0–100%, multiplied by the PNG's own alpha. Zoom, pan and resize transform the overlay with the photograph. Cached previews and originals remain unchanged.

Settings save automatically. The decoded logo is cached, and its PNG bytes are stored in the session so removing or replacing the imported file does not affect saved configuration or queued work. PNG imports are limited to 16 MiB and 16 megapixels. Each assignment captures its settings and logo version; later changes affect future assignments. Automatic retries and undo restore the original job's watermark. Reassigning a photo with changed settings creates a new job.

Enabled JPEG and RAW outputs use the selected JPEG quality; standalone PNG stays PNG, including transparency. Uncheck **Enable watermark** to remove the overlay and return future unedited JPEG/PNG assignments with default output settings to exact copying. Missing/damaged saved logos fail enabled jobs safely and can be replaced or disabled in settings. See [watermark details](docs/WATERMARK.md).

## Sessions and recovery

Each original folder stores its copy-only session in `.suresort-progress.sqlite`, including edits, destination configuration, watermark assets/settings, assignments, output ownership, Undo and browsing position. The app profile is `profile.sqlite` under Qt’s application-local data directory (normally `%LOCALAPPDATA%/SureSort/Sure Sort` on Windows). It holds app preferences, defaults, recent folders and local fallbacks. Locks prevent concurrent writers. Earlier session formats remain preserved and are not imported. To select a custom app-profile location:

```powershell
& '.\dist\Sure Sort\Sure Sort.exe' --session-db 'D:\SureSortProfile\profile.sqlite'
```

Closing pauses unfinished processing; reopening resumes journaled jobs and restores the input folder/photo. Prepared/published outputs are verified during recovery to avoid duplicates. Retry counts and pending retry operations survive restart; exhausted failed processing jobs stay stopped until reassigned. An interrupted partial temp whose ownership was never journaled is preserved as `.photocull-*.part`, and a fresh temp is used. Back up progress while the app is closed. A transient rollback journal and lock may exist while running.

## Windows prerequisites (MSVC 2022 x64)

1. Install Visual Studio 2022 Build Tools with MSVC v143 x64 tools and Windows SDK, CMake ≥3.24, Ninja and Git. In PowerShell:

   ```powershell
   winget install --exact --id Microsoft.VisualStudio.2022.BuildTools --override '--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
   winget install --exact --id Kitware.CMake
   winget install --exact --id Ninja-build.Ninja
   winget install --exact --id Git.Git
   ```

2. Download and run the [official Qt online installer](https://www.qt.io/download-qt-installer-oss). Select **Qt 6.8.3 → MSVC 2022 64-bit**, Qt base/Widgets/SQL, the SQLite driver and Qt Test development files. No WebEngine is required. Another available 6.8 LTS patch can be used; adjust `QT_ROOT` below. Later LTS patches may require a subscription; use open-source 6.8.3 if needed.

3. Start an **x64 Native Tools Command Prompt for VS 2022**, then run `powershell -NoExit` so `cl.exe` is configured. In that PowerShell:

   ```powershell
   $env:QT_ROOT = 'C:\Qt\6.8.3\msvc2022_64'
   $env:VCPKG_ROOT = 'C:\dev\vcpkg'
   git clone https://github.com/microsoft/vcpkg.git $env:VCPKG_ROOT
   & "$env:VCPKG_ROOT\bootstrap-vcpkg.bat" -disableMetrics
   Set-Location C:\Users\admin\Desktop\sure-sort
   cmake --preset windows-release
   cmake --build --preset windows-release --parallel 4
   ```

`vcpkg.json` pins a registry baseline and requires LibRaw ≥0.21.4. CMake installs it in manifest mode with `x64-windows` (dynamic linkage). Qt comes from the official kit. `libraw::raw_r` is selected from vcpkg; a system-library fallback is available on other platforms.

## Run, test, benchmark

```powershell
$env:PATH = "$env:QT_ROOT\bin;$PWD\build\windows-release\vcpkg_installed\x64-windows\bin;" + $env:PATH
& '.\build\windows-release\Sure Sort.exe' 'D:\Photos\Shoot' --cache-mb 256
ctest --preset windows-release
& .\build\windows-release\photocull_bench.exe
& .\build\windows-release\photocull_bench.exe 10000
```

Qt Test generates JPEG EXIF and DNG fixtures; no camera images are downloaded. The release benchmark creates a synthetic 24 MP JPEG and a 1,000-entry scan catalog, reporting scan/decode/cache timing. It does not measure full keyboard-to-display latency. See [performance](docs/PERFORMANCE.md) and the [manual checklist](docs/MANUAL_TESTS.md).

In restricted runners, point `TEMP` and `TMP` to a writable directory before testing. CTest uses the offscreen plugin. For native Windows UI/DPI checks:

```powershell
$env:QT_QPA_PLATFORM = 'windows'
& .\build\windows-release\photocull_tests.exe viewerAndWindowSmoke
Remove-Item Env:QT_QPA_PLATFORM
```

## Deploy with windeployqt

From the configured build shell, using a fresh destination:

```powershell
cmake --install build/windows-release --prefix 'dist/Sure Sort'
Get-ChildItem build/windows-release/vcpkg_installed/x64-windows/bin -Filter '*.dll' | Copy-Item -Destination 'dist/Sure Sort'
& "$env:QT_ROOT\bin\windeployqt.exe" --release --no-translations --no-opengl-sw --skip-plugin-types generic,networkinformation,tls --exclude-plugins qgif,qico,qsqlmimer,qsqlodbc,qsqlpsql --dir 'dist/Sure Sort' 'dist/Sure Sort/Sure Sort.exe'
```

Or use the helper, which also copies included license texts:

```powershell
.\tools\deploy-windows.ps1 -QtRoot $env:QT_ROOT
```

Keep the matching x64 MSVC runtime deployed by `windeployqt`, or install Microsoft's Visual C++ Redistributable 2015–2022 x64. Copy vcpkg `share/*/copyright` files for all shipped DLLs and the notices from the actual Qt kit; see [third-party notices](THIRD_PARTY_NOTICES.md). Test the complete folder on a clean Windows machine without Qt or vcpkg on PATH before shipping. The application itself makes no network requests.

## Linux build

Install Qt **6.8 or newer**, CMake ≥3.24, Ninja, a C++17 compiler, and dynamic LibRaw ≥0.21 development packages. Older distro Qt packages may not qualify. Set `CMAKE_PREFIX_PATH` for a separate Qt kit if needed:

```sh
cmake --preset linux-release
cmake --build --preset linux-release --parallel 4
ctest --preset linux-release
./build/linux-release/PhotoCull /path/to/photos
./build/linux-release/photocull_bench
```

Keep `Qt6Sql.dll` and `sqldrivers/qsqlite.dll` with the deployed app. Linux is untested here; its Qt kit also needs SQL/SQLite support. See [research](docs/RESEARCH.md) and [architecture](docs/DESIGN.md). Preview decoding remains separate from full-sensor processing.

## Development documentation

Start with the [documentation index](docs/README.md) for research, design, measured performance, development workflow, automated/manual testing and the roadmap. See [CONTRIBUTING.md](CONTRIBUTING.md) for change/review guidance and [CHANGELOG.md](CHANGELOG.md) for changes awaiting release.

The output path in the header opens a menu with Open in Explorer and Change output folder. When folder buttons exceed the footer width, scroll horizontally with the wheel, trackpad or scrollbar. Settings → Viewing includes an optional destination folder toast, off by default, displayed after a successful copy. Sorting Undo and Ctrl+Z are removed.
