# Dolly GUI architecture — MVP GadTools only; future optional backends

**Status 2026-11-17:** architecture and portable layout/notification components are in the repository. There is **no native GadTools window, ReAction/MUI module loader, verified 68000 binary or WinUAE run** yet.

## GUI backends

| Backend | Delivery | Requirement |
|---|---|---|
| GadTools | **Only MVP GUI**, built in | AmigaOS 2.04+, MC68000 |
| ReAction | **Post-MVP optional external module** | Developed and distributed later, only when its classes/libraries are installed |
| MUI | **Post-MVP, later than the core GUI**, independent optional module | Only if installed, never mandatory |

**MVP freeze (2026-11-17):** No ReAction/MUI implementation, runtime loading, compatibility testing or packaging is a condition for the MVP release. Their design contract may be retained to avoid future architectural coupling, but their delivery is an independent **post-MVP** milestone. All screens, dialogs, warnings and Sheep Inventory in MVP use GadTools. The standard executable has no link-time dependency on ReAction or MUI. Resource files and future plug-ins use the installation root `DOLLY:`; see [installation layout](INSTALLATION.md).

The versioned toolkit-neutral contract lives in `include/dolly_gui_backend.h`. It defines a core-owned host interface and a backend table for events/main window/notice/window relayout. **It is a design contract, not an implemented dynamic loader**. We must verify actual AmigaOS shared-library/LoadSeg ABI, compiler calling conventions, symbol lifetime and packaging before describing modules as loadable plugins. Backend failure must show a brief message and revert to built-in GadTools, never fail to launch Dolly.

## Sheep Counter window placement (M1.5)

**Default:** open on the existing public/system screen (Workbench) using
Intuition/GadTools styling and the screen's own pens/font. At launch,
the window is maximized to the usable screen area **below** the
system screen title bar, without covering that bar:

- Left = 0, Top = `Screen.BarHeight`
- Outer Width = `Screen.Width`
- Outer Height = `Screen.Height - Screen.BarHeight`

These are **outer window dimensions**, including Dolly's own title bar,
borders and close gadget. The top system title bar remains visible.
The screen size is queried at runtime; do not hard-code a window size
of 640x256 or force a new screen.

Future optional preference: open Sheep Counter on a separate dedicated
screen, with a return to default public-screen mode. Do not implement
the separate-screen option as a requirement for the current milestone.
Use one model for block progress and a simple ASCII `*` marker until
the final red/white Dolly art and animations are developed.

The M1.5 prototype `dolly-sheep-window` uses a public Intuition window
and direct RastPort drawing, not yet full GadTools gadgets. It has
synthetic progress and **no physical disk reads or writes**. Fixed
20x6 grid rendering is an experimental 640x256 baseline; adaptable
layout/font measurements and refresh handling remain open. It should
be compiled and run on MC68000 / 512 KiB Chip / Kickstart 2.04 before
claiming this UI milestone complete.

## Screen resolution and font policy

Mandatory **640x256, topaz.font/8** baseline for every built-in GadTools window and warning, considering outer dimensions (title, gadgets, borders, buttons). Prefer practical 640x200 compatibility. Existing `dolly_gui_layout` is a *portable geometry preflight*, not a real pixel-precise GadTools renderer.

Font selection priority:

1. Explicitly enabled **Dolly custom font**, if available and its measured content fits.
2. Actual current **system Screen Font**, when the user has not selected a custom font or custom font does not fit.
3. **topaz.font/8 fallback** if the screen or custom font would clip a view; use a compact, paginated layout if needed.

The actual font of a public screen's title/menu is owned by Intuition and **cannot be changed by Dolly**. Use `Screen.Font` / `GetScreenDrawInfo(...)->dri_Font` as appropriate and real glyph measurements (e.g. `TextLength`, `TextExtent`) rather than assuming that a proportional font has fixed-width cells. The portable layout receives conservative measured cell/line metrics plus exact measured title and border dimensions. The renderer must **clip all text**, truncate/scroll/paginate lengthy diagnostics and reserve a permanently reachable button row. If even compact Topaz/8 cannot fit, do **not** open an inaccessible window; provide a safe alternative (CLI or short explanation on a usable screen).

## Live screen/font change notifications — verified API distinctions

### System preferences file changes

AmigaDOS `StartNotify()` / `EndNotify()` is available from **V36 (AmigaOS 2.x)**. Monitor:
- `ENV:sys/screenmode.prefs`
- `ENV:sys/screenfont.prefs`

This alerts Dolly when preferences files change. **It does not guarantee** that Intuition has already reopened Workbench or that the same `Screen*` is valid. `IDCMP_NEWPREFS` alone is insufficient for newer font/screen settings.

Experimental adapter `amiga/dolly_amiga_pref_notify.c` uses a private Exec message port, `NRF_SEND_MESSAGE | NRF_WAIT_REPLY`, coalescing normalized events through `dolly_screen_watch_event()`. It replies to messages and calls EndNotify before releasing requests. StartNotify may fail when a particular prefs file does not exist; the failure leaves normal operation intact.

**The adapter has not been compiled against AmigaOS NDK or exercised in WinUAE.**

### Screen public lifecycle

AmigaOS 3.1.4 does **not** have a confirmed built-in classic screen-open/close notification replacement for `screennotify.library`. Don't equate file change notifications with imminent screen closing.

`screennotify.library` is a **separately installed optional third-party library** used by existing OS 2.x–3.x applications to track public-screen lifecycle events. Dolly should attempt to open it dynamically if installed; no hard dependency. Exact message protocol/function table and safe deregistration need confirmation from the library's SDK/source; **do not invent numeric function offsets**. Integrating it is still pending.

