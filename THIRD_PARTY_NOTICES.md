# Third-party notices

The dependency versions below describe the verified Windows build. Experimental
macOS packages use Homebrew Qt Base/LibRaw and their dependencies; the actual
formula versions, source metadata and supplied notices are recorded under
`Contents/Resources/licenses` in the app bundle. Windows MinGW runtime DLLs
are not part of the macOS package.

PhotoCull links dynamically to Qt and LibRaw. No source code from qimgv, qView, or nomacs is incorporated. Those GPL applications were architectural research references only.

- **Qt 6.8.3**, Copyright The Qt Company Ltd. and contributors. Qt Core, Gui, Widgets, SQL and bundled image/platform/style/SQLite plugins are used under LGPLv3 with applicable Qt exceptions and third-party notices. [Qt licensing](https://www.qt.io/licensing/open-source-lgpl-obligations), [corresponding Qt 6.8.3 source](https://download.qt.io/archive/qt/6.8/6.8.3/single/), [Qt third-party notices](https://doc.qt.io/qt-6.8/licenses-used-in-qt.html). The local verification SDK is an unmodified official dynamic MinGW build. MSVC builds use the user's official Qt installation. Preserve the notices for the actual Qt kit when distributing it.
- **LibRaw 0.21.4**, Copyright LibRaw LLC and contributors. Used under LGPL 2.1; alternatively available under CDDL 1.0. [Corresponding source](https://github.com/LibRaw/LibRaw/tree/0.21.4). Local verification compiles this release as a DLL without OpenMP, JPEG-lossy-DNG or optional external decoder libraries. vcpkg packages may enable additional capabilities and include their own notices.
- The local MinGW package also includes GCC runtime DLLs, licensed under GPLv3 with the GCC Runtime Library Exception, and MinGW-w64 runtime components with their respective permissive licenses. [GCC exception](https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html), [MinGW-w64 licensing](https://www.mingw-w64.org/disclaimer/).

The application permits replacing dynamic libraries with compatible modified versions. Qt and LibRaw can be rebuilt from their linked corresponding sources. Preserve licenses and third-party notices with redistributed binaries. vcpkg dependency notices are in `vcpkg_installed/x64-windows/share/<port>/copyright` and should accompany an MSVC distribution. This file does not set a license for the user's original application code.
