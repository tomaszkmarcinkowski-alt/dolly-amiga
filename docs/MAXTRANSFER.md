# Dolly MaxTransfer audit — read-only M0 diagnostic

**Status:** first host-side implementation is available; the final safety decision remains driver-dependent.

## What Dolly reads
For each recognized RDB `PART` descriptor, Dolly reads `de_MaxTransfer` from the `DosEnvec` (index 13, big-endian 32-bit value). This is the **maximum single transfer requested by the filesystem**, not a byte/s rate. It is **not** Dolly's own direct-device `DoIO` transfer cap, and it does not replace the DMA `Mask`.

The preliminary `dolly-rdb` and `dolly-layout` host CLIs display the value in hexadecimal and bytes.

## Classification table

| Value | Category | Response |
|---|---|---|
| Missing | `NOT_PRESENT` | Unknown; inspect active configuration |
| `0` | `INVALID_ZERO` | Suspicious, verify mount details |
| Below `0x1FE00` | `BELOW_REFERENCE` | Informational; smaller may be deliberate |
| `0x1FE00` | `AT_REFERENCE` | Agrees with conservative legacy IDE setting |
| Above `0x1FE00` | `ABOVE_REFERENCE` | Warn of possible silent corruption on some legacy IDE setups; investigate driver before suggesting a change |

**0x0001FE00 = 130560 bytes = 255 × 512 B.**

It is **not universally optimal or safe**. Some older drives/controllers need smaller values; newer SCSI/IDE controllers and drivers may support larger values safely. Dolly must never change RDB, filesystem mount settings or device-driver settings without informed, explicit user approval.

## Future work
1. Compare RDB value with active `DosList` and mountlist entries. The active system may override the stored value.
2. Classify controller and driver using confirmed capabilities, not only device string `scsi.device` (which is used by both IDE and SCSI contexts).
3. Verify and report `Mask`, `BufMemType`, DMA alignment and constraints separately.
4. Add per-driver safe-profile recommendations with documented evidence and test fixtures.
5. Keep all suggestions non-destructive until the disk-writing phase is separately reviewed.

## Verification
- Unit test: `tests/test_maxtransfer.c` checks zero, missing, below/reference/above threshold.
- RDB fixture: `tests/test_rdb.c` validates extraction from `pb_Environment`.
- CLI fixture: `tests/test_cli_smoke.py` verifies a warning for `0x7FFFFFFF` in nested RDB.
- **Limit:** local GCC+ASan/UBSan test for the standalone classifier passed; after repository integration the complete CI result still needs independent verification.

## References
- [Amiga ROM Kernel Reference Manual: DOS](https://www.aminet.net/docs/misc/rkrm-dos.pdf)
- [AmigaOS 3.9 FAQ: disk operation](https://www.gregdonner.org/os39faq/diskfileoperation.html)
- [DosEnvec definitions (NDK)](https://amigadev.elowar.com/read/ADCD_2.1/Includes_and_Autodocs_2._guide/node006A.html)
- [Issue #11](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/11)