When a public screen is about to close, close or detach Dolly windows promptly and **invalidate any old screen pointer**. After the screen is available, reacquire via `LockPubScreen()`, obtain real border/font metrics and rebuild views. Repeated notifications should be coalesced, and window state/operation progress retained, without recreating windows during partial screen reset.

### Fallback when no ScreenNotify is available

Revalidate current screen geometry on open, activation/resize, changes in Dolly font settings and ordinary preference notifications. For extra safety, a **bounded deferred retry** can attempt to reacquire the public screen on the UI task when a preference change is pending; avoid busy-polling and requester loops. If the screen is unavailable, keep the UI suspended and report status when a usable screen returns. This does *not* guarantee that every external screen close is detected in advance.

## Planned native event loop

1. On startup, subscribe using the available DOS notify adapter and optional ScreenNotify, then record the current screen/font geometry.
2. Wait for Exec/Intuition/notification ports; drain all messages and reply to the appropriate owner.
3. Feed `DollyScreenWatch`; reduce duplicate events.
4. Before touching any screen, lock or otherwise validate it; no stale pointer use.
5. Plan `custom -> Screen Font -> topaz/8` per view with real text/chrome measurements.
6. Rebuild GadTools gadgets if necessary, preserve current task and window state.
7. If no valid view fits, fail closed rather than create an off-screen requester.
8. On shutdown, EndNotify active requests, drain/reply messages, deregister ScreenNotify, then close ports/libraries.

## Verification required

- Unit tests: `tests/test_gui_layout.c` checks all seven view types, 640x256 / 640x200, font preference order, oversized fonts and minimum bounds.
- Unit tests: `tests/test_screen_watch.c` checks coalesced events, screen close/reopen, no rendering while invalid, and missing optional notification library.
- **Not yet verified:** CTest of complete current `main`, actual gadget bounds, real window border/title size, proportional font strings, NDK build, and OS 2.04/3.1/3.1.4/3.2 WinUAE tests.
- Critical acceptance: capture screenshots of every GadTools view and warning in 640x256/topaz8 without clipped text, unreachable buttons or windows exceeding screen bounds.

### References

- [AmigaOS Preferences](https://wiki.amigaos.net/wiki/Preferences)
- [AmigaDOS file notification](https://wiki.amigaos.net/wiki/Notification)
- [Intuition window IDCMP_NEW PREFS constraints](https://wiki.amigaos.net/wiki/Intuition_Input_and_Output_Methods)
- [Intuition Screens font rules](https://wiki.amigaos.net/wiki/Intuition_Screens)
- [ScreenNotify 1.0 (Aminet)](https://aminet.net/package/util/libs/ScreenNotify10)
- [Issues #14](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/14) and [#15](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/15)

## M1.6 — GadTools-backed Sheep Counter, incremental grid (experimental)

The default `dolly-sheep-window` prototype now uses OS 2.x GadTools:
two read-only `NUMBER_KIND` values (blocks processed, errors) and a
`BUTTON_KIND` Cancel control. It retains the maximized public-screen
window **below** `Screen.BarHeight` and obtains system pens/font from
the current public screen. GadTools values are changed with
`GT_SetGadgetAttrs` — never by re-running `GT_RefreshWindow` every tick.

The 20x6 synthetic surface-map grid and simple progress bar remain
direct RastPort drawing: GadTools has no native cell-grid or
read-only progress indicator. Only the old and new active cells
and newly filled progress-bar span are redrawn for each block.
The current cell is marked with ASCII `*` rather than an image.

This prototype is not yet compiled or tested on MC68000, 512 KiB
Chip RAM, Kickstart 2.04. It uses synthetic data and never opens a
physical device or writes to any disk. Window resizing/re-exposure,
font-sensitive layout, real scan integration, and functional Pause
remain future work. The Cancel button stops only synthetic progress.

The first native test should inspect correct GadTools creation,
legibility and reduced flicker on actual WinUAE hardware emulation,
and confirm `WINDOW PASS` and exit code zero.

## M1.7 — resizable public-screen Sheep Counter (unverified)

The previous screenshot showed the "Blocks" gadget's auto-placed label
clipping past the left edge. M1.7 removes the externally positioned
GadTools label and draws "Blocks:" and "Errors:" explicitly **above**
the native number gadgets, within the client area.

The prototype window starts maximized **below the system screen title
bar**, but is now resizable with the standard Intuition size gadget.
It sets a minimum outer size of 420x205. Layout is recalculated from
current window border sizes, available width and height:

- 20 columns x 6 rows; individual grid cells grow/shrink in both axes
- progress bar width follows the grid width
- count gadgets grow/shrink and remain inside the client area
- Cancel button follows the right edge, control row follows bottom edge
- current cell retains the ASCII `*` marker

A resize triggers IDCMP_NEWSIZE, detaches and rebuilds **the complete**
GadTools gadget list (not selective refresh), then redraws the
grid at its new cell dimensions. Normal scan progress still touches
only the previous/new cell, progress-bar delta and number gadget.
Exposed-window refresh uses the GadTools-aware refresh calls.
This is intentionally a source-level test until the Docker m68k build
and WinUAE test confirm it on MC68000 / 512 KiB Chip / Kickstart 2.04.

Known limitations: fixed 20x6 logical grid, no true device I/O or
non-synthetic progress; font/geometry edge cases and repeated resize
memory behavior need testing. When an actual operation is in progress,
state must be preserved across all layout changes.
