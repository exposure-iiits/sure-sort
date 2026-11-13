# Sure Sort

Browse, edit and sort a photo shoot without leaving the keyboard. Sure Sort runs offline and saves copies into your chosen folders while keeping your original photos unchanged.

## Download

**Version 0.1.6**

| Platform | Download |
| --- | --- |
| Windows x64 | [Windows installer](https://github.com/exposure-iiits/sure-sort/releases/download/v0.1.6/Sure-Sort-0.1.6-Setup-Windows-x64.exe) |
| Windows x64, no installation | [Portable ZIP](https://github.com/exposure-iiits/sure-sort/releases/download/v0.1.6/Sure-Sort-0.1.6-Windows-x64-Portable.zip) |
| macOS, Apple Silicon | [Mac installer — build pending](https://github.com/exposure-iiits/sure-sort/releases/tag/v0.1.6) |
| macOS, Intel | [Mac installer — build pending](https://github.com/exposure-iiits/sure-sort/releases/tag/v0.1.6) |

[All releases and checksums](https://github.com/exposure-iiits/sure-sort/releases)

The Mac build is experimental and is not notarized. Copy removal is currently supported only on Windows. See [Mac installation and limitations](docs/MACOS.md).

## Get started

1. Install Sure Sort and open a photo folder. Photos in nested folders are included.
2. In **Settings → Sorting**, choose an output location and add your destination folders.
3. Press a folder's number key or click its button to save a copy and advance to the next photo.

Green folder outlines show where the current photo has been copied. On Windows, press the same key again to remove that copy. Your progress saves automatically so you can pick up where you left off.

## What you can do

- Browse JPEG, PNG and common RAW formats, including paired RAW + JPEG photos.
- Copy a photo into several folders with keyboard shortcuts.
- Adjust exposure, color, rotation and crop before saving.
- Add a PNG watermark with a live preview.
- Choose JPEG quality, output size and metadata settings.
- Check photo information and histograms while you work.

Edits and watermarks apply to saved copies. Original photo files stay unchanged. RAW support depends on the camera format supported by LibRaw.

## Useful controls

| Key or gesture | Action |
| --- | --- |
| ← / → or A / D | Previous / next photo |
| Folder number key | Copy to that folder; press again to remove the copy on Windows |
| Z | Toggle 100% zoom and fit |
| Mouse wheel / trackpad | Zoom around the cursor |
| Double-click the photo | Toggle fullscreen |
| Ctrl+, | Open settings |

Open the keyboard button in the header to view and customize navigation, viewing and editing shortcuts. Folder keys are configured in **Settings → Sorting**.

## Learn more

[User guide](docs/USER_GUIDE.md) · [What's new](CHANGELOG.md) · [Build from source](docs/BUILDING.md) · [Development](docs/DEVELOPMENT.md) · [Report an issue](https://github.com/exposure-iiits/sure-sort/issues)

Built with C++17, Qt and LibRaw. See [third-party notices](THIRD_PARTY_NOTICES.md) for dependency licenses.
