/* Native AmigaOS 2.04 endpoint picker, synthetic device tree.
 * Mode-first workflow: Physical device, Logical device, Partition, File.
 * No OpenDevice(), block reads or writes.
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

extern struct Library *AslBase;
#define PICK_MODE 500u
#define PICK_DEVICE 501u
#define PICK_UNIT 502u
#define PICK_PARTITION 503u
#define PICK_SELECT 504u
#define PICK_FILE 506u
#define PICK_CANCEL 507u
#define PICK_LOGICAL 508u

static STRPTR mode_names[]={
    (STRPTR)"Physical device",
    (STRPTR)"Logical device",
    (STRPTR)"Partition",
    (STRPTR)"File",NULL
};
static STRPTR device_names[]={
    (STRPTR)"scsi.device (demo)",
    (STRPTR)"carddisk.device (demo)"
};
static STRPTR unit_names[]={(STRPTR)"Unit 0",(STRPTR)"Unit 1"};
static STRPTR partition_names[]={
    (STRPTR)"DH0: RDB (demo)",(STRPTR)"DH1: MBR (demo)"
};
static STRPTR logical_names[]={
    (STRPTR)"DH0: mounted (demo)",
    (STRPTR)"DH1: mounted (demo)"
};
static void make_list(struct List *l,struct Node *nodes,STRPTR *names) {
    unsigned i;
    l->lh_Head=(struct Node *)&l->lh_Tail;
    l->lh_Tail=NULL;
    l->lh_TailPred=(struct Node *)&l->lh_Head;
    l->lh_Type=NT_USER;
    l->l_pad=0;
    for(i=0;i<2u;++i) {
        memset(&nodes[i],0,sizeof(nodes[i]));
        nodes[i].ln_Name=(char *)names[i];
        AddTail(l,&nodes[i]);
    }
}
static struct Gadget *button(struct Gadget *last,DollyEndpointPicker *p,
                             UWORD id,WORD x,WORD y,WORD w,
                             const char *name,int disabled) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=p->screen->Font;
    ng.ng_LeftEdge=x;ng.ng_TopEdge=y;
    ng.ng_Width=w;ng.ng_Height=16;
    ng.ng_GadgetID=id;ng.ng_GadgetText=(STRPTR)name;
    return CreateGadget(BUTTON_KIND,last,&ng,
                        GA_Disabled,(ULONG)disabled,TAG_DONE);
}
static struct Gadget *list(struct Gadget *last,DollyEndpointPicker *p,
                           UWORD id,WORD x,WORD y,WORD w,
                           struct List *items,int disabled) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=p->screen->Font;
    ng.ng_LeftEdge=x;ng.ng_TopEdge=y;
    ng.ng_Width=w;ng.ng_Height=83;
    ng.ng_GadgetID=id;ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(LISTVIEW_KIND,last,&ng,
                        GTLV_Labels,(ULONG)items,
                        GTLV_Selected,(ULONG)~0ul,
                        GA_Disabled,(ULONG)disabled,TAG_DONE);
}
static struct Gadget *cycle(struct Gadget *last,DollyEndpointPicker *p) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=p->screen->Font;
    ng.ng_LeftEdge=122;ng.ng_TopEdge=24;
    ng.ng_Width=255;ng.ng_Height=17;
    ng.ng_GadgetID=PICK_MODE;ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(CYCLE_KIND,last,&ng,
                        GTCY_Labels,(ULONG)mode_names,
                        GTCY_Active,(ULONG)p->mode,TAG_DONE);
}
static void captions(DollyEndpointPicker *p) {
    struct RastPort *rp;
    struct DrawInfo *info;
    if(!p->window)return;
    rp=p->window->RPort;
    info=GetScreenDrawInfo(p->screen);
    SetDrMd(rp,JAM1);
    SetAPen(rp,info?info->dri_Pens[TEXTPEN]:1u);
    Move(rp,20,36);
    Text(rp,(STRPTR)"Select type:",12);
    if(p->mode==DOLLY_MODE_PHYSICAL || p->mode==DOLLY_MODE_PARTITION) {
        Move(rp,20,59);Text(rp,(STRPTR)"Device",6);
        Move(rp,223,59);Text(rp,(STRPTR)"Unit",4);
        if(p->mode==DOLLY_MODE_PARTITION) {
            Move(rp,426,59);Text(rp,(STRPTR)"Partition (RDB/MBR)",19);
        }
    } else if(p->mode==DOLLY_MODE_LOGICAL) {
        Move(rp,20,59);Text(rp,(STRPTR)"Mounted logical device (demo)",29);
    } else {
        Move(rp,20,85);
        Text(rp,(STRPTR)"Choose a path with the system file requester.",45);
    }
    if(info)FreeScreenDrawInfo(p->screen,info);
}
static void change_enabled(DollyEndpointPicker *p) {
    if(!p->window)return;
    if(p->unit_list)
        GT_SetGadgetAttrs(p->unit_list,p->window,NULL,
                          GA_Disabled,(ULONG)!p->has_device,TAG_DONE);
    if(p->partition_list)
        GT_SetGadgetAttrs(p->partition_list,p->window,NULL,
                          GA_Disabled,(ULONG)!p->has_unit,TAG_DONE);
    if(p->choose_button) {
        int ready=(p->mode==DOLLY_MODE_PHYSICAL && p->has_unit) ||
                  (p->mode==DOLLY_MODE_LOGICAL && p->has_logical) ||
                  (p->mode==DOLLY_MODE_PARTITION && p->has_partition);
        GT_SetGadgetAttrs(p->choose_button,p->window,NULL,
                          GA_Disabled,(ULONG)!ready,TAG_DONE);
    }
}
static int rebuild(DollyEndpointPicker *p) {
    struct Gadget *last;
    if(!p->window)return 0;
    if(p->gadgets) {
        RemoveGList(p->window,p->gadgets,-1);
        FreeGadgets(p->gadgets);
        p->gadgets=NULL;
    }
    p->device_list=NULL;p->unit_list=NULL;
    p->partition_list=NULL;p->logical_list=NULL;
    p->choose_button=NULL;p->mode_cycle=NULL;
    last=CreateContext(&p->gadgets);
    if(!last)return 0;
    last=cycle(last,p);
    if(!last)return 0;
    p->mode_cycle=last;
    if(p->mode==DOLLY_MODE_PHYSICAL || p->mode==DOLLY_MODE_PARTITION) {
        last=list(last,p,PICK_DEVICE,20,66,186,&p->devices,0);
        if(!last)return 0;
        p->device_list=last;
        last=list(last,p,PICK_UNIT,223,66,186,&p->units,!p->has_device);
        if(!last)return 0;
        p->unit_list=last;
        if(p->mode==DOLLY_MODE_PARTITION) {
            last=list(last,p,PICK_PARTITION,426,66,160,
                      &p->partitions,!p->has_unit);
            if(!last)return 0;
            p->partition_list=last;
        }
    } else if(p->mode==DOLLY_MODE_LOGICAL) {
        last=list(last,p,PICK_LOGICAL,20,66,566,&p->logical_devices,0);
        if(!last)return 0;
        p->logical_list=last;
    }
    if(p->mode==DOLLY_MODE_FILE) {
        last=button(last,p,PICK_FILE,20,160,172,"Browse file...",0);
        if(!last)return 0;
    } else {
        last=button(last,p,PICK_SELECT,20,160,172,"Use selection",1);
        if(!last)return 0;
        p->choose_button=last;
    }
    last=button(last,p,PICK_CANCEL,410,160,176,"Cancel",0);
    if(!last)return 0;
    AddGList(p->window,p->gadgets,(ULONG)-1,-1,NULL);
    RefreshGList(p->gadgets,p->window,NULL,-1);
    GT_RefreshWindow(p->window,NULL);
    change_enabled(p);
    captions(p);
    return 1;
}
static int pick_file(DollyEndpoint *result,unsigned destination) {
    struct FileRequester *fr;
    char path[256];
    size_t dn,fn;
    int ok=0,slash;
    if(!AslBase)return 0;
    fr=(struct FileRequester *)AllocAslRequestTags(
        ASL_FileRequest,ASL_FuncFlags,(ULONG)(destination?FILF_SAVE:0),
        TAG_DONE);
    if(!fr)return 0;
    if(AslRequest(fr,NULL) && fr->fr_Drawer && fr->fr_File) {
        dn=strlen((const char *)fr->fr_Drawer);
        fn=strlen((const char *)fr->fr_File);
        slash=dn>0u && fr->fr_Drawer[dn-1u]!=':' &&
              fr->fr_Drawer[dn-1u]!='/';
        if(dn>0u && fn>0u && dn+(size_t)slash+fn<sizeof(path)) {
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
    LONG top;
    if(!p || !screen || role>1u)return 0;
    if(p->window)return 1;
    memset(p,0,sizeof(*p));
    p->screen=screen;
    p->role=role;
    if(initial)p->result=*initial;
    p->mode=dolly_endpoint_mode(initial);
    make_list(&p->devices,p->device_nodes,device_names);
    make_list(&p->units,p->unit_nodes,unit_names);
    make_list(&p->partitions,p->partition_nodes,partition_names);
    make_list(&p->logical_devices,p->logical_nodes,logical_names);
    p->visual=GetVisualInfo(screen,TAG_DONE);
    if(!p->visual)return 0;
    top=(LONG)screen->BarHeight+1;
    p->window=OpenWindowTags(NULL,
          WA_PubScreen,(ULONG)screen,
          WA_Title,(ULONG)(role?"Dolly - Destination":"Dolly - Source"),
          WA_Left,12,WA_Top,(ULONG)top,
          WA_Width,606,WA_Height,195,
          WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_CloseGadget,TRUE,
          WA_Activate,TRUE,WA_SmartRefresh,TRUE,
          WA_IDCMP,IDCMP_GADGETUP|IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW,
          TAG_DONE);
    if(!p->window)return 0;
    return rebuild(p);
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
        UWORD id=0,code=msg->Code;
        int close=0;
        if(cls==IDCMP_GADGETUP && msg->IAddress)
            id=((struct Gadget *)msg->IAddress)->GadgetID;
        if(cls==IDCMP_REFRESHWINDOW) {
            GT_BeginRefresh(p->window);
            captions(p);
            GT_EndRefresh(p->window,TRUE);
        }
        GT_ReplyIMsg(msg);
        if(cls==IDCMP_CLOSEWINDOW || id==PICK_CANCEL)close=1;
        else if(id==PICK_MODE && code<4u) {
            p->mode=(DollyEndpointMode)code;
            p->has_device=0;p->has_unit=0;
            p->has_partition=0;p->has_logical=0;
            if(!rebuild(p))close=1;
            if(close)dolly_picker_close(p);
            return; /* never use a stale gadget ID after a rebuild */
        } else if(id==PICK_DEVICE && code<2u) {
            p->device=code;p->has_device=1;
            p->has_unit=0;p->has_partition=0;
            GT_SetGadgetAttrs(p->unit_list,p->window,NULL,
                              GTLV_Selected,(ULONG)~0ul,TAG_DONE);
            if(p->partition_list)
                GT_SetGadgetAttrs(p->partition_list,p->window,NULL,
                                  GTLV_Selected,(ULONG)~0ul,TAG_DONE);
            change_enabled(p);
        } else if(id==PICK_UNIT && code<2u && p->has_device) {
            p->unit=code;p->has_unit=1;p->has_partition=0;
            if(p->partition_list)
                GT_SetGadgetAttrs(p->partition_list,p->window,NULL,
                                  GTLV_Selected,(ULONG)~0ul,TAG_DONE);
            change_enabled(p);
        } else if(id==PICK_PARTITION && code<2u && p->has_unit) {
            p->partition=code;p->has_partition=1;
            change_enabled(p);
        } else if(id==PICK_LOGICAL && code<2u) {
            p->logical=code;p->has_logical=1;
            change_enabled(p);
        } else if(id==PICK_SELECT) {
            int ok=0;
            if(p->mode==DOLLY_MODE_PHYSICAL && p->has_unit)
                ok=dolly_endpoint_set_unit(&p->result,p->device,p->unit);
            else if(p->mode==DOLLY_MODE_PARTITION && p->has_partition)
                ok=dolly_endpoint_set_partition(&p->result,p->device,
                                                p->unit,p->partition);
            else if(p->mode==DOLLY_MODE_LOGICAL && p->has_logical)
                ok=dolly_endpoint_set_logical(&p->result,p->logical);
            if(ok){p->changed=1;close=1;}
        } else if(id==PICK_FILE && p->mode==DOLLY_MODE_FILE) {
            if(pick_file(&p->result,p->role)){p->changed=1;close=1;}
        }
        if(close){dolly_picker_close(p);return;}
    }
}
