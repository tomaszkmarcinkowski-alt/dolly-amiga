# Sheep Inventory — initial engineering design (prototype)

**Status:** early read-only *host-side* prototype, not the planned AmigaOS GUI. Epic: [Issue #13](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/13) / DOL-060.

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
The standalone `dolly_inventory_partition` C99 source and test passed local GCC `-Wall -Wextra -Werror -pedantic` compilation and execution, including missing fields, arithmetic overflow and early stopping. A host CLI was compiled and executed against a synthetic nested-RDB image using a locally reconstructed previous parser snapshot. **Full current GitHub CMake/CTest and native Motorola 68000 builds remain unverified.**

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
