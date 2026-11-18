# Dolly — DMA safety profile for read-only AmigaOS I/O

**Status:** host mocks, native MC68000 builds and emulated UAE-IDE checks. Physical-controller qualification remains open. See [AmigaOS I/O](AMIGA_IO.md).

## Why it matters

Allocating a buffer with `MEMF_PUBLIC` is not proof that every old SCSI/IDE/Zorro controller can address it. Some controllers require memory accessible in the first 24-bit address space and sometimes specific alignment, maximum transfer sizes or cache maintenance. Bad assumptions can cause silent data corruption even when reads appear successful.

The AmigaOS `MEMF_24BITDMA` memory attribute requests an address usable by some 24-bit/Zorro II DMA controllers. It is **not a universal property of all Amiga devices**, and Dolly requests it when the resolved memory attributes require it.

## Fail-closed API

`DollyAmigaDeviceOptions` (see `include/dolly_amiga_source.h`) now requires:

| Field | Meaning |
|---|---|
| `capacity_bytes` | Independently established physical device size |
| `physical_block_bytes` | Independently established block size |
| `max_transfer_bytes` | Conservative raw-device I/O limit (currently at most 65536) |
| `dma_address_mask` | Allowed starting address bits for the resolved DMA configuration |
| `dma_alignment` | Required DMA buffer alignment, power of 2 (2–4096) |
| `dma_profile_verified` | Explicit `1` required, otherwise device open is rejected |
| `buffer_memory` | Resolved Exec memory attributes, combined with `MEMF_PUBLIC` |
| `require_24bit_dma` | Request `MEMF_24BITDMA` and require a compatible address mask |

