# Dolly — first native 68000 smoke milestone

**Status:** source and an isolated CMake build route are published; not yet compiled with an actual AmigaOS cross compiler or executed in WinUAE or on real hardware. The host C compiler is NOT a native build.

## Goal

Prove that the portable read-only core can run as an AmigaOS CLI program on a Motorola 68000 without any POSIX functions, devices, mountlist access or disk writes.

The executable `dolly-native-smoke` supports:
- `--version` — print the tool identity;
- `--selftest` — build a tiny synthetic RDSK/PART inside an in-memory buffer, parse it, verify checksums, inspect byte ranges, execute the RDB collision audit, stream the PART's Sheep Inventory fields, evaluate MaxTransfer and check NSD64 command planning;
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
