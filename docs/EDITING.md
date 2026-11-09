# Editing workflow

Open a photograph and toggle **Edit**. Adjust sliders, use the default hold-key chords or customize them by double-clicking a shortcut cell in the header's keyboard menu. Shortcut editing stays inside that panel. The panel's **Editing shortcuts** button opens its **Editing** category directly. Settings and Shortcuts share the main application window. Double-click any slider to reset it; **Reset All** clears tone, rotation and crop together. Settings apply to the current photo and remain when you browse away, return or restart. Originals and cached photographs remain unchanged.

**Settings → Editing → Keyboard editing step** sets the increment from 1 to 20 units (default 5); each exposure unit is 0.1 EV. The preference saves automatically and survives restarts. Keyboard rotation uses 90° steps; the rotation slider supports every degree from −180° to +180°. Application, folder and editing bindings are grouped in the keyboard menu. Double-click a Primary or Alternate cell, then press a key or hold-key chord to save immediately. Delete unassigns it; Esc cancels capture. Conflicts show a toast and leave the previous binding active. Editing alternates share the hold key.

| Adjustment | Range | Default keyboard increment |
| --- | --- | --- |
| Exposure | −5 to +5 EV | E + arrows, 0.5 EV |
| Brightness | −100 to +100 | B + arrows, 5 |
| Contrast | −100 to +100 | C + arrows, 5 |
| Highlights | −100 to +100 | H + arrows, 5 |
| Shadows | −100 to +100 | S + arrows, 5 |
| Saturation | −100 to +100 | T + arrows, 5 |
| Rotation | −180° to +180°, 1° slider steps | R + arrows, 90° |

Ctrl+Shift+R resets everything. K starts/cancels crop. All sixteen editing action bindings can be changed or disabled; a blank hold key makes a plain shortcut, and a blank trigger disables the action. Hold keys are single unmodified keys. Navigation/destination keys, crop confirmation keys and duplicate edit combinations are protected. Editing chords take precedence over arrow navigation; folder assignments still ignore auto-repeat. Plain arrows on a focused slider/combo adjust that control. An older destination mapping conflicting with a new default edit hold takes priority; customize that editing binding to restore it.

For cropping, drag a rectangle on the photograph, drag its interior to move, or drag a corner to resize. Free, Original ratio, 1:1, 4:3, 3:2 and 16:9 are available. The outside is dimmed and the watermark follows the selected crop. Enter confirms, Esc cancels. Wheel zoom and middle-drag or Alt+left-drag pan without changing the selection. Navigation/sorting keys are suppressed during the draft. Rotation or Reset All cancels an active crop draft; a committed crop rotates with its original image region.

Legacy crops are normalized to the full, unrotated original **after EXIF orientation**. Crops made at a free angle are normalized to the rotated canvas. Both remain independent of the viewport and preview resolution. Pixel boundaries are rounded only when rendering at a particular resolution. Resetting crop through Reset All restores the full original image. Saved quarter-turn edits remain compatible with the degree slider.

The shared renderer converts known profiles to sRGB, applies EV gain in linear light through an 8-bit lookup curve, then brightness, contrast and weighted highlights/shadows. Saturation mixes around rounded Rec.709 luminance while preserving alpha. These are simple display-referred adjustments; they do not recover sensor data already clipped during RAW development or perform local masking. Quarter turns remain exact; free-angle rotation uses smooth resampling before rectangular cropping.

Live tone processing uses a separate single-worker queue with cooperative cancellation, one latest pending request and generation checks. It reads the existing preview/detail image. Large inputs first render at at most two megapixels; after 220 ms without another request, a full-detail refinement uses the same decoded source. Rotation/crop display immediately through painter transforms using the shared geometry helpers. No full sensor RAW development or JPEG encoding happens for a slider change. The original preview stays in the LRU; edited buffers belong only to the current viewer.

Per-photo edits save after 350 ms of inactivity and flush before assignment or normal shutdown. Assigning captures immutable edit, watermark and output parameters with PNG bytes/version. Output is original decode/full RAW development → tone → rotation → crop → watermark → configured resize → selected-quality JPEG (or PNG for standalone PNG input). JPEG/PNG without edits or watermark retain exact byte copying with default output settings.

Editing a sorted photo automatically refreshes its saved destination after the debounce, with current watermark/output settings captured for that new job. A running older job completes its captured snapshot first, then schedules the latest per-photo edits without advancing the viewer again. Explicitly cancelled assignments stay stopped until reassigned. Automatic retry uses its saved job snapshot; persistent later photo edits can then request a subsequent refresh. Undo restores the prior assignment's edit parameters and watermark snapshot.

On Windows a refresh renders and fingerprints the new temporary image while retaining the preceding output. If the old path still has its captured identity/content, the prepared journal records that same final path. Publication rechecks/deletes only the unchanged owned predecessor, then uses the existing no-overwrite rename. Changes between verification and publication fail safely; externally modified/locked files are preserved, with a new filename or an actionable failure. Decode/encode failure and early cancellation preserve the preceding completed image. Recovery handles the prepared, predecessor-deleted and renamed boundaries. Other platforms conservatively preserve old outputs and use free names; they remain unverified.

Schema 3 adds per-photo edits, editing shortcuts, immutable per-job edits and predecessor ownership. Older sessions migrate additively without changing their output snapshots. Back up with the app closed; Phase 5 upgrades to schema 4 for output preferences; older app versions cannot reopen it. Re-encoded output transfers a supported EXIF subset and omits vendor/IPTC/XMP data. RAW embedded previews can differ in crop/color from full development, so real-camera validation remains necessary.

The current Sure Sort workflow uses schema 6, with multiple folder placements and reversible original-file relocation; see the root README. Automatic edits refresh all current placements using the shared tone/geometry pipeline.
