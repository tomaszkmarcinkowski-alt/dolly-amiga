#include "dolly_gui_minimum.h"
#include <limits.h>
static uint32_t max_u32(uint32_t a,uint32_t b){return a>b?a:b;}
DollyGuiMinimum dolly_gui_measure_minimum(DollyGuiWindowKind kind,
                                         DollyGuiMinMetrics m) {
    DollyGuiMinimum out={0,0,0};
    uint32_t client_w,client_h,w,h;
    if((kind!=DOLLY_GUI_WINDOW_MAIN && kind!=DOLLY_GUI_WINDOW_SHEEP) ||
       m.line_height<6 || m.line_height>64 ||
       m.padding_x>64 || m.padding_y>64 ||
       m.heading_width>8192 ||
       m.main_content_height>8192 ||
       m.main_build_label_width>8192 ||
       m.show_button_text_width>8192 ||
       m.quit_button_text_width>8192 ||
       m.hide_button_text_width>8192 || m.log_button_text_width>8192 ||
       m.border_left>128 || m.border_right>128 ||
       m.border_top>128 || m.border_bottom>128)
        return out;
    if(kind==DOLLY_GUI_WINDOW_MAIN) {
        /* Two GadTools buttons on one row; 12px text padding per button.
         * Main explanatory sentence is optional at very narrow widths.
         */
        /* Both buttons share an equal-width row in the current layout. */
        client_w=max_u32(m.heading_width,
                 2u*max_u32(m.show_button_text_width+12u,
                            m.quit_button_text_width+12u)+8u);
        /* Main shell: source/target BUTTON+TEXT, operation CYCLE,
         * description, two action rows, blocks, bottom status strip.
         * Minimum vertical client area follows the actual last control.
         */
        /* Progress at x=86, 66px well, build label with 6px
         * clearance at each side, 70px Stop/Quit with 6px gap. */
        client_w=max_u32(client_w,86u+66u+12u+
                         m.main_build_label_width+90u*2u+6u);
        /* The caller derives the bottommost extent from actual GUI
         * positions and widget sizes, not a historical fixed height. */
        client_h=max_u32(m.main_content_height,
                         m.line_height+4u);
    } else {
        /* The native caller also checks tile size against the pixel aspect. */
        client_w=max_u32(m.heading_width,24u*10u+8u);
        client_w=max_u32(client_w,80u+6u+80u+6u+
                         max_u32(76u,m.log_button_text_width+12u)+6u+
                         max_u32(76u,m.hide_button_text_width+12u));
        client_w=max_u32(client_w,120u+7u+46u);
        /* Fits current geometry: header, grid, bevel/bar and control row.
         * The grid formula has min cell_h=9 and requires ~140px.
         */
        client_h=max_u32(140u,m.line_height+11u+8u*9u+48u);
    }
    if(client_w>UINT32_MAX-m.padding_x*2u ||
       client_h>UINT32_MAX-m.padding_y*2u)
        return out;
    w=client_w+m.padding_x*2u;
    h=client_h+m.padding_y*2u;
    if(w>UINT32_MAX-m.border_left-m.border_right ||
       h>UINT32_MAX-m.border_top-m.border_bottom)
        return out;
    out.outer_width=w+m.border_left+m.border_right;
    out.outer_height=h+m.border_top+m.border_bottom;
    if(out.outer_width>32767u || out.outer_height>32767u)
        return out;
    out.valid=1;
    return out;
}
