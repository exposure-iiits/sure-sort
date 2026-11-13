# Using Sure Sort

## Sort and save automatically

1. Open the input folder, then click the **settings gear → Sorting**, or press **Ctrl+,**.
2. Select an output directory. Use the right-hand **+** to add subfolders and edit names/shortcuts, then click **Save folders**. Each row has a **bin icon** to remove that mapping. Duplicate names/keys and conflicts with viewer shortcuts are rejected. Blank shortcuts leave additional folders accessible by clicking their buttons.
3. Save the configuration, then press a folder key or click its button below the photo. The next photo appears immediately while processing runs in the background.

New folders use the next available number shortcut. The footer's **+** adds a name field directly beside the folder buttons; type the name and press Enter or leave the field to save, or Esc to cancel. If no output directory is set, Plus opens Settings → Sorting so you can choose and save it. Rename also happens inline. Settings can customize each folder’s shortcuts.

With no edits, watermarking disabled and default output settings, JPEG/PNG are copied byte-for-byte, including metadata. Otherwise the worker decodes the original, applies tone → rotation/crop → watermark → configured resize → encoding. RAW uses full-sensor development, camera white balance (automatic fallback), AHD where appropriate, an sRGB matrix and transfer curve, and JPEG encoding. A RAW+JPEG pair produces one JPEG from the RAW; a bad RAW fails rather than exporting its companion/embedded preview. Orientation is baked into processed pixels. Camera previews can differ from developed RAW in color, tone and crop.

Choose **Settings → Saving** for JPEG quality **80–100** (default **95**), **Original / 75% / 50%** dimensions and **Preserve EXIF where possible** (default on). RAW produces JPEG; JPEG/PNG retain their format. Quality changes affect JPEG; PNG remains lossless with alpha. Non-default JPEG quality, reduced dimensions or disabling metadata causes processing instead of an exact copy. Saving preferences save automatically and apply to future assignments; queued jobs and automatic retries retain their captured output settings. Reassign to the same folder to regenerate with new settings.

Supported EXIF includes camera/lens, exposure, capture dates, author/copyright and GPS when available in TIFF-based RAW, JPEG EXIF or PNG eXIf. Other RAW containers retain basic camera/exposure fields exposed by LibRaw. Processed metadata gets orientation 1, final dimensions and sRGB; stale thumbnails, MakerNotes, IPTC/XMP and unknown/vendor fields are not transferred. Exact-copy jobs retain all original bytes. No metadata library or new runtime dependency is added.

The compact footer groups folder buttons with processing counts, then photo/assignment details with dimensions and zoom. Failed processing and removal cleanup retry automatically after **1 second**, then **3 seconds**, using the same captured settings and verified file ownership. After two unsuccessful retries, an inline error appears; click processing status for full details. Fix the cause and reassign a failed photo to try again. There are no Retry or Cancel queue buttons. One low-priority processing worker limits simultaneous RAW conversions; preview workers remain separate. Existing names get `_2`, `_3`, etc.; publication never overwrites an occupied path.

Removal/reassignment cancel stale work and remove only verified, unchanged application outputs. Modified, replaced or locked files are preserved with a notice. Windows verifies/deletes using the same handle while denying concurrent writes/deletion. Other platforms conservatively preserve outputs during removal; Linux is unverified. Configuration changes affect future assignments; prior assignments retain their saved destination snapshots. Subfolders are created when first used.

## Copying, progress and removal

Click a footer folder or press its shortcut to copy the current photo there. A green outline on that folder means the selected photo has a copy there. Click or press the same shortcut again to remove that copy. Copying advances immediately; removing keeps the current photo selected. Pressing again while a copy is still saving cancels that pending copy.

Sure Sort automatically saves each opened folder’s session in **.suresort-progress.sqlite** beside its original photos. Reopening restores destination names, shortcuts, output location, edits, output settings, copy progress and the last viewed photo. The latest watermark settings follow you across folders. Source paths are stored relative to the opened folder so its progress follows it. External output locations keep their absolute paths. Pending jobs remain attached to their folder while you browse another folder, with one shared processing worker.

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

