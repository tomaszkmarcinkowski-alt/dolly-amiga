/* AmigaOS 2.04: two independent hierarchical GadTools endpoint dialogs.
 * All devices/units/partitions are SYNTHETIC. No device I/O.
 * Choosing a file only selects a pathname via asl.library requester.
 */
#include "dolly_amiga_endpoint_picker.h"
#include <exec/types.h>
#include <exec/libraries.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/gadtools.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/asl.h>
#include <graphics/rastport.h>
#include <string.h>
#include <stdio.h>

extern struct Library *AslBase;
#define PICK_DEVICE 501u
#define PICK_UNIT 502u
#define PICK_PARTITION 503u
#define PICK_WHOLE 504u
#define PICK_USE_PART 505u
#define PICK_FILE 506u
#define PICK_CANCEL 507u

static STRPTR device_names[2]={(STRPTR)"scsi.device (demo)",
                               (STRPTR)"carddisk.device (demo)"};
static STRPTR unit_names[2]={(STRPTR)"Unit 0", (STRPTR)"Unit 1"};
static STRPTR partition_names[2]={(STRPTR)"DH0: (demo)",
                                  (STRPTR)"DH1: (demo)"};

static void make_list(struct List *l,struct Node *nodes,STRPTR *names) {
    unsigned i;
    NewList(l);
    for(i=0;i<2u;++i) {
        memset(&nodes[i],0,sizeof(nodes[i]));
        nodes[i].ln_Name=names[i];
        AddTail(l,&nodes[i]);
    }
}
static struct Gadget *create_button(struct Gadget *prev,APTR visual,
                                     struct TextAttr *font,UWORD id,
                                     WORD x,WORD y,WORD width,
                                     const char *name) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=visual;
    ng.ng_TextAttr=font;
    ng.ng_LeftEdge=x;
    ng.ng_TopEdge=y;
    ng.ng_Width=width;
    ng.ng_Height=16;
    ng.ng_GadgetID=id;
    ng.ng_GadgetText=(STRPTR)name;
    return CreateGadget(BUTTON_KIND,prev,&ng,TAG_DONE);
}
static struct Gadget *create_list(struct Gadget *prev,APTR visual,
                                   struct TextAttr *font,UWORD id,
                                   WORD x,WORD y,WORD width,
                                   struct List *items,int disabled) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=visual;
    ng.ng_TextAttr=font;
    ng.ng_LeftEdge=x;
    ng.ng_TopEdge=y;
    ng.ng_Width=width;
    ng.ng_Height=88;
    ng.ng_GadgetID=id;
    ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(LISTVIEW_KIND,prev,&ng,
                        GTLV_Labels,(ULONG)items,
                        GTLV_Selected,(ULONG)~0u,
                        GA_Disabled,(ULONG)disabled,
                        TAG_DONE);
}
static void picker_caption(DollyEndpointPicker *p) {
    struct RastPort *rp;
    struct DrawInfo *info;
    UWORD ink;
    if(!p->window)return;
    rp=p->window->RPort;
    info=GetScreenDrawInfo(p->screen);
    ink=info?info->dri_Pens[TEXTPEN]:1u;
    SetDrMd(rp,JAM1);
    SetAPen(rp,ink);
    Move(rp,20,34);Text(rp,(STRPTR)"Device",6);
    Move(rp,222,34);Text(rp,(STRPTR)"Unit",4);
    Move(rp,424,34);Text(rp,(STRPTR)"Partition",9);
    if(info)FreeScreenDrawInfo(p->screen,info);
}
static void change_enabled(DollyEndpointPicker *p) {
    if(p->window && p->unit_list)
        GT_SetGadgetAttrs(p->unit_list,p->window,NULL,
                          GA_Disabled,(ULONG)!p->has_device,TAG_DONE);
    if(p->window && p->partition_list)
        GT_SetGadgetAttrs(p->partition_list,p->window,NULL,
                          GA_Disabled,(ULONG)!p->has_unit,TAG_DONE);
}
static int pick_file(DollyEndpoint *result) {
    struct FileRequester *fr;
    char path[256];
    size_t dn,fn;
    int ok=0,slash;
    if(!AslBase)return 0;
    fr=(struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,TAG_DONE);
    if(!fr)return 0;
    if(AslRequest(fr,NULL) && fr->fr_Drawer && fr->fr_File) {
        dn=strlen(fr->fr_Drawer);
        fn=strlen(fr->fr_File);
        slash=dn>0u && fr->fr_Drawer[dn-1u]!=':' &&
              fr->fr_Drawer[dn-1u]!='/';
        if(dn>0u && fn>0u &&
           dn+(size_t)slash+fn<sizeof(path)) {
            memcpy(path,fr->fr_Drawer,dn);
            if(slash)path[dn++]='/';
            memcpy(path+dn,fr->fr_File,fn+1u);
            ok=dolly_endpoint_set_file(result,path);
        }
    }
    FreeAslRequest(fr);
    return ok;
}
int dolly_picker_open(DollyEndpointPicker *p,struct Screen *screen,
                      unsigned role,const DollyEndpoint *initial) {
    struct Gadget *last;
    LONG top;
    if(!p || !screen || role>1u)return 0;
    if(p->window)return 1;
    memset(p,0,sizeof(*p));
    p->screen=screen;
    p->role=role;
    if(initial)p->result=*initial;
    make_list(&p->devices,p->device_nodes,device_names);
    make_list(&p->units,p->unit_nodes,unit_names);
    make_list(&p->partitions,p->partition_nodes,partition_names);
    p->visual=GetVisualInfo(screen,TAG_DONE);
    if(!p->visual)return 0;
    last=CreateContext(&p->gadgets);
    if(!last)return 0;
    last=create_list(last,p->visual,screen->Font,PICK_DEVICE,
                     20,42,186,&p->devices,0);
    if(!last)return 0;
    p->device_list=last;
    last=create_list(last,p->visual,screen->Font,PICK_UNIT,
                     222,42,186,&p->units,1);
    if(!last)return 0;
    p->unit_list=last;
    last=create_list(last,p->visual,screen->Font,PICK_PARTITION,
                     424,42,156,&p->partitions,1);
    if(!last)return 0;
    p->partition_list=last;
    last=create_button(last,p->visual,screen->Font,PICK_WHOLE,
                       20,150,115,"Whole unit");
    if(!last)return 0;
    last=create_button(last,p->visual,screen->Font,PICK_USE_PART,
                       141,150,115,"Partition");
    if(!last)return 0;
    last=create_button(last,p->visual,screen->Font,PICK_FILE,
                       262,150,115,"File...");
    if(!last)return 0;
    last=create_button(last,p->visual,screen->Font,PICK_CANCEL,
                       383,150,115,"Cancel");
    if(!last)return 0;
    top=(LONG)screen->BarHeight+1;
    p->window=OpenWindowTags(NULL,
          WA_PubScreen,(ULONG)screen,
          WA_Title,(ULONG)(role?"Dolly - Destination":"Dolly - Source"),
          WA_Left,12,WA_Top,(ULONG)top,
          WA_Width,606,WA_Height,190,
          WA_Gadgets,(ULONG)p->gadgets,
          WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_CloseGadget,TRUE,
          WA_Activate,TRUE,WA_SmartRefresh,TRUE,
          WA_IDCMP,IDCMP_GADGETUP|IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW,
          TAG_DONE);
    if(!p->window)return 0;
    GT_RefreshWindow(p->window,NULL);
    picker_caption(p);
    return 1;
}
void dolly_picker_close(DollyEndpointPicker *p) {
    if(!p)return;
    if(p->window){CloseWindow(p->window);p->window=NULL;}
    if(p->gadgets){FreeGadgets(p->gadgets);p->gadgets=NULL;}
    if(p->visual){FreeVisualInfo(p->visual);p->visual=NULL;}
}
void dolly_picker_pump(DollyEndpointPicker *p) {
    struct IntuiMessage *msg;
    if(!p || !p->window)return;
    while((msg=GT_GetIMsg(p->window->UserPort))!=NULL) {
        ULONG cls=msg->Class;
        UWORD id=0;
        UWORD code=msg->Code;
        int close=0;
        if(cls==IDCMP_GADGETUP && msg->IAddress)
            id=((struct Gadget *)msg->IAddress)->GadgetID;
        if(cls==IDCMP_REFRESHWINDOW) {
            GT_BeginRefresh(p->window);
            picker_caption(p);
            GT_EndRefresh(p->window,TRUE);
        }
        GT_ReplyIMsg(msg);
        if(cls==IDCMP_CLOSEWINDOW || id==PICK_CANCEL)close=1;
        else if(id==PICK_DEVICE && code<2u) {
            p->device=code;p->has_device=1;
            p->has_unit=0;p->has_partition=0;
            GT_SetGadgetAttrs(p->unit_list,p->window,NULL,
                              GTLV_Selected,(ULONG)~0u,TAG_DONE);
            GT_SetGadgetAttrs(p->partition_list,p->window,NULL,
                              GTLV_Selected,(ULONG)~0u,TAG_DONE);
            change_enabled(p);
        } else if(id==PICK_UNIT && code<2u && p->has_device) {
            p->unit=code;p->has_unit=1;p->has_partition=0;
            GT_SetGadgetAttrs(p->partition_list,p->window,NULL,
                              GTLV_Selected,(ULONG)~0u,TAG_DONE);
            change_enabled(p);
        } else if(id==PICK_PARTITION && code<2u && p->has_unit) {
            p->partition=code;p->has_partition=1;
        } else if(id==PICK_WHOLE && p->has_unit) {
            if(dolly_endpoint_set_unit(&p->result,p->device,p->unit)) {
                p->changed=1;close=1;
            }
        } else if(id==PICK_USE_PART && p->has_partition) {
            if(dolly_endpoint_set_partition(&p->result,p->device,
                                            p->unit,p->partition)) {
                p->changed=1;close=1;
            }
        } else if(id==PICK_FILE) {
            if(pick_file(&p->result)){p->changed=1;close=1;}
        }
        if(close){dolly_picker_close(p);return;}
    }
}
