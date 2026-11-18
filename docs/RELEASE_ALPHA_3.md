# Dolly 0.1.0 Alpha 3 — Restore preflight preview

This development release targets MC68000 and AmigaOS 2.04 or later.
The GUI reads devices and writes image, backup, log, preference and
checkpoint files. Raw Restore, Copy/Clone, partition-table restoration
and resizing remain development work.

- Surface Scan lets you edit inclusive LBA or CHS bounds and the retry
  budget. The default range covers the selected object.
- Start becomes Pause during an operation and Resume while paused.
  Sheep Counter has the same control. Stop offers Abort or Resume.
  Quit keeps the application open while an operation is active.
- Create Image checks for overlapping source and destination ranges.
  An unexpected failure can offer a DLRP checkpoint when the staging
  file is available. The release includes checkpoint creation.
- Restore runs read-only RAW/HDF preflight. It compares object type,
  size, filesystem signatures and supported RDB metadata. The
  optional settings override for compatible-sized metadata mismatches
  requires a ten-second destructive-operation confirmation.
- Preferences use schema v4. Incompatible preferences produce a
  requester and fall back to defaults.

The native adapter and batch restore engine are built and exercised
with host mocks. Device write authorization, media ownership and
end-to-end native write verification are pending. See the test matrix
for the distinction between current automated and historical emulator
coverage.

Follow-up work is tracked in [Restore and Copy/Clone #72](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/72)
and [DLRP resume #73](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/73).
