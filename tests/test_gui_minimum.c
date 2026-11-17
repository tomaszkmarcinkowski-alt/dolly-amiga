#include "dolly_gui_minimum.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    DollyGuiMinMetrics m={0};
    DollyGuiMinimum a,b,c;
    m.border_left=4;m.border_right=4;
    m.border_top=12;m.border_bottom=4;
    m.padding_x=4;m.padding_y=2;
    m.line_height=8;
    m.heading_width=190;
    m.show_button_text_width=104;
    m.quit_button_text_width=80;
    m.hide_button_text_width=32;
    a=dolly_gui_measure_minimum(DOLLY_GUI_WINDOW_MAIN,m);
    b=dolly_gui_measure_minimum(DOLLY_GUI_WINDOW_SHEEP,m);
    assert(a.valid && b.valid);
    /* Equal-width buttons must each fit the longer 104px caption. */
    assert(a.outer_width>=2*(104+12)+6+8+8);
    assert(b.outer_width>=24*10+8+8+8);
    assert(b.outer_height>=140+4+12+4);
    m.heading_width+=100;
    c=dolly_gui_measure_minimum(DOLLY_GUI_WINDOW_MAIN,m);
    assert(c.valid && c.outer_width>a.outer_width);
    m.heading_width=190;
    m.padding_x=2;
    c=dolly_gui_measure_minimum(DOLLY_GUI_WINDOW_SHEEP,m);
    assert(c.valid && c.outer_width<b.outer_width);
    m.line_height=0;
    assert(!dolly_gui_measure_minimum(DOLLY_GUI_WINDOW_MAIN,m).valid);
    puts("GUI content-derived minimum: PASS");
    return 0;
}
