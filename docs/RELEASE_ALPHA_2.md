# Dolly 0.1.0 Alpha 2 — final read-only build

Alpha 2 closes the read-only device phase. Surface Scan, Create Image and
Sheep Inventory can read physical media. Image creation, metadata backup
and preferences write destination files. Raw device writes and restore
remain disabled in this release.

- Auto uses validated RDB I/O parameters. Sources without complete RDB
  settings get an Intuition requester offering Continue, Settings and Cancel.
- Settings supports per-unit overrides, versioned IFF preferences and an
  opt-in legacy scsi.device limit override. Large-offset access still needs a
  supported command.
- The Amiga executable targets MC68000 and AmigaOS 2.04 or later. Actual
  controller compatibility remains hardware-dependent.

See [Amiga I/O](AMIGA_IO.md), [Surface Scan](SURFACE_SCAN.md) and
[Create Image](CREATE_IMAGE.md).
