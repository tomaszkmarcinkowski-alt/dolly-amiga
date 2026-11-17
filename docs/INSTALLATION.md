# Installation and directory conventions — Dolly

**Status:** design + portable asset-path API only; the final classic Installer script has **not** been implemented or run on AmigaOS.

## One installation root: `DOLLY:`

The program and its optional data should live under one user-chosen installation drawer. The installer creates an AmigaDOS assignment `DOLLY:` to it, and optionally makes that assignment persistent in `S:User-startup` with explicit user consent.

Example, **after choosing the actual destination folder**:

```amigados
Assign DOLLY: Work:Apps/Dolly
```

Illustrative future package layout:

```text
DOLLY:
    Dolly
    Catalogs/
    Docs/
    Icons/
    Presets/
    Plugins/       ; optional, POST-MVP ReAction/MUI backends
```

All resource references use the assign as an absolute **logical** root; e.g. `DOLLY:Docs/Help.guide`, `DOLLY:Catalogs/dolly.catalog` and, post-MVP, `DOLLY:Plugins/ReAction.gui`. **No dependence on the Shell's current directory.** The system `LIBS:`, `FONTS:`, `LOCALE:` and `DEVS:` are not redirected or replaced.

For a portable program launched before installation, use an explicit bootstrap in the *program's installation directory* to establish the assign after checking it. Do not silently assign `DOLLY:` to an arbitrary working directory. If the assign is absent or points somewhere else, show a precise, helpful error and provide installation/bootstrap steps rather than guess.

### Portable M0 helper

`dolly_path_resource(relative, out, cap)` produces a bounded AmigaDOS logical path beginning with `DOLLY:`. It rejects absolute paths, nested `..` traversal, `.` components, colons, backslashes and control characters. This helper **does not create any assignment and does not touch the filesystem**. It is tested on the host and included in the native-only CMake source subset for later m68k checks.

Do not treat path construction as a guarantee that an existing `DOLLY:` assignment resolves to a trusted installation directory. The actual AmigaOS adapter must verify the destination and file content as needed.

## Classic Installer (DOL-065, post-M0 packaging)

The requested installer targets **Commodore's classic Installer script language**, not AmigaOS 4's Python-based Installation Utility. Compatibility target: AmigaOS 2.04+ on Motorola 68000.

Installation steps:
1. Welcome, license and disk-space check; choose destination drawer.
2. Detect an existing installation and previously assigned `DOLLY:`, present a clear choice. Never overwrite the user's existing assign silently.
3. Copy executable/resources. Do not install ReAction/MUI backend modules for MVP; optional backend packages arrive only after MVP.
4. Establish the `DOLLY:` assignment for the session after successful file installation. Offer a **separate opt-in** to edit `S:User-startup` for a persistent assign.
5. For any startup edit: check for an existing `DOLLY:` entry, avoid duplicate lines, preserve other user edits, make a backup where possible and test rollback.
6. Reinstall/update preserving configs and custom presets. Provide uninstall instructions that remove only Dolly-owned files/lines and respect user choice.
7. Test on OS 2.04, 3.1, 3.1.4 and 3.2 in WinUAE, including low RAM, floppy-only setups and no available `Installer` executable.

A startup assignment may be created immediately:

```amigados
Assign DOLLY: Work:Apps/Dolly
```

For drives not mounted during startup, the classic `DEFER` option exists, but the installer should not use it automatically; choose based on deployment context. Always prefer a correct user-selected destination.

**Source:** [AmigaOS Assign command reference](https://wiki.amigaos.net/wiki/AmigaOS_Manual%3A_AmigaDOS_Command_Reference), [classic Installer tutorial](https://www.ppa.pl/programy/kurs-installera.html), [Installer 43.3 distribution reference](https://www.amigaforever.com/classic/download/).

**Progress tracking:** [Issue #16](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/16).
