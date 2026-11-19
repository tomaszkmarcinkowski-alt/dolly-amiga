# Sheep Inventory — initial engineering design (prototype)

**Status:** host CLI and native GadTools prototype. Native views include device geometry, RDB/MBR partition details, clipboard copying and metadata backup. Restore RDB has a restricted offline implementation. Restore MBR is pending. Historical sections preserve their original scope. Epic: [Issue #13](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/13) / DOL-060.

## Why a streaming model?
Classic Amigas may have little free Fast RAM and very limited stack space. The first implementation emits one `DollyInventoryField` at a time via a callback; the GUI or CLI consumes each field immediately without allocating an inventory list or copying the entire RDB layout again.

`dolly_inventory_partition()` currently emits 17 fields:
- Partition name and PART descriptor block;
- DosType, low and high cylinder;
- Start offset, byte length and end-exclusive offset;
- RDB filesystem `de_MaxTransfer` (not Dolly's raw backend transfer limit);\n- `de_NumBuffers`, `de_BufMemType`, `de_Mask`, `de_SizeBlock`, `de_SectorPerBlock`, `de_Reserved` and `de_PreAlloc`, with explicit field origin and availability.

Each field contains a stable key, display label, unit, typed value (`uint64_t` or text), data origin (`RDB_PART` or `DERIVED`), and an availability flag. Missing DosType and missing MaxTransfer are intentionally distinct from numeric zero. Overflow during end-offset computation is presented as unavailable.

Callbacks are synchronous; text pointers are borrowed from the source. The callback must not retain them after the producer or source object changes. A nonzero callback result stops enumeration early; no disk I/O is performed by the inventory field generator.

## Host CLI
```sh
cmake -S . -B build
cmake --build build
./build/dolly-inventory sd.img
./build/dolly-inventory sd.img 512
```

The CLI reads primary MBR and nested RDB or a whole-device RDB and formats PART information. It uses a heap-allocated partition-map object. It does not scan filesystem contents, read disk device units directly, query health metrics, or change any media.

## Constraints and next steps
1. Add separate field providers for **devices**, **images**, **RDB/MBR**, **filesystems**, **volumes** and runtime AmigaDOS Mount/DosList records.
2. Include richer status/provenance: observed vs declared vs calculated vs estimated, collection timestamps and controller capability evidence.
3. Preserve distinctions: unavailable, unsupported, not queried, permission denied, inconsistent, present-but-zero.
4. Introduce an explicit machine-readable field identifier and optional localization key; avoid treating English labels as stable APIs.
5. Add an owner-level warning aggregator (RDB collision audit, MaxTransfer advisory, bad checksum). Do not mistake zero warnings for verified device health.
6. Only explicitly requested extended analysis should traverse OFS/FFS trees; cost and cancellation must be visible.
7. GadTools low-memory window for AmigaOS 2.04+, optional ReAction; full information in CLI and readable logs.
8. A future Flock Manager can reuse **user-approved, privacy-filtered** inventory metadata, not automatically uploaded hardware IDs.

## Verification
The standalone `dolly_inventory_partition` C99 source and test passed local GCC `-Wall -Wextra -Werror -pedantic` compilation and execution, including missing fields, arithmetic overflow and early stopping. A host CLI was compiled and executed against a synthetic nested-RDB image using a locally reconstructed previous parser snapshot. For later CMake/CTest and native MC68000 coverage, see the [test matrix](TEST_MATRIX.md).

## Incremental providers added (November 2026)

The common callback now supports three provenance types beyond computed values: `RDB PART`, `MBR`, and `RDSK` header. The MBR provider emits 7 fields; the RDB header provider emits 7; and the partition provider emits 17. These are **preliminary**, not exhaustive.

Availability states distinguish `present`, `not present`, `not queried`, `unsupported` and `inconsistent`. Only `present` is currently generated from parsed header fields; the others are reserved for deeper inspection providers and are not substitutes for observed failures.

Cross-source contradictions (e.g. mounted vs on-disk settings) must report both values and their origins, never collapse them into one without explanation. `de_Mask` represents AmigaDOS's address mask; interpreting its safety requires identifying the real controller and buffer path.

Local host unit verification for the extended streaming interface passed GCC/Clang. A full current-repository CTest run is **not yet independently confirmed**.

### Evidence and trust model

Every inventory field additionally carries `DollyInventoryEvidence`:

| Evidence | Meaning |
|---|---|
| `DECLARED` | Recorded on disk (MBR/RDB/PART); does **not** prove the device currently uses it |
| `OBSERVED` | Measured or queried from a running device/system (future providers) |
| `DERIVED` | Calculated from other metadata, with bounds checks |
| `ESTIMATED` | Approximation that should be explicitly labelled |

The current MBR/RDB providers emit **declared** and **derived** only. Active controller limits, mounted filesystem options and actual I/O capabilities will require dedicated, read-only AmigaOS providers.

## Shared advisory feed — implemented prototype

`dolly_inventory_partition_notices()` emits stable diagnostic codes and severities without allocating a list or changing the disk. Initial rules include above/below/missing/zero MaxTransfer, nonstandard `de_SectorPerBlock`, suspicious zero `de_Mask`, and absent `DosType`.

`dolly_inventory_rdb_notices()` uses the existing RDB collision audit to report overlapping partitions, header/PART descriptor collisions, out-of-container extents and invalid audit parameters. The `dolly-inventory` CLI prints these messages after the corresponding object. Absence of warnings is **not** proof of a healthy disk.

`DollyInventoryNotice.code` is stable and nonlocalized, whereas the English message is provisional; localization and tailored suggestions will be added for GadTools. The warning feed is distinct from the raw `InventoryField` data stream, so clients can filter, group and display severity consistently.

Local standalone compiler checks: C99 GCC + Clang **PASS**, ASan/UBSan for partition notices **PASS**, and GCC/Clang after adding RDB collision notices **PASS**. Full current GitHub Actions and native AmigaOS tests remain unverified.

### Signed values and boot configuration

The `PART` provider now reads `de_BootPri` (index 15), preserving its signed 32-bit interpretation. The field stream supports `DOLLY_INV_I64` with an explicit `signed_number` slot; it does not coerce a negative priority into a huge unsigned value. Absent fields remain marked as unavailable.

Example test fixture: `de_BootPri=-5` appears as `-5 priority` in `dolly-inventory`. This value is **declared in RDB**, not proof of the current boot selection order. Host unit tests under GCC/Clang/ASan/UBSan passed locally.

## Guided response to detected problems (2026-11-17)

`DollyInventoryNotice` now carries `suggested_action` in addition to its stable code, severity and human-readable message. The host `dolly-inventory` CLI prints **Suggested check:** below each emitted notice. The current set covers MaxTransfer, unusual `DosEnvec` values and RDB range collisions, and is **strictly non-destructive**.

For example, `MAXTRANSFER_ABOVE_REFERENCE` recommends identifying the actual controller and driver and verifying their documented transfer limit before changing the mount configuration. For `RDB_PARTITIONS_OVERLAP`, the user is directed to compare the byte/LBA ranges and leave the medium read-only until the layout is understood.

The English text is provisional and must be replaced by locale catalog references for the final Polish/English GUI. Absence of suggested action or missing hardware information must never cause Dolly to invent a universal fix.

The new CLI smoke assertion checks that the MaxTransfer warning has a visible suggested action. Full current GitHub CI and AmigaOS operation remain unverified.

## M1.45 native GadTools first step

A small standalone Sheep Inventory window is now wired to the existing
`Inventory` Operation + `Start` path. The main window's reserved blank
buttons are deliberately unchanged. Inventory does not start synthetic
progress and does not claim that copying or writing has begun.

For Physical or Partition source paths generated by Dolly's picker, the
window parses the driver name and actual controller unit number from the
saved selection (not from the picker index). Only the explicit Start action
runs an additional `dolly_amiga_device_query()`. Valid capacity, sector
size and the source of the geometry are displayed. With 512-byte sectors,
the existing bounded read-only partition scanner may list up to 16
validated RDB/MBR partition labels; otherwise an explicit status is shown.
This is a point-in-time snapshot. Declared partitions are not proof of
filesystem correctness. The native window does not yet stream the full
host-side InventoryField API, perform filesystem audits, support arbitrary
logical/file media or preserve persistent hardware profiles. These are
separate subsequent milestones.

The window is implemented in `amiga/dolly_amiga_inventory_view.c` and is
built with the native demo; it is **not yet cross-compiled or WinUAE
verified**. Hardware access must be tested on disposable images before
relying on any device-specific results.

## M1.46 — partition selection details

The native Sheep Inventory list is still interactive, but occupies the
left side of the existing window. Its right side displays attributes of the
currently selected PART record without issuing another device query.
The scanner retains explicit presence flags for DosType (DosEnvec index 16),
BootPri (15), DMA Mask (14), and MaxTransfer (13). An absent field is
shown as `unavailable`; a valid zero remains zero. BootPri is signed.

RDB records expose DosType in hexadecimal and escaped ASCII (for example
`DOS\1` and `PFS\3`), with the known filesystem family and variant.
They also show signed BootPri, the declared Bootable and Automount flags,
MaxTransfer, Mask, filesystem block size, SectorPerBlock, cylinder range
and partition size. `DOS\6` and `DOS\7` identify long-filename formats.
The DosType does not identify the installed filesystem handler version
or the version of AmigaOS that formatted the disk.

For MBR entries, Inventory shows the role, active flag,
type code and common name, inclusive LBA range, sector count and size.
The type name comes from the MBR entry. Dolly does not probe the actual
filesystem. Extended entries and type 0x76 appear as containers without
scanning their contents. These values are
read from the on-disk partition metadata and do not certify mounted
configuration or DMA safety. The source list and main Dolly layout stay
otherwise unchanged. The MC68000 Release cross-build passed. WinUAE
validation on AmigaOS 2.04 is pending.

### Backup RDB and Backup MBR

For a supported whole-disk RDB on a queried 512-byte device, the
**Backup RDB** button opens the AmigaOS ASL save requester. Choose a
drawer and a new filename. Dolly revalidates the partition map and
streams the raw sectors from LBA 0 through the RDB's declared reserved
area. Starting at LBA 0 preserves the original offsets. The source
device is accessed with read commands.

The output is a raw binary prefix of the source disk. Dolly publishes
it under the selected name after the temporary `.part` file has been
written and closed. An existing destination is refused. Partition data
and a restore workflow are outside the scope of this export.

The **Backup RDB** button stays visible and disabled when no valid RDB
backup range is available, including on a disk without an RDB.

For a supported primary MBR on a queried 512-byte device, **Backup MBR**
uses the same ASL save flow. It exports the full LBA 0, including boot
code, the primary partition entries and the MBR signature. Export is
disabled for layouts with extended or nested RDB containers. Protective
MBR (GPT) and hybrid layouts remain unsupported. Restore MBR is pending.

**Restore RDB** is always enabled and opens the destructive-operation
requester before checking target eligibility. Writing requires a ROM
scsi.device driver, 512-byte sectors, a 16–128-sector
reserved prefix, and a target with no AmigaDOS registration and no
other observed opens. The original and replacement must have the
same RDB geometry, metadata locations, partition extents and DosTypes.
Both snapshots must be free of a populated MBR at LBA 0. The
first implementation accepts changes to the validated RDSK/PART
records only, so filesystem driver segments and other sectors must
match. It is intended for compatible metadata recovery, including
changes to partition labels or flags.

Select a backup and a separate path for the mandatory recovery copy.
The shared Proceed/Cancel requester delays approval for ten seconds.
Dolly saves the current metadata, rereads the original reserved area
and aborts if it changed, then writes and reads back changed sectors.
It also verifies the complete reserved area after the write sequence.
The RDSK header, when changed, is verified last. This is a sequence
of writes. If a failure leaves a partially updated or invalid RDB,
keep the recovery file and use an independent recovery environment.

### Resizable window

The header shows Driver and the actual controller unit, Device Name,
then two Geometry lines. The first shows the reported cylinders, heads
and sectors per track. The second shows capacity, sector size and sector
count. CHS comes from TD_GETGEOMETRY or SCSI MODE SENSE pages 4 and 3
when the driver supports them. Unreported values are left unknown.
Reported CHS may be translated by a controller or emulator.

The header stays anchored at the top and the buttons stay anchored at
the bottom. The heading above the list is `4 MBR Partitions:` or
`4 RDB Partitions:`, using the current count. MBR rows show the role and
the original slot number, for example `MBR Partition (Extended) 2`.
The partition and details lists touch and have equal heights. A recessed
status box spans both lists directly above the buttons. Resizing updates
the GadTools controls after replying to the queued messages and preserves
the selected partition without querying the source again.

### Experimental lists and clipboard

Clicking a row in the driver/geometry header or details list copies the
entire row, including text clipped by the list width. The left list
selects which partition to display. **Edit → Copy** or **Right Amiga+C**
copies the selected header or detail row again.
Dolly writes IFF FORM FTXT with a CHRS
chunk to clipboard.device unit 0. [GadTools listview](https://wiki.amigaos.net/wiki/GadTools_Gadgets#Listview_Gadgets) · [Clipboard Device](https://wiki.amigaos.net/wiki/Clipboard_Device#Writing_to_the_Clipboard_Device)

Character-range selection in the drawn details pane would require
application-owned text, Intuition mouse tracking, hit testing against
the screen font and selection highlighting. The same Copy action
could export the resulting range through clipboard.device.

Every click on **Restore RDB** opens the shared ten-second confirmation.
After Proceed, Inventory checks target eligibility and displays an explicit
reason for any refusal. Eligible operations retain the final confirmation
for the validated write plan, including its LBA range.


### Header and partition list spacing

The header LISTVIEW has five rows, including ATA IDENTIFY. Dolly
measures it using the screen font and places the partition captions
and both partition lists below it. On resize, the two lists grow
together and the status row and buttons stay anchored at the bottom.
