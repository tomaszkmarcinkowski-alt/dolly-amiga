#include "dolly_gui_layout.h"
#include <stddef.h>
/* Rows are a bounded *page*, not an unlimited list. Long inventories,
 * advice and logs are rendered on successive clipped pages. */
typedef struct ViewSpec {
    uint16_t columns,rows;
} ViewSpec;
static const ViewSpec views[DOLLY_GUI_VIEW_COUNT]={
    {68,14}, /* device/operation selector */
    {70,18}, /* Sheep Inventory: paginated rows */
    {64,13}, /* settings with short labelled controls */
    {68,10}, /* progress/status, not full-screen modal */
    {60,8},  /* warning: brief cause and Details page(s) */
    {68,16}, /* help, wrapped and paginated */
    {52,8}   /* about */
};
static uint32_t smaller(uint32_t a,uint32_t b){return a<b?a:b;}
static int good_font(DollyGuiFont f){
    return f.available && f.cell_width>=4 && f.cell_width<=64 &&
           f.line_height>=6 && f.line_height<=64;
}
static int measure(DollyGuiView view,DollyGuiScreen s,DollyGuiFont font,
                   uint32_t available_w,uint32_t available_h,
                   uint32_t *wanted_w,uint32_t *wanted_h,
                   uint32_t *frame_w,uint32_t *frame_h){
    uint32_t cw=views[view].columns;
    uint32_t rows=views[view].rows;
    (void)available_w;(void)available_h;
    *frame_w=s.border_left+s.border_right+24u;
    *frame_h=s.border_top+s.border_bottom+60u;
    *wanted_w=cw*font.cell_width+*frame_w;
    *wanted_h=rows*(font.line_height+2u)+*frame_h;
    return *wanted_w>=*frame_w && *wanted_h>=*frame_h;
}
DollyGuiLayout dolly_gui_plan(DollyGuiView view,DollyGuiScreen s,
                             DollyGuiFont preferred,DollyGuiFont topaz8){
    DollyGuiLayout r={0};
    uint32_t avail_w,avail_h,wanted_w=0,wanted_h=0,frame_w=0,frame_h=0;
    uint32_t width,height,cols,rows,max_rows;
    DollyGuiFont chosen;
    int compact=0;
    r.fit=DOLLY_GUI_BAD_INPUT;
    if((unsigned)view>=DOLLY_GUI_VIEW_COUNT ||
       !s.screen_width||!s.screen_height||
       s.screen_width>8192||s.screen_height>8192||
       s.reserved_top+s.reserved_bottom>=s.screen_height||
       s.border_left>128||s.border_right>128||
       s.border_top>128||s.border_bottom>128 ||
       (!good_font(preferred)&&!good_font(topaz8)))
        return r;
    avail_w=s.screen_width;
    avail_h=s.screen_height-s.reserved_top-s.reserved_bottom;
    if(s.max_window_width)avail_w=smaller(avail_w,s.max_window_width);
    if(s.max_window_height)avail_h=smaller(avail_h,s.max_window_height);
    if(good_font(preferred) &&
       measure(view,s,preferred,avail_w,avail_h,
               &wanted_w,&wanted_h,&frame_w,&frame_h) &&
       wanted_w<=avail_w && wanted_h<=avail_h){
        chosen=preferred;
        r.fit=DOLLY_GUI_FIT_PREFERRED;
    }else if(good_font(topaz8)){
        chosen=topaz8;
        measure(view,s,chosen,avail_w,avail_h,
                &wanted_w,&wanted_h,&frame_w,&frame_h);
        if(wanted_w<=avail_w && wanted_h<=avail_h)
            r.fit=DOLLY_GUI_FIT_TOPAZ8;
        else {
            compact=1;
            r.fit=DOLLY_GUI_FIT_COMPACT_TOPAZ8;
        }
        r.use_topaz8=1;
    }else{
        r.fit=DOLLY_GUI_NO_FIT;
        return r;
    }
    if(avail_w<=frame_w || avail_h<=frame_h){
        r.fit=DOLLY_GUI_NO_FIT;return r;
    }
    width=smaller(avail_w,wanted_w);
    height=smaller(avail_h,wanted_h);
    cols=(width-frame_w)/chosen.cell_width;
    max_rows=(height-frame_h)/(chosen.line_height+2u);
    rows=smaller(max_rows,(uint32_t)views[view].rows);
    /* Three clipped rows plus a reachable button strip, not an unreadable
     * fragment of a dialog. No popup is opened if this is impossible. */
    if(cols<28 || rows<3 || width<240){
        r.fit=DOLLY_GUI_NO_FIT;return r;
    }
    if(rows<(uint32_t)views[view].rows ||
       cols<(uint32_t)views[view].columns)compact=1;
    r.compact=(uint8_t)compact;
    r.needs_paging=(uint8_t)(rows<(uint32_t)views[view].rows ||
                             cols<(uint32_t)views[view].columns);
    if(compact && r.fit==DOLLY_GUI_FIT_TOPAZ8)
        r.fit=DOLLY_GUI_FIT_COMPACT_TOPAZ8;
    r.page_count=((uint32_t)views[view].rows+rows-1u)/rows;
    r.width=width;
    r.height=height;
    r.x=(s.screen_width-width)/2u;
    r.y=s.reserved_top+(s.screen_height-s.reserved_top-s.reserved_bottom-height)/2u;
    r.inner_x=s.border_left;
    r.inner_y=s.border_top;
    r.inner_width=width-s.border_left-s.border_right;
    r.inner_height=height-s.border_top-s.border_bottom;
    r.columns=cols;
    r.visible_rows=rows;
    return r;
}
const char *dolly_gui_fit_name(DollyGuiFit f){
    switch(f){
    case DOLLY_GUI_FIT_PREFERRED:return "Preferred font fits";
    case DOLLY_GUI_FIT_TOPAZ8:return "Topaz/8 fallback";
    case DOLLY_GUI_FIT_COMPACT_TOPAZ8:return "Compact Topaz/8 paged fallback";
    case DOLLY_GUI_NO_FIT:return "Cannot fit safely; keep window closed";
    default:return "Invalid screen/font geometry";
    }
}
