# Development documentation

Sure Sort is the desktop viewer and automatic sorting application; PhotoCull remains the internal C++/CMake target name. Start with the root [README](../README.md) for installation, build, run and deployment commands.

| Document | Purpose |
| --- | --- |
| [RELEASE-0.1.2.md](RELEASE-0.1.2.md) | Current release controls and changes |
| [VALIDATION-0.1.2.md](VALIDATION-0.1.2.md) | Release build and verification results |
| [RESEARCH.md](RESEARCH.md) | Reference repositories, inspected source revisions and adopted/rejected techniques |
| [DESIGN.md](DESIGN.md) | Preview/processing architecture, threading, ownership, caching, SQLite and recovery |
| [DEVELOPMENT.md](DEVELOPMENT.md) | Source map, local development workflow and implementation constraints |
| [TESTING.md](TESTING.md) | Automated checks, focused test commands, fixtures and result recording |
| [MANUAL_TESTS.md](MANUAL_TESTS.md) | Real-camera, responsiveness, DPI, failure and deployment checklist |
| [WATERMARK.md](WATERMARK.md) | Watermark controls, relative geometry, automatic processing and snapshot behavior |
| [EDITING.md](EDITING.md) | Editing controls/shortcuts, crop coordinates, preview performance, persistence and output refresh |
| [PERFORMANCE.md](PERFORMANCE.md) | Actual measurements, environment, verification evidence and limitations |
| [ROADMAP.md](ROADMAP.md) | Phase boundaries, remaining validation and future extension points |

Additional project documents:

- [CONTRIBUTING.md](../CONTRIBUTING.md): change and review workflow.
- [CHANGELOG.md](../CHANGELOG.md): user-visible changes awaiting release.
- [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md): dependency provenance and notices.

Record architectural changes in DESIGN, measured results in PERFORMANCE, and outstanding validation in the manual checklist. Keep proposed work in ROADMAP distinct from implemented behavior.
