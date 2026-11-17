# Dolly Rescue — optional minimal bootable floppy variant

**Backlog only:** not an existing bootable distribution or tested 880 KiB image.

Goal: minimal AmigaOS 2.04+/MC68000 Dolly with **built-in GadTools**, essential read-only hardware/disk inspection, readable error advice and safe CRC32/version checks. No ReAction/MUI, device database, heavyweight codecs or other nonessential external files in the initial image.

Target a standard Amiga **880 KiB DD floppy** but measure real output sizes and OS requirements before promising fit. A bootable floppy may need licensed Workbench/AmigaOS components; do not redistribute copyrighted files without authorization. Provide a reproducible script that assembles a blank licensed disk or ADF with startup files, sets DOLLY: to the floppy root and provides an offline verification path, while allowing storage/memory-related warnings to degrade gracefully.

Release gates: working 68000 cross build, compatible OS startup, screenshot-based 640×256 Topaz/8 GadTools fit, floppy and low-memory boot test in WinUAE, and physical floppy validation. Optional post-MVP edition independent of primary M0 deliverables.

Tracked at [Issue #20](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/20).
