# macOS installation and builds

Version 0.1.7 provides Windows installers and portable packages. macOS packaging remains experimental and can be built locally or through the manual GitHub Actions workflow below. Check [GitHub Releases](https://github.com/exposure-iiits/sure-sort/releases) for any separately published experimental Mac packages.

## Install

Download the Mac DMG, open it and drag **Sure Sort** to **Applications**. The package targets Apple Silicon Macs; an Intel installer is not included. It is ad-hoc signed, without an Apple Developer ID or notarization.

## Current limitations

- Sorting creates copies and leaves original photos unchanged.
- Automatic removal of destination copies currently requires Windows file locking. Clear, removal and reassignment preserve those files on macOS.
- Editing an already sorted photo may save a replacement under a new filename while preserving the previous output.
- The cloud build checks folder scanning and packaged startup/SQLite loading. Full photo editing, RAW, sorting and recovery behavior still needs native Mac testing.

Use disposable photo copies when evaluating this experimental build. See the [package notes](MACOS-PACKAGE.txt).

## Build on a Mac

Install Apple's Xcode Command Line Tools and Homebrew, then run from the project root:

```sh
brew install cmake ninja qtbase libraw
bash tools/package-macos.sh
```

The script builds for the host architecture, bundles Qt/LibRaw and their dependencies, includes license notices, creates the app icon, verifies signatures and startup, and creates a DMG plus SHA-256 checksum in `dist/`. Existing DMGs are preserved. Build staging and smoke-test data stay under `build/`.

Homebrew supplies Qt 6.8 or newer and LibRaw 0.21 or newer. The minimum macOS version depends on those dependencies. Intel builds require a separate build with Intel dependencies.

## Build with GitHub Actions

Run **Actions → Build experimental MacBook package → Run workflow**. Leave **release_tag** empty to download the result from the run's Artifacts section. Maintainers can provide an existing release tag, such as `v0.1.7`, to attach the DMG and checksum to that release.

Choose **macos-15** or **macos-26** for Apple Silicon, or **macos-15-intel** for Intel. The workflow builds the selected release tag, then attaches the matching architecture's DMG. Its build logs and bundled dependency metadata describe the actual toolchain used for that package.
