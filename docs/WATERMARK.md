# Watermark workflow

Open **Settings → Watermark** and check **Enable watermark** to reveal PNG selection, position and sliders. Select a transparent PNG, then adjust the controls while watching the existing photo viewer. Disabling watermark hides all its subordinate controls and retains their values for re-enabling. Changes preview instantly and save automatically. Close only dismisses the controls. Press the destination shortcut to save in the background and immediately advance.

| Control | Meaning |
| --- | --- |
| Select PNG | Import a logo, preserving alpha; successful import enables it |
| Enable watermark | Show overlay and apply it to future assignments |
| Position | Top Left, Top Right, Bottom Left, Bottom Right |
| Size, 1–100% | Longest logo edge as a percentage of the photograph's shorter side |
| Opacity, 0–100% | Multiplies each PNG pixel's alpha |
| Padding, 0–25% | Inset from both corner edges as a percentage of the shorter photo side |

For a 6000×4000 photo, Size 20% sets the longest logo edge to 800 pixels; Padding 3% gives a 120-pixel inset. A 1200×600 logo becomes 800×400. At Bottom Right its top-left is (5080, 3480). Doubling the photograph dimensions doubles every distance. The logo scales down further if needed to fit both padded dimensions. The same calculation is used for landscape, portrait and square images, in the viewer and at output resolution.

The viewer paints the cached PNG over the unchanged photograph using its actual image rectangle. Zoom and pan move both together; window margins are never used to anchor the watermark. Cached photographs and source files are not changed.

Enabled JPEG: decode original with orientation → tone → rotation/crop → composite → configured resize → selected-quality JPEG. RAW/RAW+JPEG: full-sensor development and orientation → edits → composite → configured resize → selected-quality JPEG. Standalone PNG retains PNG/alpha. Unedited JPEG/PNG with watermark disabled and default output settings use exact copies. Opacity zero still means enabled processing/re-encoding. Re-encoded images carry sRGB and baked orientation; supported EXIF fields are transferred; vendor/IPTC/XMP fields are omitted. RAW embedded previews may have different crop/color from final development. Watermark geometry always follows the final edited/cropped image.

Every assignment captures its settings and logo version before auto-advance. The session stores PNG bytes once per SHA-256 version, and queued jobs share the decoded asset. Changing settings or replacing/deleting the external logo does not change pending jobs. Retry and Undo use the captured watermark; a new assignment uses the current configuration. Reassigning the same photo/folder with changed settings creates a fresh job under existing safe output ownership rules.

PNG imports are bounded to 16 MiB encoded and 16 megapixels decoded. Invalid import leaves the prior logo intact. Missing/damaged saved PNGs cannot silently create unwatermarked outputs: enabled jobs fail safely, and the settings window lets you replace the logo or disable watermarking. Settings persist after a 150 ms debounce and flush on normal shutdown. Task snapshots are committed immediately with assignment.

Schema 2 originally added watermark assets/settings/version. Schema 3 added photo edits and predecessor output ownership; current schema 4 also captures output preferences. Existing Phase 2 jobs remain unwatermarked; Phase 3 jobs keep their watermark snapshots. Back up sessions with the app closed before release upgrades; older app versions cannot read schema 4. Editing an already sorted photo creates an automatic refresh with its saved destination and current watermark/output settings.
