# Versioning

Current version: **0.1.2**.

Versions use **MAJOR.MINOR.PATCH**:

- Major: incompatible changes.
- Minor: new features.
- Patch: fixes and refinements.

During 0.x development, minor releases may include breaking changes.

The single source is `project(PhotoCull VERSION x.y.z)` in `CMakeLists.txt`. CMake generates `app_version.h`, which supplies Settings → About and the application's `--version` output. Update the matching FILEVERSION, PRODUCTVERSION and version strings in `assets/suresort.rc` for Windows executable properties when making a release. SQLite's schema version is independent of the application release number.
