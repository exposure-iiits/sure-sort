# Experimental MacBook packaging

The current source version is **0.1.2**. macOS packaging support has been
prepared, but no Mac installer was built or tested in the Windows workspace.
The package is experimental: Move and verified output removal still require
Windows file locking. See [package limitations](MACOS-PACKAGE.txt). Packaging
does not implement these missing platform features.

## Build in the cloud without owning a Mac

1. Put this source project in a GitHub repository, including the hidden
   `.github/workflows/macos-package.yml` file. Upload the project contents,
   with `CMakeLists.txt` at the repository root, as described in
   [START_HERE](../START_HERE.md).
2. On GitHub, open **Actions > Build experimental MacBook package > Run workflow**.
3. After a successful run, open its **Artifacts** section and download
   **Sure-Sort-macOS-Apple-Silicon-experimental**.
4. Extract the artifact ZIP to get the `.dmg` and SHA-256 checksum. Open the DMG
   on an Apple Silicon Mac and drag **Sure Sort** to **Applications**.

This is a manually started workflow using a macOS 15 Apple Silicon runner.
It does not publish a release. No Apple signing credentials are required for
the ad-hoc signed experimental build. GitHub Actions availability and account
billing depend on the repository and account.

There is no configured Git repository or authenticated GitHub connection in
this workspace, so the cloud workflow has not been uploaded or run here.

## Build on a Mac

Install Apple's Xcode Command Line Tools and Homebrew first, then run from the
project directory:

```sh
brew install cmake ninja qtbase libraw
bash tools/package-macos.sh
```

The script builds for the host architecture (`arm64` or `x86_64`), creates an
application bundle, copies Qt plugins and runtime libraries, adds dependency
notices, verifies ad-hoc signatures and bundled startup/SQLite loading, and
creates a drag-to-Applications disk image and checksum under `dist/`:

```text
Sure-Sort-0.1.2-macOS-arm64-experimental.dmg
Sure-Sort-0.1.2-macOS-arm64-experimental.dmg.sha256
```

Homebrew supplies the actual Qt (at least 6.8) and LibRaw (at least 0.21)
versions; their metadata is saved in the bundle. Builds use those versions,
not the Windows SDK files. The minimum supported macOS version also depends
on the selected Qt and dependency builds. Intel builds require compatible
Intel dependencies; the included cloud workflow builds Apple Silicon only.

Existing DMGs are preserved. Move an earlier DMG aside before building another
with the same version/architecture. Temporary staging files and smoke-test
session data stay under `build/`, outside the shipped app and disk image.

## Validation status

Local checks passed on Windows on 9 November 2026: Bash syntax, the macOS-only
host guard, embedded Python syntax, CMake configuration, a Windows application
build, the six scanner checks and CLI version `Sure Sort 0.1.2`. They do not validate
Mac compilation or runtime behavior. The workflow runs the scanner test suite
and packaged native startup/SQLite checks; the full application test suite
contains Windows-specific Move/Undo expectations and has not been ported.

A distribution release still needs macOS file-operation support, native
functional testing, an Apple Developer ID signature and notarization.

References: [Qt macOS deployment](https://doc.qt.io/qt-6/macos-deployment.html),
[CMake BundleUtilities](https://cmake.org/cmake/help/latest/module/BundleUtilities.html),
[GitHub hosted runners](https://docs.github.com/en/actions/reference/runners/github-hosted-runners),
[Homebrew Qt Base](https://formulae.brew.sh/formula/qtbase) and
[Homebrew LibRaw](https://formulae.brew.sh/formula/libraw).
