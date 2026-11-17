#ifndef DOLLY_GUI_LAYOUT_H
#define DOLLY_GUI_LAYOUT_H
/* Portable Amiga GadTools layout preflight: C99, no OS/API dependencies.
 * Renderer must measure actual screen/window chrome and font before calling.
 * Coordinates are window outer bounds; no window may exceed screen bounds. */
#include <stdint.h>
typedef enum DollyGuiView {
    DOLLY_GUI_MAIN=0,
    DOLLY_GUI_INVENTORY,
    DOLLY_GUI_SETTINGS,
    DOLLY_GUI_TRANSFER,
    DOLLY_GUI_WARNING,
    DOLLY_GUI_HELP,
    DOLLY_GUI_ABOUT,
    DOLLY_GUI_VIEW_COUNT
} DollyGuiView;
typedef enum DollyGuiFit {
    DOLLY_GUI_FIT_PREFERRED=0,
    DOLLY_GUI_FIT_TOPAZ8,
    DOLLY_GUI_FIT_COMPACT_TOPAZ8,
    DOLLY_GUI_NO_FIT,
    DOLLY_GUI_BAD_INPUT
} DollyGuiFit;
typedef struct DollyGuiScreen {
    uint32_t screen_width,screen_height;
    uint32_t reserved_top,reserved_bottom; /* screen menu/title and margins */
    uint32_t border_left,border_right,border_top,border_bottom;
    uint32_t max_window_width,max_window_height; /* 0 = unrestricted */
} DollyGuiScreen;
typedef struct DollyGuiFont {
    uint32_t cell_width,line_height; /* measured worst-case text cell/pixel line */
    uint8_t available;
} DollyGuiFont;
typedef struct DollyGuiLayout {
    DollyGuiFit fit;
    uint32_t x,y,width,height;     /* complete outer window rectangle */
    uint32_t inner_x,inner_y,inner_width,inner_height;
    uint32_t columns,visible_rows;
    uint32_t page_count;           /* fixed content-row budget, >=1 on success */
    uint8_t use_topaz8,compact,needs_paging;
} DollyGuiLayout;
/* For view content beyond one page: render a bounded, clipped page with
 * Previous/Next/Close always visible; wrap/truncate labels and messages.
 * No window opens when fit is NO_FIT or BAD_INPUT. */
DollyGuiLayout dolly_gui_plan(DollyGuiView view,DollyGuiScreen screen,
                             DollyGuiFont preferred,DollyGuiFont topaz8);
/* Default: use font of the actual screen (Screen.Font/DrawInfo.dri_Font).
 * Explicit custom Dolly font takes precedence only when enabled and available.
 * A non-fitting custom font may fall back to Screen Font, then topaz/8.
 * This function plans gadget/text fonts, NEVER alters a public screen's
 * own title/menu font. Renderer must measure real string widths separately. */
typedef enum DollyGuiFontSource {
    DOLLY_GUI_FONT_SYSTEM_SCREEN=0,
    DOLLY_GUI_FONT_CUSTOM,
    DOLLY_GUI_FONT_TOPAZ8
} DollyGuiFontSource;
typedef struct DollyGuiFontSettings {
    uint8_t custom_enabled;
    DollyGuiFont custom;
} DollyGuiFontSettings;
typedef struct DollyGuiFontPlan {
    DollyGuiLayout layout;
    DollyGuiFontSource selected_font;
    uint8_t custom_unavailable;
    uint8_t custom_rejected_for_size;
    uint8_t screen_rejected_for_size;
} DollyGuiFontPlan;
DollyGuiFontPlan dolly_gui_plan_with_fonts(
    DollyGuiView view,DollyGuiScreen screen,
    DollyGuiFont system_screen_font,
    DollyGuiFontSettings settings,
    DollyGuiFont topaz8);
const char *dolly_gui_font_source_name(DollyGuiFontSource source);

const char *dolly_gui_fit_name(DollyGuiFit fit);
#endif