The native GUI resolves and validates all three parameters for raw reads. Auto uses RDB declarations, and the operator can override fields per unit. The declarations describe the intended configuration. Physical testing is required to establish the actual DMA constraints. See [Amiga I/O policy](AMIGA_IO.md#automatic-settings-and-manual-override).

After allocation, the adapter checks that the buffer's first address satisfies the mask and alignment, and that **every address within the transfer** is permitted by the profile's address aperture. The second check is done using a constant-space range bit-OR calculation, not a per-byte runtime loop. A rejected address fails the open operation and cleans up allocated resources.

## Mask calculation note

For `mask=0x00FFFFFE` and `alignment=2`, an even start address is required and the **whole transfer** must stay within the lower 16 MiB. The low bit of the mask applies to the DMA start address, not to the last byte of the transfer (which may legitimately be odd). This distinction is covered by boundary tests.

These checks are **necessary, not sufficient**. DMA bus routing, controller firmware limitations, actual memory type, MMU mapping, cache coherency and request-specific restrictions still require controller-aware validation.

## MaxTransfer is separate

- The filesystem's `de_MaxTransfer` is read from RDB as part of Sheep Inventory; `0x1FE00` is a conservative reference for some older IDE implementations, **not a universal value**.
- The backend's `max_transfer_bytes` is an explicitly selected raw `DoIO` chunk limit; it is capped by the resolved MaxTransfer for native GUI operations.
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

## Important bug found during validation: sparse masks

The first implementation checked only the first and last addresses of a DMA buffer. That method can incorrectly accept a transfer crossing a **forbidden hole** in an irregular mask even if both endpoints appear valid. A concrete regression uses start `0x00000FF0`, length `0x1020` and mask `0x00002FFE`: the original algorithm returned `OK` while the transfer passes addresses with the prohibited bit 12 set.

The corrected policy evaluates the OR of **all address bit patterns across the interval**, then rejects any prohibited bit. The algorithm's result was compared with a deliberately simple byte-by-byte reference implementation for **6000 deterministic random cases**. GCC and Clang passed; GCC ASan/UBSan with leak checking also passed locally. See [DJ-0040](ENGINEERING_JOURNAL.md).

## User-facing DMA refusal messages (DOL-061)

**Mandatory UX rule:** when a DMA safety check fails, Dolly must not silently return only `EINVAL` or `EUNSUPPORTED`. The operation is stopped, and the caller receives a **specific diagnostic reason, readable message and suggested safe next check**. There is no automatic fallback to unverified address masks, alternate RAM or unsafe alignment.

The experimental native backend provides a `report_dma_problem(reason, message, user)` callback on `DollyAmigaDeviceOptions`. The callback is required before raw device access. `dolly_dma_user_message()` provides a 7-bit English fallback for older AmigaOS displays; the eventual CLI and GadTools dialog must translate messages to Polish/English via locale catalogs and visibly show them.

Example planned Polish warning:

> **Problem z DMA — odczyt zatrzymany.** Bufor znajduje się poza zakresem adresów obsługiwanym przez kontroler. Sprawdź ustawienia kontrolera i maskę DMA. Dolly nie wykonało niebezpiecznego transferu.

Message categories: missing/invalid controller profile, memory outside DMA mask, buffer misalignment, address-range overflow and inability to allocate compatible memory. Messages must not assert that the disk is damaged or that a real transfer failed if Dolly refused to start it.

**Local verification:** a reconstructed adapter/fake-Exec subset was compiled and tested with GCC and Clang (PASS). Callback delivery was verified for rejected profile, alignment and address mask, with no spurious warnings on accepted profiles. Full repository CI, AmigaOS SDK build, actual visible UI and real-controller tests remain open. See [Engineering Journal DJ-0041](ENGINEERING_JOURNAL.md).

## Guided DMA troubleshooting — implemented prototype (2026-11-17)

The new `DollyDmaAdvice` model separates **stable code, title, explanation, 1–3 suggested checks and blocked-operation status**. It is small, uses static strings and avoids dynamic allocations. A stable enum lets the AmigaOS adapter's `report_dma_problem` callback select the same advice that a future GadTools window or a CLI prints.

### Portable CLI example

```
dolly-dma-advice DMA_ADDRESS_MASK pl
```

Sample output (temporary 7-bit PL fallback):

```text
[DMA_ADDRESS_MASK] Bufor poza obszarem DMA
Bufor nie miesci sie w obszarze adresowym dostepnym dla kontrolera.
Operacja zatrzymana. Brak automatycznych zmian ustawien.
1. Sprawdz obslugiwane rodzaje RAM i zakres adresow kontrolera.
2. Ustal, czy wymagany jest RAM zgodny z 24-bitowym DMA.
3. Nie poszerzaj maski tylko po to, aby ominac ostrzezenie.
```

Values are **not** universal repair recipes. Dolly cannot safely tell the user to change `Mask` to `0xFFFFFFFF` or enlarge `MaxTransfer` merely to silence a warning. The appropriate fix depends on the controller model, driver, DMA-visible memory and actual mounted configuration.

### Architecture, build and limitations

- `include/dolly_dma_advice.h` + `src/dolly_dma_advice.c`: read-only catalog, EN and 7-bit ASCII Polish fallback, stable identifiers, three optional safe actions, no heap allocation.
- `src/dolly_dma_advice_cli.c`: standalone command, also included in `DOLLY_NATIVE_ONLY=ON` for future m68k builds; prints a blocked status and exits nonzero for genuine DMA errors.
- `tests/test_dma_advice.c` + `tests/test_dma_advice_cli.py`: tests added for every known status in both languages and CLI exit codes.
- **Actual AmigaOS 2.04/68000 build, GadTools UI, proper Polish locale encoding, full current GitHub CTest and CI are not yet verified.** Do not mark these as complete.
- Existing `dolly_dma_user_message()` is a basic compatibility string; callers use `dolly_dma_get_advice(status,language)` for detailed steps.

## Source-grounded controller guidance (DOL-069)

When Dolly reports a DMA problem, it first displays generic safe checks. An optional profile package under `DOLLY:Plugins/Devices/` may add **controller-specific information** only when all gates pass:

1. The actual database file's integrity is verified and its **release/build identity matches the running Dolly exactly**, including the database format version.
2. The detected **controller model, driver name/version and hardware revision** match one unique profile. An ambiguous or incomplete match does not become advice.
3. The record includes a **source reference and documented known behavior**, not an invented universal DMA Mask or MaxTransfer.

`dolly_dma_diagnostic_build()` combines the generic `DollyDmaAdvice` with the optional sourced device note. For an absent/mismatched/corrupt database, Dolly simply omits the specific note, informs the user about the disabled database, and retains the ordinary troubleshooting path. It **never** sets `dma_profile_verified` or authorizes device reads based solely on the database.

M0 implementation includes only matching/composition interfaces with **synthetic example controller records**. No real hardware profiles, production native database loader or auto-tuning are implemented yet. Before collecting actual entries, every claim needs a vendor manual, driver documentation or reproducible physical test, plus scope limitations (revision, firmware, OS, driver). External corrections must be versioned and auditable; no silent global values.
