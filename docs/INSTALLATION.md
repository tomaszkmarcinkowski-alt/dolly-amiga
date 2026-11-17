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
    Plugins/
        GUI/          ; post-MVP ReAction/MUI GUI adapters
        Filesystems/  ; optional filesystem support
        Devices/      ; known controller/device profiles
        Formats/      ; future disk image formats
        Compression/  ; future codecs
```

All resource references use the assign as an absolute **logical** root; e.g. `DOLLY:Docs/Help.guide`, `DOLLY:Catalogs/dolly.catalog` and, post-MVP, `DOLLY:Plugins/GUI/ReAction.module`. **No dependence on the Shell's current directory.** The system `LIBS:`, `FONTS:`, `LOCALE:` and `DEVS:` are not redirected or replaced.

For a portable program launched before installation, use an explicit bootstrap in the *program's installation directory* to establish the assign after checking it. Do not silently assign `DOLLY:` to an arbitrary working directory. If the assign is absent or points somewhere else, show a precise, helpful error and provide installation/bootstrap steps rather than guess.

### Portable M0 helper

`dolly_path_resource(relative, out, cap)` produces a bounded AmigaDOS logical path beginning with `DOLLY:`. It rejects absolute paths, nested `..` traversal, `.` components, colons, backslashes and control characters. This helper **does not create any assignment and does not touch the filesystem**. It is tested on the host and included in the native-only CMake source subset for later m68k checks.

Do not treat path construction as a guarantee that an existing `DOLLY:` assignment resolves to a trusted installation directory. The actual AmigaOS adapter must verify the destination and file content as needed.

## Classic Installer (DOL-065, post-M0 packaging)

The requested installer targets **Commodore's classic Installer script language**, not AmigaOS 4's Python-based Installation Utility. Compatibility target: AmigaOS 2.04+ on Motorola 68000. **The classic Installer executable is not guaranteed to be present on a bare OS 2.04 system** (the language entered distributions around 2.1); packaging must document and, if redistributable, bundle a compatible Installer or provide a safe manual/Shell route. Do not assume OS 4's Python utility is present.

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

## File-version and compatibility policy (DOL-068)

**Every optional file must be an exact match for the running Dolly release**, and must also pass its own format/API compatibility and integrity checks. Version information cannot be inferred from the name alone, and a sidecar manifest that is not bound to the actual file is insufficient. Release packaging must generate and embed a single authoritative **release/build identifier** in the main executable and in every owned asset, and record the expected per-resource format version. The native loader must verify the actual content (embedded header or trusted manifest with verified file digest) before attempting to load or interpret it.

If an optional file is absent, inaccessible, from another release, from another API/format, or cannot be verified:
1. Do not use it at all; disable only the dependent optional feature.
2. Show the **exact `DOLLY:` path**, the required and detected release/format when available, the disabled feature and practical reinstatement/update steps.
3. Continue running the core CLI and GadTools with built-in functionality; do not display missing-plugin warnings for features the user has not selected.
4. Keep all DMA safety gates enabled when a controller profile database cannot be loaded. Missing data must not trigger guessed Mask/MaxTransfer values.

The C99 module `dolly_resources_scan_optional()` now makes the deterministic compatibility decision; its `DollyResourceInspect` callback is only a **contract for future AmigaDOS file inspection**. Actual cryptographic/integrity verification, version-bearing package generation and native resource loading **are not implemented or hardware-tested yet**.

Example contextual error text:

> **Plik z innego stada — wersja nie pasuje do Dolly!**
>
> Plik: `DOLLY:Plugins/Devices/ClassicIDE.profile` (illustrative)
>
> Wymagana wersja: aktualne wydanie Dolly. Znaleziona: starsze wydanie.
>
> Skutek: brak automatycznych wskazówek konfiguracji tego kontrolera. Podstawowe funkcje Dolly nadal działają; niepotwierdzone DMA pozostaje zablokowane.
>
> Rozwiązanie: zainstaluj pakiet profili przeznaczony dla tego samego wydania Dolly.

The details must be paged or wrapped to fit the mandatory 640×256 Topaz/8 GadTools layout.

## Main executable self-check (no bootstrap executable)

Dolly performs the CRC32/version check of its **own actual executable file** after startup, before enabling disk operations. The expected CRC32 and release ID are **embedded in that executable itself** during post-link stamping. The standard `$VER:` string remains available for AmigaDOS `C:Version`. The Installer packages the program and creates the `DOLLY:` assign; it installs **no manifest or extra verifier program for the main executable**. This detects accidental file damage but cannot protect code before it starts executing. See [Integrity](INTEGRITY.md).
