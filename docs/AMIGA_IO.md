# Dolly — experimental AmigaOS native I/O

**Status:** native MC68000 build and FS-UAE checks completed for the qualified UAE-IDE read path. The device backend remains read-only. Create Image writes a staged file on a separate destination.

## Architecture

The portable `DollyBlockSource` owns a caller-provided backend and enforces range checks. The POSIX file adapter resides in `src/dolly_file_source.c`; the separate device adapter is `amiga/dolly_amiga_source.c`, with `DollyAmigaDeviceOptions` in `include/dolly_amiga_source.h`.

`DollyAmigaDeviceOptions` deliberately requires **explicit, trusted** device size, physical block size and maximum transfer size. These are **not guessed** from an unverified driver. A bad capacity value or controller DMA constraints can invalidate the safety guarantee; the native picker uses explicit read-only queries and the backend requires explicit DMA constraints.

## Initial read path

1. Create a message port and I/O request, with additional request room for driver compatibility.
2. `OpenDevice(name, unit, ...)` for the explicitly selected unit.
3. Query NSD capabilities with `NSCMD_DEVICEQUERY`; validate response length, format, device type, command list.
4. Use `CMD_READ` only for requests completely inside the first 4 GiB; use `NSCMD_TD_READ64` beyond or across the boundary **only if the driver advertised it**.
5. For the 64-bit command, the high 32 bits of the byte offset go in `io_Actual` **before** `DoIO()`; `io_Offset` holds the low 32 bits. The device normally replaces `io_Actual` with the transferred byte count.
6. Require block-aligned offsets and transfer lengths; cap each transfer by the resolved `max_transfer_bytes`. Use a bounce buffer allocated with the resolved memory attributes and `MEMF_PUBLIC`. The adapter requires explicit DMA constraints and checks start address, alignment and buffer extent; controller-specific bus reachability and cache requirements still require hardware validation.
7. Close the device, free its request/message port and memory on all paths.

The source adapter is **read-only**. The separate [device write adapter](DEVICE_WRITE.md) is a development target.

## Automatic settings and manual override

[Surface Scan](SURFACE_SCAN.md) and [Create Image](CREATE_IMAGE.md) use
the same raw read policy:

1. Auto uses the checksum-validated RDB partition's `Mask`,
   `BufMemType` and `MaxTransfer`. A logical source is matched to
   an on-disk RDB extent. Whole-unit reads intersect the masks,
   combine memory requirements and use the smallest transfer limit
   declared by the validated partitions.
2. With missing or incomplete RDB parameters, including MBR sources,
   an Intuition requester offers **Continue**, **Settings** and
   **Cancel**. Continue fills missing fields with the stock
   `scsi.device` defaults: `Mask=0x00fffffe`, Chip RAM with
   `MEMF_24BITDMA` and 16 KiB per transfer (at least one sector).
   Valid RDB fields and manual overrides still apply. Settings ends
   the current attempt and opens preferences. Start again to retry.
3. The middle **Settings** button opens preferences. With a selected
   physical, partition or logical source, the three per-unit fields
   accept decimal or `0x` hexadecimal values. Blank means Auto.
   **Reset Auto** clears the editable per-unit fields. **Save** applies and
   persists every accepted change. **Cancel** discards the edits. Initial
   geometry and table discovery use small read-only probes before RDB is
   available.

`Mask` constrains the complete DMA allocation and its start alignment.
`BufMemType` supplies Exec allocation attributes, and `MaxTransfer`
caps the sector-aligned request size. Dolly normally caps batches at
16 KiB, or one physical sector when the sector is larger. Invalid
combinations and incompatible allocations stop the operation.

Controller-specific handling is reserved for known configuration
incompatibilities. The active `scsi.device` below version 43 is
limited to 4 GiB by default. The explicit **Bypass legacy scsi.device
4 GiB limit** setting permits NSD capability discovery on those older
drivers. Reads above the boundary still require verified NSD READ64
or the existing INQUIRY-qualified UAE-IDE READ(10) path.

