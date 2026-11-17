# Dolly — first native 68000 smoke milestone

**Status (2026-11-17):** user-reported successful cross-build and WinUAE MC68000 / Kickstart 2.04 smoke run (`PASS`, RC=0). Physical Amiga hardware has not been tested. Host CTest is not a substitute for a native run.

## Goal

Prove that the portable read-only core can run as an AmigaOS CLI program on a Motorola 68000 without any POSIX functions, devices, mountlist access or disk writes.

The executable `dolly-native-smoke` supports:
- `--version` — print the tool identity;
- `--selftest` — build a tiny synthetic RDSK/PART inside an in-memory buffer, parse it, verify checksums, inspect byte ranges, execute the RDB collision audit, stream the PART's Sheep Inventory fields, evaluate MaxTransfer, verify DMA mask/address boundaries and check NSD64 command planning;
- `--help` / no arguments — show instructions.

**This tool does NOT open any device.** It cannot write to any disk. A successful smoke test proves only a small portable code path, not the correctness of AmigaOS I/O drivers, NSD/TD64, DMA or bootability.

## Host dry run

```sh
cmake -S . -B build-native-host -DDOLLY_NATIVE_ONLY=ON
cmake --build build-native-host
ctest --test-dir build-native-host --output-on-failure
./build-native-host/dolly-native-smoke --selftest
```

Use a fresh separate build directory: the native-only flag deliberately avoids all POSIX targets.

## Cross compilation — requires external Amiga SDK/toolchain

```sh
cmake -S . -B build-m68k -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/amigaos-m68k.cmake
cmake --build build-m68k
```

Expected compiler: `m68k-amigaos-gcc` with appropriate AmigaOS headers and libraries; compiler flags request `-m68000` for CPU compatibility. The executable may need packaging or startup adjustment according to that toolchain's runtime. It is *not* provided by this repository and is *not* available in the current build environment.

**Do not claim native validation based on the host dry-run.** The next verification requires transferring the binary into a WinUAE setup booting AmigaOS 2.04+ on an emulated 68000, then running:

```
dolly-native-smoke --version
dolly-native-smoke --selftest
```

Capture output, CPU model, OS version, free memory, binary size, build command and any startup/runtime failures in [Engineering Journal](ENGINEERING_JOURNAL.md).

The self-test prints a **stage number** if it fails and the size of the heap-allocated `DollyRdb` result. Capture both, especially on low-memory Amigas. An emitted PASS is only the memory-based self-test result; physical device I/O is not exercised.

## Known limitations and follow-ups

1. Confirm NDK v39/AmigaOS 2.04 compatibility for all linked runtime/library symbols (including 64-bit integer helper routines).
2. Measure stack and Fast/Chip RAM use. The RDB result is heap-allocated.
3. Verify portable `BlockSource` callbacks on the native ABI and inspect 32-bit `size_t` behavior.
4. Only after this succeeds, expose **read-only** physical devices through a separately validated adapter: DMA-safe memory, `Mask`, geometry and media-change handling are currently unresolved.
5. Expand the test plan to classic 68020+, PiStorm/Emu68, NSD and TD64 drivers once basic 68000 behavior is known.

See also [AmigaOS I/O limitations](AMIGA_IO.md), [Issue #3](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/3) and [Issue #8](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/8).

## Host regression update (2026-11-17)

The native-only source path also exercises `dolly_dma_check_buffer()`: a valid aligned lower-16-MiB range and a deliberately overflowing 24-bit DMA aperture. A locally reconstructed source subset compiled with GCC and Clang (`-std=c99 -Wall -Wextra -Wpedantic -Werror`) and ran `--selftest` successfully. GCC ASan/UBSan with leak checking also passed.

On this **64-bit host**, the heap-allocated `DollyRdb` result reports **14376 bytes**. Do not project this number onto m68k without actually compiling the target; sizes may differ with alignment/ABI.

The reconstructed host source subset is not a complete checkout of current `main`. GitHub Actions, the real m68k cross build and WinUAE are not yet independently validated.

The native-only memory smoke now includes a third DMA policy check rejecting an interval that crosses a forbidden **interior hole** in an irregular address mask. The reconstructed local native-only source path (GCC, Clang and GCC ASan/UBSan) completed `--selftest` successfully after the DMA policy fix. This remains host verification, not a native AmigaOS result.

## Standalone DMA help command (source ready)

The `DOLLY_NATIVE_ONLY` CMake mode also defines the portable `dolly-dma-advice` executable. After cross-compilation, it can print a stable diagnostic code, probable cause and up to three safe troubleshooting steps for DMA problems:

```
dolly-dma-advice DMA_ADDRESS_MASK en
dolly-dma-advice DMA_ALIGNMENT pl
```

The `pl` catalog currently uses Polish written with 7-bit ASCII characters as a compatibility fallback; proper Polish locale catalogs and the final GadTools warning window remain future work. This helper accesses no disk, and has not yet been executed in WinUAE or on physical AmigaOS.

## Optional native ScreenMode/Screen Font watcher build

For NDK-enabled native cross-build validation, add `-DDOLLY_NATIVE_GUI_WATCH=ON` alongside the existing AmigaOS toolchain file. This requests compilation of an **experimental** `dolly-native-gui-watch` static library containing `amiga/dolly_amiga_pref_notify.c` and `src/dolly_screen_watch.c`. It does **not** create a running GUI nor prove DOS/Intuition integration. Do not enable it for the plain host-only `DOLLY_NATIVE_ONLY` smoke test, as that host lacks AmigaOS NDK headers.

## WinUAE MC68000 result and memory checkpoints (2026-11-17)

Environment reported by tester: Windows 11 + Docker + Ubuntu on WSL2,
WinUAE configured for MC68000, 512 KiB Chip RAM and Kickstart 2.04.
The Amiga-format executable was verified by `m68k-amigaos-objdump -f`
(`architecture: m68k:68000`). The default runtime failed to open
`mathieeedoubbas.library` (absent in the tested image). Rebuilding with
`-noixemul` (libnix) fixed startup and yielded `Dolly native smoke: PASS`,
`RDB result allocation: 13604 bytes`, and return code 0.

The shell's free-memory reports were 347352 bytes before and 347328 bytes
after the process, a difference of 24 bytes. This is **not** evidence of
24 bytes of peak memory use or a confirmed leak.

The native-only m68k smoke now records four AmigaOS `AvailMem()` checkpoints:
`start`, `allocated`, `parsed`, `released`. Each `MEM` line shows
free Chip RAM, Fast RAM, total RAM and the largest contiguous free block.
Sampling avoids heap allocation and is excluded from ordinary host builds.
These values are **snapshots**, not a true peak heap or stack measurement;
they include unrelated OS activity and effects of the C runtime allocator.
The largest-block value is important on low-memory systems.

Fresh build using Dockerized `m68k-amigaos-gcc`:

```sh
cmake -S . -B build-m68k-memory \\
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/amigaos-m68k.cmake \\
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-m68k-memory -j2
```

The toolchain preset defaults to `-noixemul`; the compile target defines
`DOLLY_AMIGAOS_MEMORY_PROFILING`. After copying the executable to the
WinUAE shared directory, run `Stack 16384`, then
`dolly-native-smoke --selftest` and `Echo $RC`.
