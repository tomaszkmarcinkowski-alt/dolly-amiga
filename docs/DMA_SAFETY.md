# Dolly — DMA safety profile for read-only AmigaOS I/O

**Status:** experimental implementation with verified host mock tests. **NOT** validated against AmigaOS NDK, WinUAE, a real DMA controller or any physical drive.

## Why it matters

Allocating a buffer with `MEMF_PUBLIC` is not proof that every old SCSI/IDE/Zorro controller can address it. Some controllers require memory accessible in the first 24-bit address space and sometimes specific alignment, maximum transfer sizes or cache maintenance. Bad assumptions can cause silent data corruption even when reads appear successful.

The AmigaOS `MEMF_24BITDMA` memory attribute requests an address usable by some 24-bit/Zorro II DMA controllers. It is **not a universal property of all Amiga devices**, so Dolly only requests it when an **explicitly verified device profile** requires it.

## Fail-closed API

`DollyAmigaDeviceOptions` (see `include/dolly_amiga_source.h`) now requires:

| Field | Meaning |
|---|---|
| `capacity_bytes` | Independently established physical device size |
| `physical_block_bytes` | Independently established block size |
| `max_transfer_bytes` | Conservative raw-device I/O limit (currently at most 65536) |
| `dma_address_mask` | Allowed starting address bits for the device DMA profile |
| `dma_alignment` | Required DMA buffer alignment, power of 2 (2–4096) |
| `dma_profile_verified` | Explicit `1` required, otherwise device open is rejected |
| `require_24bit_dma` | Request `MEMF_24BITDMA` only when needed by the device |

The program **does not set** `dma_profile_verified` from the on-disk `de_Mask` alone. RDB values are declarations, not proof of the driver's real DMA constraints. The feature is not exposed as a user-selectable device operation yet.

After allocation, the adapter checks that the buffer's first address satisfies the mask and alignment, and the transfer does not spill beyond allowed high address bits. A rejected address fails the open operation and cleans up allocated resources.

## Mask calculation note

For `mask=0x00FFFFFE` and `alignment=2`, an even start address is required and the **whole transfer** must stay within the lower 16 MiB. The low bit of the mask applies to the DMA start address, not to the last byte of the transfer (which may legitimately be odd). This distinction is covered by boundary tests.

These checks are **necessary, not sufficient**. DMA bus routing, controller firmware limitations, actual memory type, MMU mapping, cache coherency and request-specific restrictions still require controller-aware validation.

## MaxTransfer is separate

- The filesystem's `de_MaxTransfer` is read from RDB as part of Sheep Inventory; `0x1FE00` is a conservative reference for some older IDE implementations, **not a universal value**.
- The backend's `max_transfer_bytes` is an explicitly selected raw `DoIO` chunk limit; it is independent of `de_MaxTransfer`.
- DMA address mask, memory location, alignment and transfer-size limits must be considered together.

## Regression checks

- `tests/test_dma_policy.c`: allowed/disallowed 24-bit address ranges, buffer extent crossing a boundary, alignment, missing profile, overflow.
- `tests/test_amiga_device_mock.c`: rejects missing profiles and out-of-mask addresses; confirms `MEMF_24BITDMA` request flag when selected; confirms 32-bit read gating, valid NSD READ64, **absence of advertised READ64**, short reads and injected I/O errors.

**Local result (2026-11-17):** GCC and Clang compilation with `-std=c99 -Wall -Wextra -Wpedantic -Werror` passed; reconstructed adapter + fake Exec tests passed on both; ASan/UBSan with leak checking passed on the same reconstructed subset. Current full `main` CTest, GitHub Actions, 68000 compiler and real device testing remain **unverified**.

## Sources and follow-up

- [AmigaOS Exec memory](https://wiki.amigaos.net/wiki/Obsolete_Exec_Memory_Allocation)
- [Classic Exec memory.h](https://d0.se/include/exec/memory.h)
- [NSD command spec](https://wiki.amigaos.net/wiki/NSD_Command_Specifications)
- [AmigaOS I/O design](AMIGA_IO.md)
- [Issue #8](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/8)

**Safety rule:** no automatic physical disk I/O or destructive operation is enabled by these checks alone.