**Verify writes by reading sectors back** defaults to off. When enabled,
the block writer reads each written batch back after flushing and compares
it byte for byte against the input. Connecting the native adapter and
this setting to Restore is pending. See [Device writes](DEVICE_WRITE.md).

## Preferences file

The default is `ENVARC:dolly.prefs`. From Workbench, set the icon
tooltype `PREFS="path"` (relative paths resolve from the icon drawer).
From Shell, use `Dolly PREFS "path"` or `Dolly PREFS="path"`.
A missing file starts Dolly with built-in defaults. A malformed file
or an unsupported schema version displays a system requester and
ignores the entire file.

The file is an Amiga IFF `FORM PREF` with a six-byte `PRHD` chunk
and Dolly's `DOIO` chunk. The first byte of PRHD is the schema version,
currently 4. DOIO contains a 32-bit global flags field (bit 0 enables
the legacy-limit override, bit 1 enables write verification, bit 2 hides
the same-device speed warning, bit 3 allows the Restore compatibility
override), a 32-bit
record count, and up to 64 fixed records. Each record contains a
60-byte NUL-terminated driver name followed by five 32-bit fields:
unit, presence flags, Mask, BufMemType and MaxTransfer. IFF integers
are big-endian. Increment the PRHD version on every structural change.

## Test coverage

`tests/test_io_plan.c` checks the command choice around 4 GiB, alignment, transfer size and overflow. `tests/test_amiga_device_mock.c` exercises open/read/close and 64-bit capability selection with a **mocked Exec**, not a real OS. The [current test matrix](TEST_MATRIX.md) records native and host coverage.

A probe on FS-UAE A600 / Kickstart 2.05 selected the ROM path instead
of the UAE-IDE shortcut. The active resident passed qualification
and 12,096 sectors were read with no errors or retries. The host mock
also checks version 40 at 4 GiB and version 43 with and without
advertised NSD READ64.

## Important limitations

- Qualified reads across 4 GiB use NSD READ64 or the UAE-IDE SCSI READ(10) fallback. Legacy TD64 is pending.
- Real-device capacity and geometry, driver-specific MaxTransfer, DMA reachability, media changes and mounted-volume coordination require physical-controller testing.
- The read callback currently assumes the hardware can satisfy a complete aligned transfer to its bounce buffer. A device-specific failed or short transfer returns an error.
- A large `DollyPartitionMap` still needs heap/scratch-based lifetime management for tiny Amiga task stacks.
- A driver claiming NSD support may still be buggy. No destructive trial-and-error probes are allowed.

## References

- [Exec Device I/O](https://wiki.amigaos.net/wiki/Exec_Device_I/O)
- [NSD Command Specifications](https://wiki.amigaos.net/wiki/NSD_Command_Specifications)
- [TrackDisk64 Standard](https://wiki.amigaos.net/wiki/TrackDisk64_Standard)
- [Trackdisk Device](https://wiki.amigaos.net/wiki/Trackdisk_Device)

The above documents inform design only; they do not constitute hardware validation.

## DMA safety update (2026-11-17)

`MEMF_PUBLIC` alone is insufficient evidence of hardware DMA reachability. The adapter now fails closed unless the caller provides resolved DMA mask/alignment constraints, can optionally request `MEMF_24BITDMA` and rejects a bounce buffer outside the addressable aperture. This remains a **partial** safety check; it does not validate cache coherency, bus routing, controller quirks or active configuration. See [DMA safety](DMA_SAFETY.md) and [Issue #8](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/8).

A local reconstructed adapter/mock suite passed GCC, Clang and GCC ASan/UBSan, with short read and I/O-error injection. Native AmigaOS ABI and real device tests remain open.

### Finding scsi.device without a mounted volume

The physical and partition pickers also list Unit 0 as a candidate if
Exec knows `scsi.device` as a resident or loaded device and the unit
was absent from the AmigaDOS startup list. This covers the case where
no volume on the disk has been mounted. Select the driver and unit,
then use **Query** to confirm that the driver can open the unit and
report media geometry. The picker performs no automatic media probe.
