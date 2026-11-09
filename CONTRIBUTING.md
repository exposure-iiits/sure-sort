# Contributing to SureSort

Read the [documentation index](docs/README.md), [design](docs/DESIGN.md) and [development guide](docs/DEVELOPMENT.md) before changing the viewer. Current priorities are responsiveness, resource use, simple UX and reliability.

## Change workflow

1. Identify the observable issue or intended behavior and the component responsible. Keep unrelated changes separate.
2. Preserve originals, browsing responsiveness, bounded work, generation checks and verified output ownership. Processing runs automatically on assignment; editing uses shared tone/geometry logic and never changes cached originals. Output refreshes preserve external changes and remain recoverable. For architecture changes, update DESIGN with the decision and reason.
3. Build using a compatible Qt/compiler/LibRaw kit. Use modern C++ ownership, RAII and the project's existing formatting; avoid unnecessary dependencies and abstractions.
4. Run relevant automated tests and manual checks from [TESTING](docs/TESTING.md). Add meaningful regression coverage for a defect when practical. Record what actually ran and what remains untested.
5. Update affected user/development documentation. Put user-visible changes in [CHANGELOG](CHANGELOG.md); put measured performance evidence in PERFORMANCE, not estimates.

If creating a branch, use a descriptive `codex/` prefix unless the task specifies another naming convention. Keep personal SDK paths, generated builds, reference clones, binaries and private photographs out of commits. Do not overwrite unrelated working-tree changes.

## Review information

Describe the problem and resulting behavior first, then the relevant implementation tradeoff, verification and limitations. For performance changes, include Release measurements and conditions. For RAW changes, state the LibRaw version and distinguish generated fixtures from verified camera files.

Keep dependency notices current. Reference viewers are research material; their GPL application code must not be copied into this project. See [third-party notices](THIRD_PARTY_NOTICES.md) for the libraries used by the application.
