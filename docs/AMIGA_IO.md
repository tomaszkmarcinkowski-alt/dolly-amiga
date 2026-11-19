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

### Enumerating disk drivers and units

The picker combines disk startup tuples from the locked AmigaDOS
device list with storage driver candidates from Exec. The Exec list
contains driver names, and AmigaOS has no generic list of their unit
numbers. For drivers with no DOS startup record, Dolly recognizes the
standard resident names and looks for storage-related names in the
loaded Exec list. A driver missing from the list can be entered by name in the **Device**
field.

When a driver is selected, Dolly attempts OpenDevice for units 0–9
and immediately closes successful opens. It merges those units with
AmigaDOS registrations and sorts the list by numeric ID. The same
rule applies to each driver and keeps disks without RDB discoverable.
Dolly does this outside the DOS list lock. No disk I/O commands are
issued by unit discovery. Query separately checks the selected unit
and reads capacity and partition metadata as appropriate. Query
prefers the last addressable LBA from SCSI READ CAPACITY(10) when it
returns a valid count and agrees with the sector size from
TD_GETGEOMETRY. The latter remains the fallback and supplies CHS when
available. RDB partition bounds are checked against the resulting
sector count. An out-of-bounds diagnostic reports the partition's
last LBA and the disk sector count. Partition selection and Sheep
Inventory show structurally valid entries even if some extend beyond
the driver's reported capacity. Affected rows are marked with ! and display the sector and byte
difference. Other in-range entries remain usable.
RDB backup and restore require a map within the reported capacity.

In Physical device and Partition mode, **Device** and **Unit** are
editable. **Device**, **SCSI name** and **Geometry** or **Partition**
share a left edge based on the current font metrics. In Physical
device mode, **Size** shares the **Geometry** row, leaving the full
value column available to **SCSI name**. The **Unit** field defaults
to 0 and accepts any unsigned 32-bit unit number.
Press Return to select it, or select **Query** directly. Partition
mode has an integer **Partition** field. The first entry in the RDB
PART chain is 1. After Query, selecting an item or entering its
number updates the partition choice. A zero or a number outside the
scanned list cannot be committed.

References: [AmigaOS Exec Device I/O](https://wiki.amigaos.net/wiki/Exec_Device_I/O),
[AmiPart device discovery](https://github.com/ChuckyGang/AmiPart/blob/main/src/devices.c),
and [WinUAE hardfile driver](https://github.com/tonioni/WinUAE/blob/master/hardfile.cpp).

Settings places the entry legend below the three I/O fields and above
the checkboxes. The recessed help panel describes the control under
the pointer. Save errors are also shown there.
The Settings window measures labels, help text and gadget rows using
the screen font to size its client area.

### ATA IDENTIFY

The physical and partition pickers, Sheep Inventory, and exported
Surface Scan logs report the ATA geometry and LBA sector count when
the driver returns a complete IDENTIFY DEVICE response through
SCSI ATA PASS-THROUGH(16). Query does not access IDE registers
directly. A driver without passthrough reports Unavailable. ATA
values are advisory and do not change the reported I/O bounds.


### Source partitions outside the reported range

Allow partitions outside range defaults to off and is stored in
dolly.prefs schema v5. When enabled, selecting an out-of-range source
requires a ten-second confirmation with both last LBAs and the size
of the gap. Destination writes remain bounded by reported capacity.

Fill gap with 0s preserves the full RDB length in Create Image. Dolly
only reads the prefix within reported capacity and synthesizes zeros
for the tail. Errors reading the prefix still abort image creation.
Surface Scan marks that tail unreadable without issuing reads.
Use as-is attempts reads over the full selected range using the usual
driver addressing and error handling. Dolly rechecks the partition
and reported capacity before opening the source. Operation Restore
pointers are unavailable for these reads until their format can
preserve the selected policy.
