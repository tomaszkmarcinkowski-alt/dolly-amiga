# Dolly M0 — AmigaOS native I/O design (experimental)

**Status: prototype source published, NOT tested with an actual Amiga compiler, NDK, emulator or physical device. No writes exist.**

## Architecture

The portable `DollyBlockSource` owns a caller-provided backend and enforces range checks. The POSIX file adapter resides in `src/dolly_file_source.c`; the separate device adapter is `amiga/dolly_amiga_source.c`, with `DollyAmigaDeviceOptions` in `include/dolly_amiga_source.h`.

`DollyAmigaDeviceOptions` deliberately requires **explicit, trusted** device size, physical block size and maximum transfer size. These are **not guessed** from an unverified driver. A bad capacity value or controller DMA constraints can invalidate the safety guarantee; user-facing native device selection is therefore disabled until device capacity probing and testing are added.

## Initial read path

1. Create a message port and I/O request, with additional request room for driver compatibility.
2. `OpenDevice(name, unit, ...)` for the explicitly selected unit.
3. Query NSD capabilities with `NSCMD_DEVICEQUERY`; validate response length, format, device type, command list.
4. Use `CMD_READ` only for requests completely inside the first 4 GiB; use `NSCMD_TD_READ64` beyond or across the boundary **only if the driver advertised it**.
5. For the 64-bit command, the high 32 bits of the byte offset go in `io_Actual` **before** `DoIO()`; `io_Offset` holds the low 32 bits. The device normally replaces `io_Actual` with the transferred byte count.
6. Require block-aligned offsets and transfer lengths; cap each transfer by user-supplied safe `max_transfer_bytes`. Use a small public-memory bounce buffer. The provisional adapter now requires an explicit, verified DMA profile and checks start address, alignment and buffer extent; controller-specific bus reachability and cache requirements still require hardware validation.
7. Close the device, free its request/message port and memory on all paths.

The device adapter is **read-only**. It contains no `CMD_WRITE`, `TD_FORMAT` or other mutating device command.

## Test coverage

`tests/test_io_plan.c` checks the command choice around 4 GiB, alignment, transfer size and overflow. `tests/test_amiga_device_mock.c` exercises open/read/close and 64-bit capability selection with a **mocked Exec**, not a real OS. Earlier host suites passed in isolated runs, but a full current-branch CTest and CI result has not yet been independently confirmed.

## Important limitations

- A POSIX-free native smoke **source** and CMake target now exist, but no verified native 68000 binary has been built. See [NATIVE_SMOKE.md](NATIVE_SMOKE.md).
- `CreateMsgPort`, `CreateIORequest`, `OpenDevice` and the NSD struct ABI must be checked against an installed Amiga NDK.
- Only the NSD 64-bit path is prototyped; **TD64 is not yet implemented**.
- Actual drive capacity, `TD_GETGEOMETRY`, change counters, driver-specific MaxTransfer, DMA-suitable memory beyond preliminary mask checks, retries, media-change handling and mounted-volume coordination remain unverified.
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

`MEMF_PUBLIC` alone is insufficient evidence of hardware DMA reachability. The adapter now fails closed unless the caller provides a verified controller DMA mask/alignment profile, can optionally request `MEMF_24BITDMA` and rejects a bounce buffer outside the addressable aperture. This remains a **partial** safety check; it does not validate cache coherency, bus routing, controller quirks or active configuration. See [DMA safety](DMA_SAFETY.md) and [Issue #8](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/8).

A local reconstructed adapter/mock suite passed GCC, Clang and GCC ASan/UBSan, with short read and I/O-error injection. Native AmigaOS ABI and real device tests remain open.
