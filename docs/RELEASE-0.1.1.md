# Sure Sort 0.1.1

- Faster browsing: retain useful in-flight previews, prepare two neighbors with a reserved foreground slot, follow navigation direction, and bound the plan to the cache budget.
- Faster catalog sorting: compute names and paths once. Per-photo assignment lookup is indexed instead of scanning every assignment during navigation.
- New minimal folder/checkmark logo in the header and Windows app icon.
- Footer + creates a sorting folder immediately, with Copy/Move and an optional shortcut. The first folder can set the output directory here.
- Right-click a footer folder to choose Copy/Move or Rename. There is no Delete action. Rename waits for background saving to finish and preserves existing photos, moved originals, Undo paths and restart recovery. Occupied names are rejected. A destination containing photos imported directly from itself cannot be renamed in this session.
- Shortcut categories across the top: Navigation, Viewing, Sorting, Editing and Application. One row per action, with Primary and Alternate bindings, including folders and editing. Existing shortcuts migrate automatically.
- Double-click a photo to toggle fullscreen. Move to the top edge to reveal the header and exit button. F11/Esc continue working; crop double-clicks do not toggle fullscreen.
- Reset edits appears beside Undo only when the current photo has adjustments, rotation or crop. Undo continues to undo sorting operations.
- Same-named photos in different directories remain separate. Both export into the chosen folder, using IMG_1.jpg and IMG_1_2.jpg. Verified for Copy/Move, with and without edits, through restart and Undo. RAW/JPEG companions within one directory remain one logical photo.
- Version 0.1.1 in About, CLI and Windows executable properties. Session schema 7 adds alternate folder shortcuts and rejects older app builds after upgrade.

Output hashing, no-overwrite publication, durability and verified Undo protections remain enabled. Their cost has not been removed to improve benchmark numbers.