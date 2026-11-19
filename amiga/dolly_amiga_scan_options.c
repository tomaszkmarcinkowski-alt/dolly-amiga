#include "dolly_amiga_fonts.h"
#include "dolly_amiga_surface_scan.h"
#include "dolly_scan_range.h"
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <graphics/text.h>
#include <proto/intuition.h>
#include <string.h>

int dolly_amiga_scan_options(DollySurfaceScan *scan,struct Window *parent,
                            const DollyMediaGeometry *geometry) {
    static const char *const labels[]={"First sector","Last sector","Retries"};
    struct Window *window=NULL;
    struct Gadget *list=NULL,*last,*fields[3],*status;
    struct NewGadget ng={0};
    struct Screen *screen;
    APTR visual;
    char defaults[3][21];
    uint64_t lower[]={scan->first_lba,scan->first_lba,0};
    uint64_t upper[]={scan->first_lba+scan->total-1u,
                      scan->first_lba+scan->total-1u,255};
    int done=0,accepted=0;
    unsigned i;
    LONG row,field_x,field_w,content,left,y,hint_width;
    static const char hint[]="LBA or C/H/S (S starts at 1). Retries: 0-255.";
    if(!parent)return 0;
    screen=parent->WScreen;
    struct RastPort measure=screen->RastPort;
    SetFont(&measure,dolly_amiga_font(0));
    row=dolly_amiga_font(0)->tf_YSize+7;
    field_x=0;
    for(i=0;i<3;++i) {
        LONG width=TextLength(&measure,(STRPTR)labels[i],strlen(labels[i]));
        if(width>field_x)field_x=width;
    }
    field_x+=24;
    field_w=TextLength(&measure,(STRPTR)"00000000000000000000",20)+18;
    hint_width=TextLength(&measure,(STRPTR)hint,sizeof(hint)-1)+24;
    content=field_x+field_w+77;
    if(content<hint_width)content=hint_width;
    visual=GetVisualInfo(screen,TAG_DONE);
    if(!visual)return 0;
    dolly_surface_log_decimal(defaults[0],lower[0]);
    dolly_surface_log_decimal(defaults[1],upper[1]);
    dolly_surface_log_decimal(defaults[2],scan->max_retries);
    window=OpenWindowTags(NULL,WA_PubScreen,(ULONG)screen,
         WA_Title,(ULONG)"Surface Scan - Parameters",WA_InnerWidth,(ULONG)content,WA_InnerHeight,(ULONG)(5*row+28),
         WA_Activate,TRUE,WA_CloseGadget,TRUE,WA_DragBar,TRUE,WA_SmartRefresh,TRUE,
         WA_IDCMP,IDCMP_GADGETUP|IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW,TAG_DONE);
    if(!window)goto out;
    last=CreateContext(&list);
    if(!last)goto out;
    dolly_amiga_window_font(window,0);
    ng.ng_VisualInfo=visual;ng.ng_TextAttr=dolly_amiga_text_attr(0);ng.ng_Height=(WORD)row;
    left=window->BorderLeft;
    y=window->BorderTop+6;
    for(i=0;i<3;++i) {
        unsigned button;
        ng.ng_LeftEdge=(WORD)(left+field_x);ng.ng_Width=(WORD)field_w;
        ng.ng_TopEdge=(WORD)(y+(row+3)*i);
        ng.ng_GadgetText=(STRPTR)labels[i];ng.ng_Flags=PLACETEXT_LEFT;
        ng.ng_GadgetID=(UWORD)(i+1u);
        fields[i]=last=CreateGadget(STRING_KIND,last,&ng,
                  GTST_String,(ULONG)defaults[i],GTST_MaxChars,34UL,TAG_DONE);
        if(!last)goto out;
        ng.ng_Flags=0;ng.ng_LeftEdge=(WORD)(left+field_x+field_w+5);ng.ng_Width=28;
        for(button=0;button<2;++button) {
            ng.ng_GadgetID=(UWORD)(20+2*i+button);
            ng.ng_GadgetText=(STRPTR)(button?"+":"-");
            last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);
            if(!last)goto out;
            ng.ng_LeftEdge+=33;
        }
    }
    ng.ng_LeftEdge=(WORD)(left+12);ng.ng_Width=(WORD)(content-24);
    ng.ng_TopEdge=(WORD)(y+3*(row+3)+2);
    ng.ng_GadgetText=NULL;
    ng.ng_GadgetID=0;
    status=last=CreateGadget(TEXT_KIND,last,&ng,
       GTTX_Text,(ULONG)hint,TAG_DONE);
    if(!last)goto out;
    ng.ng_TopEdge+=(WORD)(row+5);
    ng.ng_Width=(WORD)(TextLength(&measure,(STRPTR)"Cancel",6)+20);
    for(i=0;i<2;++i) {
        ng.ng_GadgetID=(UWORD)(10+i);ng.ng_GadgetText=(STRPTR)(i?"Cancel":"Start");
        last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);
        if(!last)goto out;
        ng.ng_LeftEdge+=ng.ng_Width+8;
    }
    AddGList(window,list,(ULONG)-1,-1,NULL);
    RefreshGList(list,window,NULL,-1);
    GT_RefreshWindow(window,NULL);
    while(!done) {
        struct IntuiMessage *msg;
        WaitPort(window->UserPort);
        while(!done && (msg=GT_GetIMsg(window->UserPort))!=NULL) {
            ULONG cls=msg->Class;
            UWORD id=cls==IDCMP_GADGETUP && msg->IAddress?
                    ((struct Gadget *)msg->IAddress)->GadgetID:0;
            if(cls==IDCMP_REFRESHWINDOW) {
                GT_BeginRefresh(window);GT_EndRefresh(window,TRUE);
            }
            GT_ReplyIMsg(msg);
            if(cls==IDCMP_CLOSEWINDOW || id==11)done=1;
            else if(id==10 || (id>=20 && id<=25)) {
                uint64_t values[3];
                unsigned first=id==10?0u:(id-20u)/2u;
                unsigned count=id==10?3u:1u;
                int valid=1;
                for(i=first;i<first+count;++i) {
                    const struct StringInfo *info=(const struct StringInfo *)fields[i]->SpecialInfo;
                    static const DollyMediaGeometry decimal_only={0};
                    if(!dolly_scan_position((const char *)info->Buffer,
                                             i==2?&decimal_only:geometry,&values[i]) ||
                       values[i]<lower[i] || values[i]>upper[i]) {
                        valid=0;
                        break;
                    }
                }
                if(valid && id==10 && values[0]<=values[1]) {
                    scan->first_lba=values[0];
                    scan->total=values[1]-values[0]+1u;
                    scan->max_retries=(unsigned)values[2];
                    done=accepted=1;
                } else if(valid && id!=10) {
                    char number[21];
                    uint64_t value=values[first];
                    if(id&1u) { if(value<upper[first])++value; }
                    else if(value>lower[first])--value;
                    dolly_surface_log_decimal(number,value);
                    GT_SetGadgetAttrs(fields[first],window,NULL,GTST_String,(ULONG)number,TAG_DONE);
                } else GT_SetGadgetAttrs(status,window,NULL,
                         GTTX_Text,(ULONG)"Check extent, sector order and retry count.",TAG_DONE);
            }
        }
    }
out:
    if(window)CloseWindow(window);
    if(list)FreeGadgets(list);
    FreeVisualInfo(visual);
    return accepted;
}
