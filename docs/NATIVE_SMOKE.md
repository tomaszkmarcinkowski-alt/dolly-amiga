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

### M1.1 real WinUAE checkpoint result (2026-11-17)

User-confirmed native execution on MC68000, 512 KiB Chip RAM,
Kickstart 2.04 (no Fast RAM):

```text
Dolly native smoke: PASS
RDB result allocation: 13604 bytes
MEM start        chip=278672 fast=0 total=278672 largest=277944
MEM allocated    chip=262288 fast=0 total=262288 largest=261560
MEM parsed       chip=262288 fast=0 total=262288 largest=261560
MEM released     chip=278672 fast=0 total=278672 largest=277944
RC=0
```

Free memory fell by **16384 bytes (16 KiB)** between the start and
allocated checkpoints, and the largest contiguous free block fell by the
same amount. Both returned **exactly** to their starting values at the
released checkpoint. This supports correct cleanup in the measured interval,
not a general no-leaks claim; checkpoint sampling does not include the
program's loader/runtime baseline, transient peaks, or stack high-water.
The 13604-byte RDB structure is not the full process memory footprint.

The provisional WinUAE compatibility baseline remains MC68000,
512 KiB Chip, Kickstart 2.04, without Fast RAM.

## M1.2 synthetic 320x256 Chip RAM feasibility probe

After validating the smoke checkpoints, native m68k builds provide
`dolly-native-smoke --chip-budget`. This **does not** open a screen,
install bitplanes or access any drive. It attempts three independent,
sequential allocations with `AllocMem(MEMF_CHIP)`, touching the first
and last byte, then freeing each allocation:

| Depth | Colours | Bitplane bytes |
| --- | ---: | ---: |
| 2 bitplanes | 4 | 20480 |
| 3 bitplanes | 8 | 30720 |
| 4 bitplanes | 16 | 40960 |

The probe skips a candidate unless at least 64 KiB would remain free,
including within the largest contiguous allocation, **before** allocating.
It prints PASS/FAIL/SKIP, free memory before/during/after, and returns 10
on allocation or cleanup failure. SKIP does not indicate a successful
allocation, and no test result is claimed until it is run in WinUAE.
The sizes cover raw planar buffers only: not copperlists, screens, sprites,
GUI, audio, filesystem buffers or system overhead. The sample beep remains
optional and is deliberately excluded from this initial probe.

To test on the validated MC68000 + 512 KiB Chip + Kickstart 2.04 setup:

```text
Stack 16384
DOLLYTEST:dolly-memory --chip-budget
Echo $RC
```

### M1.2 WinUAE result (2026-11-17)

Tester confirmed all three `--chip-budget` allocations on MC68000,
512 KiB Chip RAM, Kickstart 2.04, no Fast RAM:

```text
Dolly native smoke: PASS
RDB result allocation: 13604 bytes
MEM start       chip=277928 fast=0 total=277928 largest=277200
MEM allocated   chip=261544 fast=0 total=261544 largest=260816
MEM parsed      chip=261544 fast=0 total=261544 largest=260816
MEM released    chip=277928 fast=0 total=277928 largest=277200
CHIP 2 planes bytes=20480 status=PASS free_before=277920 free_during=257440 free_after=277920
CHIP 3 planes bytes=30720 status=PASS free_before=277920 free_during=247200 free_after=277920
CHIP 4 planes bytes=40960 status=PASS free_before=277920 free_during=236960 free_after=277920
RC=0
```

All three independent allocations restored their initial free Chip RAM.
These confirm raw planar-buffer feasibility, **not** a rendered Intuition
screen or the full Sheep Counter memory footprint.

## M1.3 — first real Sheep Counter screen (UNVERIFIED)

Experimental AmigaOS 2.04+ `dolly-screen-smoke` target opens its **own**
320x256, 2-bitplane (four-colour) Intuition screen, draws a few text lines
and closes after about 250 display frames. It captures Chip RAM and largest
contiguous free block before opening, while open, and after closing. It
requires at least 128 KiB available Chip RAM and a largest contiguous
block of 64 KiB before attempting to open.

This is **not** the final Sheep Counter: no real sheep graphics, progress
animations, user input, audio, gadget UI, disk reads or writes. The screen
smoke is a separate CLI executable; the proven memory-test binary is left
untouched.

The screen target is compiled only in an actual m68k native-only build and
links against AmigaOS's system library stubs (`-lamiga`). In the existing
Windows 11 + WSL2 Ubuntu + Docker workflow, use a fresh build folder:

```sh
cmake -S . -B build-m68k-screen \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/amigaos-m68k.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-m68k-screen -j2
```

Copy `build-m68k-screen/dolly-screen-smoke` to the WinUAE shared
`DOLLYTEST:` directory, then run:

```text
Stack 16384
DOLLYTEST:dolly-screen-smoke
Echo $RC
```

Capture the full `SCREEN before`, `SCREEN opened`, `SCREEN closed`
and `SCREEN PASS/FAIL` output, or any compile/runtime error. A screen
allocation can legitimately fail on a 512 KiB machine; record and analyze
rather than increasing RAM. CloseScreen cleanup is mandatory. Do not claim
a native PASS until the tester has run it under WinUAE.

### M1.3 WinUAE screen result and fragmentation follow-up (2026-11-17)

Tester reports native `dolly-screen-smoke` successfully opened and closed a
320x256 two-bitplane Intuition screen on MC68000 / 512 KiB Chip /
Kickstart 2.04. The first measured run was:

```text
SCREEN before chip=306760 largest=306160
SCREEN opened chip=283800 largest=283712
SCREEN closed chip=306760 largest=283544
SCREEN PASS
```

The screen occupied **22960 bytes of free Chip RAM** while open;
all 22960 bytes were returned after closing. However, the largest
contiguous free block remained **22616 bytes smaller** than before
the screen opened, despite unchanged total free memory. This
may indicate fragmentation or other allocator/runtime behavior,
not necessarily a leak. The first implementation's `SCREEN PASS`
was based only on restoration of total free Chip RAM.

The smoke tool now prints separate messages for free-RAM cleanup
(PASS/FAIL) and largest-block changes (WARN/NOTE). Follow-up:
run the screen test three times consecutively in the same WinUAE
session and record `before/opened/closed` for each. Track whether
the largest block stabilizes, shrinks repeatedly, or recovers.
Do not treat an isolated `SCREEN PASS` as proof of fragmentation-free
operation. No device or disk access is performed by this test.

### M1.3 follow-up native screen run (2026-11-17)

A subsequent tester-supplied WinUAE screenshot shows:

```text
SCREEN before chip=306512 largest=283528
SCREEN opened chip=283552 largest=283096
SCREEN closed chip=306512 largest=283880
SCREEN PASS: total free Chip RAM restored
SCREEN NOTE: largest free block grew by 352 bytes
```

Total free Chip RAM returned to its initial value; transient
screen allocation accounted for 22960 bytes. The largest free
block increased by 352 bytes in this run, as opposed to the
22616-byte decrease observed in the earlier run. Memory layout
varies between runs, and these snapshots alone do not establish
either an accumulating memory leak or absence of fragmentation.
Further consecutive runs in the same emulator session are useful.
