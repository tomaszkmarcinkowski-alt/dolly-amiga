# Dolly Alpha 3 — Restore RDB test build

This test build targets MC68000 and AmigaOS 2.04+. The application
still displays Dolly Alpha 3.

- Inventory enables Restore RDB for supported offline ROM scsi.device
  targets with matching layout. Dolly saves a recovery copy before
  writing. The shared requester requires a ten-second confirmation.
  Changed sectors are read back and verified.
- Copy compares the exact selected sizes and sector geometry. A
  mismatch in either direction blocks the operation and shows both
  sizes. Matching ranges proceed through confirmation to a read-only
  preview. Copy does not yet write sectors.
- The Restore and Create Image selectors accept typed file paths.
  Progress and completion identify the image source and output.
- The Inventory partition list selects a row. The header and detail
  lists support clipboard copying.
- Physical selection lists scsi.device Unit 0 if Exec identifies the
  driver even without a mounted volume. Query verifies the medium.

The MC68000 build and 36 host tests passed. The new Restore RDB write
path still requires runtime validation on a disposable emulator HDF.

See the [Inventory guide](SHEEP_INVENTORY.md), [I/O guide](AMIGA_IO.md)
and [Copy and Restore status](DEVICE_WRITE.md).