**Editing shortcuts** in the panel opens the Editing category directly. Double-click a Primary or Alternate shortcut and press its replacement key or chord to save immediately. Esc cancels capture; Delete clears a binding. Conflicts show a toast and keep the previous key. Settings and keyboard shortcuts open as modal windows, and the active shortcut category is highlighted. Repeated arrow presses adjust while the hold key is active; normal navigation resumes on release. Existing destination keys take priority if an older session uses a new default hold key.

Choose **Crop** or press K, then drag a rectangle on the photograph. Drag inside to move it and drag a corner to resize. Select Free, Original ratio, 1:1, 4:3, 3:2 or 16:9. Original ratio follows the full oriented image, including rotation. The outside area dims and the watermark previews within the crop. Enter commits; Esc discards the draft. Wheel zoom and middle-drag (or Alt+left-drag) pan while cropping. Browsing/sorting waits for crop confirmation or cancellation.

Edits belong to each Photo ID, survive navigation/restarts and never re-encode previews. A separate worker reuses decoded images; large images get a bounded interactive preview followed by full detail after a short pause. Sorting captures edits and watermark settings, advances immediately, and applies them at full resolution in the background.

Editing an already sorted photo refreshes its output automatically after adjustments settle, using its saved destination and the current watermark. On Windows an unchanged generated file keeps the same filename; it remains in place until its replacement is rendered and journaled. Externally changed files are preserved and a free filename is used, with a queue notice. Rendering failures and early cancellation retain the previous completed output. Queued assignments keep their snapshots; if edits change while one runs, the latest edits are rendered afterward. Cancelled jobs stay stopped until reassigned. See [editing details](EDITING.md).

## Watermark while sorting

1. Choose **Settings → Watermark… → Select PNG…** and select your transparent club logo. Enable watermarking to apply the selected PNG.
2. Choose Top Left, Top Right, Bottom Left or Bottom Right. Adjust Size, Opacity and Padding while watching the photograph itself. The whole Settings window fades during adjustments and returns to normal afterward. Close Settings to resume browsing.
3. Press your destination shortcut, such as `1`. The next photo appears immediately; the background worker renders the watermark at original output resolution and saves the result.

Size is the logo's longest edge as a percentage of the photo's shorter side (1–100%). Padding uses the same photo dimension (0–25%). Aspect ratio is preserved and the logo is constrained to fit inside the photograph. Opacity is 0–100%, multiplied by the PNG's own alpha. Zoom, pan and resize transform the overlay with the photograph. Cached previews and originals remain unchanged.

Settings save automatically across folders and app restarts. The decoded logo is cached, and its PNG bytes are stored in the app profile and folder session so removing or replacing the imported file does not affect saved configuration or queued work. PNG logo imports have a 16 MiB file-size limit and no 16 MP restriction. Each assignment captures its settings and logo version; later changes affect future assignments. Automatic retries use the original job's watermark. Reassigning a photo with changed settings creates a new job.

Enabled JPEG and RAW outputs use the selected JPEG quality; standalone PNG stays PNG, including transparency. Uncheck **Enable watermark** to remove the overlay and return future unedited JPEG/PNG assignments with default output settings to exact copying. Missing/damaged saved logos fail enabled jobs safely and can be replaced or disabled in settings. See [watermark details](WATERMARK.md).

## Sessions and recovery

Each original folder stores its copy-only session in `.suresort-progress.sqlite`, including edits, destination configuration, watermark assets/settings, assignments, output ownership and browsing position. The app profile is `profile.sqlite` under Qt’s application-local data directory (normally `%LOCALAPPDATA%/SureSort/Sure Sort` on Windows). It holds app preferences, defaults, recent folders and local fallbacks. Locks prevent concurrent writers. Earlier session formats remain preserved and are not imported. To select a custom app-profile location:

```powershell
& '.\dist\Sure Sort\Sure Sort.exe' --session-db 'D:\SureSortProfile\profile.sqlite'
```

Closing pauses unfinished processing; reopening resumes journaled jobs and restores the input folder/photo. Prepared/published outputs are verified during recovery to avoid duplicates. Retry counts and pending retry operations survive restart; exhausted failed processing jobs stay stopped until reassigned. An interrupted partial temp whose ownership was never journaled is preserved as `.photocull-*.part`, and a fresh temp is used. Back up progress while the app is closed. A transient rollback journal and lock may exist while running.
