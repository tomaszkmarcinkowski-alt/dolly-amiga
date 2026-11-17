# Sheep Inventory — initial engineering design (prototype)

**Status:** early read-only *host-side* prototype, not the planned AmigaOS GUI. Epic: [Issue #13](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/13) / DOL-060.

## Why a streaming model?
Classic Amigas may have little free Fast RAM and very limited stack space. The first implementation emits one `DollyInventoryField` at a time via a callback; the GUI or CLI consumes each field immediately without allocating an inventory list or copying the entire RDB layout again.

`dolly_inventory_partition()` currently emits 9 fields:
- Partition name and PART descriptor block;
- DosType, low and high cylinder;
- Start offset, byte length and end-exclusive offset;
- RDB filesystem `de_MaxTransfer` (not Dolly's raw backend transfer limit).

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
