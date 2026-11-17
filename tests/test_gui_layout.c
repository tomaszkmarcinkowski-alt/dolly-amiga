#include "dolly_gui_layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static DollyGuiScreen classic(void){
    DollyGuiScreen s={640,256,10,0,4,4,12,4,0,0};
    return s;
}
static DollyGuiFont f(uint32_t w,uint32_t h){
    DollyGuiFont out;out.cell_width=w;out.line_height=h;out.available=1;return out;
}
static void fits(DollyGuiLayout l,DollyGuiScreen s){
    assert(l.fit!=DOLLY_GUI_NO_FIT && l.fit!=DOLLY_GUI_BAD_INPUT);
    assert(l.width && l.height && l.page_count>=1);
    assert(l.x<=s.screen_width && l.width<=s.screen_width-l.x);
    assert(l.y>=s.reserved_top);
    assert(l.y<=s.screen_height-s.reserved_bottom);
    assert(l.height<=s.screen_height-s.reserved_bottom-l.y);
    assert(l.inner_x+l.inner_width<=l.width);
    assert(l.inner_y+l.inner_height<=l.height);
    assert(l.columns>=28 && l.visible_rows>=3);
}
int main(void){
    DollyGuiScreen screen=classic();
    DollyGuiFont topaz=f(8,8),screenfont=f(8,8),big=f(15,17);
    DollyGuiFontSettings settings={0};
    DollyGuiFontPlan result;
    unsigned i;
    /* Every view must be reachable on a standard 640x256 display.
     * Some content is paged, but no window or button is clipped. */
    for(i=0;i<DOLLY_GUI_VIEW_COUNT;i++){
        result=dolly_gui_plan_with_fonts((DollyGuiView)i,screen,
                                       screenfont,settings,topaz);
        fits(result.layout,screen);
        assert(result.selected_font==DOLLY_GUI_FONT_SYSTEM_SCREEN ||
               result.selected_font==DOLLY_GUI_FONT_TOPAZ8);
    }
    result=dolly_gui_plan_with_fonts(DOLLY_GUI_MAIN,screen,screenfont,settings,topaz);
    assert(result.selected_font==DOLLY_GUI_FONT_SYSTEM_SCREEN);
    /* User-selected Dolly font is preferred when it really fits. */
    settings.custom_enabled=1;settings.custom=f(9,9);
    result=dolly_gui_plan_with_fonts(DOLLY_GUI_ABOUT,screen,screenfont,settings,topaz);
    fits(result.layout,screen);
    assert(result.selected_font==DOLLY_GUI_FONT_CUSTOM);
    /* Larger custom font -> system Screen Font, no forced Topaz. */
    settings.custom=big;
    result=dolly_gui_plan_with_fonts(DOLLY_GUI_ABOUT,screen,screenfont,settings,topaz);
    fits(result.layout,screen);
    assert(result.custom_rejected_for_size==1);
    assert(result.selected_font==DOLLY_GUI_FONT_SYSTEM_SCREEN);
    /* If both custom and screen font are too tall, Topaz is forced. */
    result=dolly_gui_plan_with_fonts(DOLLY_GUI_INVENTORY,screen,big,settings,topaz);
    fits(result.layout,screen);
    assert(result.selected_font==DOLLY_GUI_FONT_TOPAZ8);
    assert(result.layout.use_topaz8==1);
    assert(result.layout.needs_paging);
    assert(result.layout.fit==DOLLY_GUI_FIT_COMPACT_TOPAZ8);
    /* Missing custom font falls back to screen instead of crashing. */
    settings.custom.available=0;
    result=dolly_gui_plan_with_fonts(DOLLY_GUI_WARNING,screen,screenfont,settings,topaz);
    assert(result.custom_unavailable && result.selected_font==DOLLY_GUI_FONT_SYSTEM_SCREEN);
    /* Topaz font unavailable must not produce a clipped window. */
    settings.custom_enabled=0;
    topaz.available=0;
    result=dolly_gui_plan_with_fonts(DOLLY_GUI_INVENTORY,screen,big,settings,topaz);
    assert(result.layout.fit==DOLLY_GUI_NO_FIT);
    topaz.available=1;
    /* Smaller maximum window: compact page or fail closed. */
    screen.max_window_width=390;screen.max_window_height=170;
    for(i=0;i<DOLLY_GUI_VIEW_COUNT;i++){
        result=dolly_gui_plan_with_fonts((DollyGuiView)i,screen,
                                        big,settings,topaz);
        fits(result.layout,screen);
        assert(result.selected_font==DOLLY_GUI_FONT_TOPAZ8);
    }
    screen.max_window_width=200;
    result=dolly_gui_plan_with_fonts(DOLLY_GUI_WARNING,screen,screenfont,settings,topaz);
    assert(result.layout.fit==DOLLY_GUI_NO_FIT);
    screen=classic();screen.screen_height=200;
    for(i=0;i<DOLLY_GUI_VIEW_COUNT;i++){
        result=dolly_gui_plan_with_fonts((DollyGuiView)i,screen,
                                         big,settings,topaz);
        fits(result.layout,screen);
    }
    screen=classic();screen.reserved_top=250;
    result=dolly_gui_plan_with_fonts(DOLLY_GUI_MAIN,screen,
                                    screenfont,settings,topaz);
    assert(result.layout.fit==DOLLY_GUI_NO_FIT ||
           result.layout.fit==DOLLY_GUI_BAD_INPUT);
    assert(!strcmp(dolly_gui_font_source_name(DOLLY_GUI_FONT_SYSTEM_SCREEN),
                   "System Screen Font"));
    /* Deterministic geometries include irregular screen areas and borders.
     * Each returned window must fit; otherwise the API must reject it. */
    {
        uint32_t seed=0xD011A600u;
        unsigned n;
        for(n=0;n<5000;n++){
            DollyGuiScreen v={0};
            DollyGuiFont sys;
            DollyGuiFontSettings cfg={0};
            DollyGuiFontPlan planned;
            seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
            v.screen_width=240u+(seed%900u);
            seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
            v.screen_height=110u+(seed%500u);
            v.reserved_top=8u+(seed%45u);
            v.border_left=2u+(seed%10u);
            v.border_right=2u+(seed%10u);
            v.border_top=8u+(seed%25u);
            v.border_bottom=2u+(seed%10u);
            seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
            sys=f(5u+(seed%19u),7u+(seed%19u));
            cfg.custom_enabled=(uint8_t)(seed&1u);
            cfg.custom=f(5u+((seed>>5)%21u),7u+((seed>>7)%21u));
            planned=dolly_gui_plan_with_fonts(
                (DollyGuiView)(seed%DOLLY_GUI_VIEW_COUNT),v,sys,cfg,topaz);
            if(planned.layout.fit==DOLLY_GUI_BAD_INPUT ||
               planned.layout.fit==DOLLY_GUI_NO_FIT)continue;
            fits(planned.layout,v);
        }
    }
    screen=classic();screen.reserved_top=UINT32_MAX;
    screen.reserved_bottom=1;
    result=dolly_gui_plan_with_fonts(DOLLY_GUI_MAIN,screen,
                                     screenfont,settings,topaz);
    assert(result.layout.fit==DOLLY_GUI_BAD_INPUT);
    puts("GadTools layout and font fallback tests passed");
    return 0;
}
