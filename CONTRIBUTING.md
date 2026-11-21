# Contributing to Dolly

Dolly is an alpha Amiga application with a portable inspection core and native disk I/O.

- Propose a change or bug report in GitHub Issues.
- Prefer small, reviewable PRs linked to a user story.
- Keep inspection paths read-only. Changes to write paths must preserve target bounds, warning review and optional verification.
- Include regression tests for parsers, sector boundaries and malformed input.
- Preserve support for a baseline Motorola 68000 / low-memory Amiga.
- Separate professional diagnostics from optional sheep-related humor.
- Do not add third-party artwork, samples or code without verifying distribution rights.
- When fixing a surprising bug, hardware incompatibility or architectural assumption, add a dated, traceable entry to [Engineering Field Notes](docs/ENGINEERING_JOURNAL.md). Preserve corrected hypotheses rather than silently erasing history.

For each public release, increment the shared version in `include/dolly_version.h`
and create a new release tag. Add a dated section to the [revision history](docs/REVISION_HISTORY.md)
covering features, fixes and other changes since the last public release.
Keep test builds within the interval they were developed in.

**License:** Dolly is MIT-licensed (see [LICENSE](LICENSE)). By contributing you agree that your contributions are submitted under the same MIT terms; only submit work you have the rights to contribute.
