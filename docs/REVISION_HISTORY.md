# Dolly public revision history

Each entry covers the changes since the preceding public release. Test builds
are included in the public revision that first ships their changes.

## Alpha 3.8 — 2026-11-22

Changes since [Alpha 3.7](#alpha-37--2026-11-21).

- Recognize Macintosh HFS, HFS Plus, wrapped HFS Plus, HFSX and Apple
  Partition Maps in raw media.
- Export an eligible Macintosh volume as a single RAW FileDisk, or
  export the whole virtual disk with its partition map and driver.
- Import a FileDisk into a dedicated RDB partition with a partition
  map and a compatible Macintosh SCSI driver from a user-supplied
  template. Warning Review shows the destination filesystem and
  write extent before timed approval.
- Check the 2 GiB FileDisk and inner Macintosh partition limits and
  the 4 GiB destination extent limit. ShapeShifter uses single RAW
  image files for this workflow.
- Add desktop `import-apple` and `export-apple` commands to
  `dolly-image`.

See the [ShapeShifter guide](SHAPESHIFTER_HFS_RESEARCH.md) and
[test matrix](TEST_MATRIX.md#native-shapeshifter-4-gib-boundary).
Mac OS runtime verification inside ShapeShifter remains pending.

[GitHub release](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/releases/tag/v0.1.0-alpha.3.8)

## Alpha 3.7 — 2026-11-21

Changes since [Alpha 3.6 Write Review and Mounting](#alpha-36-write-review-and-mounting--2026-11-21).

### Imaging, copy and recovery

- Added **Project → Resume operation...**. Resume a failed RAW, HDF or
  single-file IFF DOLY Create Image from its retained staging file. Dolly
  checks the checkpoint, source identity and staged prefix, including DOLY
  chunk checksums, and preserves the original compression mode.
- Added version 2 DLRP checkpoints for sector-to-sector Copy and Restore
  Image, including single-file and split IFF DOLY input. Resume repeats
  preflight, checks the original destination anchors, compares the committed
  prefix, and rewrites and verifies the failed batch from the saved cursor.
  A changed destination anchor blocks resumption. Smart Copy, resize and
  split Create Image keep their separate recovery paths or limitations.
- Tightened write isolation to the selected partition range. Copy can work
  between disjoint partitions of one unit. Restore checks the volumes and
  open DOS handlers of every image member, including relocated split parts.
  Verified non-overlapping partitions stay available. Confirmed overlap
  blocks the write, and a single optional warning covers same-unit performance.
- Improved handling of a registered unformatted destination whose DOS
  handler refuses shutdown. Dolly can hold the matching handler inhibited
  during the write and bring it back for mounting when eligible. A formatted
  active volume that cannot be taken offline is still protected.
- Extended the DOS/1 and DOS/3 Clone and resize prototype to relocate a
  conflicting bitmap page and extension in supported layouts. Corrected the
  last bitmap word and linked the relocated structures into the resized image.
  Dolly supplies a larger stack when the launcher stack is too small.
- Improved ROM IDE capacity rechecks after block transfers in UAE and
  retained separate approval for additional device-wide opens.

### Results and progress

- Added operation summaries for Surface Scan, Create Image, Copy and
  Restore Image with endpoints, outcomes, sectors and bytes. Additional
  fields cover scan errors and retries, skipped Smart Copy sectors, image
  validation and write verification when applicable.
- Added elapsed and active time, live rate, average rate and compressed output
  size where relevant. The main window and Sheep Counter draw the caption
  over the fill and shorten it to fit the current font and width. Sheep
  Counter can adjust its grid shape when a larger font reduces available space.
  The rate formatter emits one fractional digit and builds with strict
  truncation checks on the MC68000 compiler.
- Moved the destination mounting choice to the successful write summary and
  combined it with any applicable rename review.
- Added Inventory summaries for RDB and MBR backups. Restore RDB reports
  unchanged sectors, written and verified sectors, the final reread and
  the recovery copy. An identical backup requires zero writes and no new
  recovery file.

### Documentation and verification

- Updated the operation guide, device-write rules and DLRP format for the
  new workflows. Added ShapeShifter HFS/HFS Plus feasibility findings and
  the proposed FileDisk conversion and overwrite workflow to the backlog.
- Recorded native FS-UAE coverage for partition isolation, split Restore,
  checkpoint resumption, resize, progress and Inventory metadata operations.
  See the [test matrix](TEST_MATRIX.md) for the fixtures and their limits.

[GitHub release](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/releases/tag/v0.1.0-alpha.3.7)

## Alpha 3.6 Write Review and Mounting — 2026-11-21

First baseline for this revision history. Unified destination overwrite review,
timed warnings and automatic dismount for Copy and Restore Image. Added
destination mounting from the final review and enabled mapped Logical device
endpoints for Clone and resize. Restore RDB used timed confirmation, a
recovery copy and verified writes. Fixed the Logical device picker and
Start button behavior.

[GitHub release](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/releases/tag/v0.1.0-alpha.3.6-write-review)
