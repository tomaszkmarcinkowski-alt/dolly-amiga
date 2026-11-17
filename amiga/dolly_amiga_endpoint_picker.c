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
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

extern struct Library *AslBase;
extern struct DosLibrary *DOSBase;
#define PICK_MODE 500u
#define PICK_DEVICE 501u
#define PICK_UNIT 502u
#define PICK_PARTITION 503u
#define PICK_SELECT 504u
#define PICK_FILE 506u
#define PICK_CANCEL 507u
#define PICK_LOGICAL 508u
#define PICK_FILE_TEXT 509u
#define PICK_IMAGE_FORMAT 510u
#define PICK_FILE_INFO 511u
#define PICK_SELECTION_TEXT 512u

static STRPTR image_formats[]={
    (STRPTR)"RAW",(STRPTR)"HDF",(STRPTR)"ADF",(STRPTR)"IFF DOLY",NULL
};
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
/* All coordinates are window-relative. At the 606x195 baseline this
 * matches the accepted layout; on larger public screens the LISTVIEWs
 * grow vertically and each row grows horizontally. The bottom actions
 * stay accessible, regardless of how large the chooser becomes.
 */
typedef struct PickerGeometry {
    WORD left,right,full_width;
    WORD mode_x,mode_w;
    WORD list_y,list_h,action_y,preview_y;
    WORD browse_y,path_y,detail_y;
    WORD format_x,format_w;
} PickerGeometry;
static PickerGeometry picker_geometry(const DollyEndpointPicker *p) {
    PickerGeometry g;
    const struct Window *w=p->window;
    LONG left=20;
    LONG right=(LONG)w->Width-(LONG)w->BorderRight-20;
    LONG action=(LONG)w->Height-(LONG)w->BorderBottom-31;
    LONG preview=action-23;
    LONG height=preview-66-7;
    if(right<left+320)right=left+320;
    if(height<56)height=56;
    g.left=(WORD)left;
    g.right=(WORD)right;
    g.full_width=(WORD)(right-left);
    g.mode_x=122;
    g.mode_w=(WORD)(right-122);
    g.list_y=66;
    g.list_h=(WORD)height;
    g.action_y=(WORD)action;
    g.preview_y=(WORD)preview;
    g.browse_y=66;
    g.path_y=105;
    g.detail_y=132;
    g.format_x=159;
    g.format_w=(WORD)(right-159);
    return g;
}
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
    ULONG selected=(ULONG)~0ul;
    if(id==PICK_DEVICE && p->has_device)selected=(ULONG)p->device;
    if(id==PICK_UNIT && p->has_unit)selected=(ULONG)p->unit;
    if(id==PICK_PARTITION && p->has_partition)
        selected=(ULONG)p->partition;
    if(id==PICK_LOGICAL && p->has_logical)selected=(ULONG)p->logical;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=p->screen->Font;
    ng.ng_LeftEdge=x;ng.ng_TopEdge=y;
    ng.ng_Width=w;
    ng.ng_Height=picker_geometry(p).list_h;
    ng.ng_GadgetID=id;ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(LISTVIEW_KIND,last,&ng,
                        GTLV_Labels,(ULONG)items,
                        GTLV_Selected,selected,
                        GA_Disabled,(ULONG)disabled,TAG_DONE);
}
/* Read-only GadTools text field with the same inset border convention as
 * the other Dolly controls. The string lives in the picker until close.
 */
/* Display may truncate, but the committed endpoint keeps the FULL path.
 * Use the actual window font metrics, not an assumed character width.
 */
static void preview_file(DollyEndpointPicker *p) {
    ULONG len;
    struct RastPort *rp;
    if(p->pending_file.kind!=DOLLY_ENDPOINT_FILE) {
        p->file_display[0]='\0';
        return;
    }
    memcpy(p->file_display,p->pending_file.path,
           sizeof(p->file_display));
    if(!p->window)return;
    rp=p->window->RPort;
    len=(ULONG)strlen(p->file_display);
    while(len>3u &&
          (LONG)TextLength(rp,(STRPTR)p->file_display,len)>
          (LONG)picker_geometry(p).full_width-16) {
        p->file_display[--len]='\0';
    }
    if(len<(ULONG)strlen(p->pending_file.path) && len>=3u) {
        p->file_display[len-3u]='.';
        p->file_display[len-2u]='.';
        p->file_display[len-1u]='.';
    }
}
/* Preview is provisional: the parent selection only changes after
 * Use selection. Show the current hierarchy as the user moves through
 * Device -> Unit -> Partition.
 */
static void update_selection_preview(DollyEndpointPicker *p) {
    char full[160];
    ULONG length;
    const char *dev=p->has_device ?
        (const char *)device_names[p->device] : NULL;
    const char *unit=p->has_unit ?
        (const char *)unit_names[p->unit] : NULL;
    const char *part=p->has_partition ?
        (const char *)partition_names[p->partition] : NULL;
    if(p->mode==DOLLY_MODE_LOGICAL) {
        if(p->has_logical)
            snprintf(full,sizeof(full),"%s",
                     (const char *)logical_names[p->logical]);
        else strcpy(full,"Select logical device...");
    } else if(!p->has_device) {
        strcpy(full,"Select device...");
    } else if(!p->has_unit) {
        snprintf(full,sizeof(full),"%s / select unit...",dev);
    } else if(p->mode==DOLLY_MODE_PARTITION && !p->has_partition) {
        snprintf(full,sizeof(full),"%s / %s / select partition...",dev,unit);
    } else if(p->mode==DOLLY_MODE_PARTITION) {
        snprintf(full,sizeof(full),"%s / %s / %s",dev,unit,part);
    } else {
        snprintf(full,sizeof(full),"%s / %s (whole unit)",dev,unit);
    }
    strncpy(p->selection_display,full,sizeof(p->selection_display)-1u);
    p->selection_display[sizeof(p->selection_display)-1u]='\0';
    if(!p->window)return;
    length=(ULONG)strlen(p->selection_display);
    while(length>3u &&
          (LONG)TextLength(p->window->RPort,
               (STRPTR)p->selection_display,length)>
               (LONG)picker_geometry(p).full_width-16)
        p->selection_display[--length]='\0';
    if(length<(ULONG)strlen(full) && length>=3u) {
        p->selection_display[length-3u]='.';
        p->selection_display[length-2u]='.';
        p->selection_display[length-1u]='.';
    }
}
static struct Gadget *selection_text(struct Gadget *last,
                                     DollyEndpointPicker *p) {
    struct NewGadget ng;
    PickerGeometry g=picker_geometry(p);
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=p->screen->Font;
    ng.ng_LeftEdge=g.left;ng.ng_TopEdge=g.preview_y;
    ng.ng_Width=g.full_width;ng.ng_Height=16;
    ng.ng_GadgetID=PICK_SELECTION_TEXT;
    ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(TEXT_KIND,last,&ng,
                        GTTX_Text,(ULONG)p->selection_display,
                        GTTX_Border,TRUE,TAG_DONE);
}
static void refresh_selection_preview(DollyEndpointPicker *p) {
    update_selection_preview(p);
    if(p->selection_text && p->window)
        GT_SetGadgetAttrs(p->selection_text,p->window,NULL,
                          GTTX_Text,(ULONG)p->selection_display,TAG_DONE);
}
static struct Gadget *file_text(struct Gadget *last,DollyEndpointPicker *p) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=p->screen->Font;
    ng.ng_LeftEdge=picker_geometry(p).left;
    ng.ng_TopEdge=picker_geometry(p).path_y;
    ng.ng_Width=picker_geometry(p).full_width;ng.ng_Height=17;
    ng.ng_GadgetID=PICK_FILE_TEXT;
    ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(TEXT_KIND,last,&ng,
                        GTTX_Text,(ULONG)p->file_display,
                        GTTX_Border,TRUE,TAG_DONE);
}
static struct Gadget *details_text(struct Gadget *last,DollyEndpointPicker *p) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=p->screen->Font;
    ng.ng_LeftEdge=picker_geometry(p).left;
    ng.ng_TopEdge=picker_geometry(p).detail_y;
    ng.ng_Width=picker_geometry(p).full_width;ng.ng_Height=16;
    ng.ng_GadgetID=PICK_FILE_INFO;ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(TEXT_KIND,last,&ng,
                        GTTX_Text,(ULONG)p->file_details,
                        GTTX_Border,TRUE,TAG_DONE);
}
static struct Gadget *format_choice(struct Gadget *last,DollyEndpointPicker *p) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=p->screen->Font;
    ng.ng_LeftEdge=picker_geometry(p).format_x;
    ng.ng_TopEdge=130;
    ng.ng_Width=picker_geometry(p).format_w;ng.ng_Height=17;
    ng.ng_GadgetID=PICK_IMAGE_FORMAT;ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(CYCLE_KIND,last,&ng,
                        GTCY_Labels,(ULONG)image_formats,
                        GTCY_Active,(ULONG)p->output_format,TAG_DONE);
}
/* The extension is only a hint; never assert that bytes are valid. */
static int suffix_is(const char *name,const char *ext) {
    size_t n=strlen(name),m=strlen(ext),i;
    if(n<m)return 0;
    for(i=0;i<m;++i) {
        char a=name[n-m+i],b=ext[i];
        if(a>='A' && a<='Z')a=(char)(a+('a'-'A'));
        if(b>='A' && b<='Z')b=(char)(b+('a'-'A'));
        if(a!=b)return 0;
    }
    return 1;
}
static void file_details(DollyEndpointPicker *p) {
    const char *hint="unknown";
    struct FileInfoBlock fib;
    BPTR lock;
    ULONG len;
    if(p->pending_file.kind!=DOLLY_ENDPOINT_FILE) {
        strcpy(p->file_details,p->role ?
               "Output file not created yet." : "No source file selected.");
        return;
    }
    if(p->role)return; /* Destination shows image format Cycle instead. */
    if(suffix_is(p->pending_file.path,".adf"))hint="ADF (name only)";
    else if(suffix_is(p->pending_file.path,".hdf"))hint="HDF (name only)";
    else if(suffix_is(p->pending_file.path,".raw") ||
            suffix_is(p->pending_file.path,".img"))hint="RAW (name only)";
    else if(suffix_is(p->pending_file.path,".doly") ||
            suffix_is(p->pending_file.path,".iff"))hint="IFF? (name only)";
    sprintf(p->file_details,"Size: unavailable | Type: %s",hint);
    if(DOSBase) {
        lock=Lock((STRPTR)p->pending_file.path,ACCESS_READ);
        if(lock) {
            memset(&fib,0,sizeof(fib));
            if(Examine(lock,&fib) && fib.fib_DirEntryType<0 &&
               fib.fib_Size>=0) {
                sprintf(p->file_details,"Size: %lu bytes | Type: %s",
                        (unsigned long)fib.fib_Size,hint);
            }
            UnLock(lock);
        }
    }
    /* Clip only the display string, not the stored filename/path. */
    if(p->window) {
        LONG available=(LONG)picker_geometry(p).full_width-16;
        len=(ULONG)strlen(p->file_details);
        while(len>3u &&
              (LONG)TextLength(p->window->RPort,
                               (STRPTR)p->file_details,len)>available)
            p->file_details[--len]='\0';
    }
}
static struct Gadget *cycle(struct Gadget *last,DollyEndpointPicker *p) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=p->screen->Font;
    ng.ng_LeftEdge=picker_geometry(p).mode_x;
    ng.ng_TopEdge=24;
    ng.ng_Width=picker_geometry(p).mode_w;ng.ng_Height=17;
    ng.ng_GadgetID=PICK_MODE;ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(CYCLE_KIND,last,&ng,
                        GTCY_Labels,(ULONG)mode_names,
                        GTCY_Active,(ULONG)p->mode,TAG_DONE);
}
/* LISTVIEW columns must occupy the whole client width, with the same
 * gutter independent of whether this mode has two or three columns.
 * Geometry follows actual Intuition borders, not absolute screen size.
 */
static void column_geometry(DollyEndpointPicker *p,unsigned count,
                            unsigned index,WORD *left,WORD *width) {
    PickerGeometry g=picker_geometry(p);
    LONG total=(LONG)g.full_width;
    LONG x=(LONG)g.left+(total*(LONG)index)/(LONG)count;
    LONG next=(LONG)g.left+(total*(LONG)(index+1u))/(LONG)count;
    *left=(WORD)x;
    *width=(WORD)(next-x);
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
        WORD x,w;
        unsigned columns=p->mode==DOLLY_MODE_PHYSICAL?2u:3u;
        column_geometry(p,columns,0u,&x,&w);
        Move(rp,x,59);Text(rp,(STRPTR)"Device",6);
        column_geometry(p,columns,1u,&x,&w);
        Move(rp,x,59);Text(rp,(STRPTR)"Unit",4);
        if(columns==3u) {
            column_geometry(p,columns,2u,&x,&w);
            Move(rp,x,59);
            Text(rp,(STRPTR)"Partition (RDB/MBR)",19);
        }
    } else if(p->mode==DOLLY_MODE_LOGICAL) {
        Move(rp,20,59);Text(rp,(STRPTR)"Mounted logical device (demo)",29);
    } else {
        Move(rp,20,99);
        Text(rp,(STRPTR)"Selected file:",14);
        if(p->role) {
            Move(rp,20,143);
            Text(rp,(STRPTR)"Image format:",13);
        }
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
                  (p->mode==DOLLY_MODE_PARTITION && p->has_partition) ||
                  (p->mode==DOLLY_MODE_FILE &&
                   p->pending_file.kind==DOLLY_ENDPOINT_FILE);
        GT_SetGadgetAttrs(p->choose_button,p->window,NULL,
                          GA_Disabled,(ULONG)!ready,TAG_DONE);
    }
}
/* Removing GadTools gadgets does not erase their rendered pixels.
 * Clear only the actual window client area, respecting Intuition chrome,
 * BEFORE attaching the new mode's gadgets. No extra backing bitmap.
 */
static void clear_picker_client(DollyEndpointPicker *p) {
    struct DrawInfo *info;
    struct Window *w=p->window;
    LONG x1,y1,x2,y2;
    if(!w)return;
    x1=(LONG)w->BorderLeft;
    y1=(LONG)w->BorderTop;
    x2=(LONG)w->Width-(LONG)w->BorderRight-1;
    y2=(LONG)w->Height-(LONG)w->BorderBottom-1;
    if(x2<x1 || y2<y1)return;
    info=GetScreenDrawInfo(p->screen);
    SetDrMd(w->RPort,JAM1);
    SetAPen(w->RPort,info?info->dri_Pens[BACKGROUNDPEN]:0u);
    RectFill(w->RPort,x1,y1,x2,y2);
    if(info)FreeScreenDrawInfo(p->screen,info);
}
static int rebuild(DollyEndpointPicker *p) {
    struct Gadget *last;
    PickerGeometry g;
    if(!p->window)return 0;
    g=picker_geometry(p);
    preview_file(p);
    file_details(p);
    if(p->mode!=DOLLY_MODE_FILE)update_selection_preview(p);
    if(p->gadgets) {
        if(!p->gadgets_detached)
            RemoveGList(p->window,p->gadgets,-1);
        FreeGadgets(p->gadgets);
        p->gadgets=NULL;
    }
    /* The previous LISTVIEW_KIND images remain in the RastPort until
     * explicitly cleared. Do this after removing all old gadgets.
     */
    clear_picker_client(p);
    p->device_list=NULL;p->unit_list=NULL;
    p->partition_list=NULL;p->logical_list=NULL;
    p->choose_button=NULL;p->mode_cycle=NULL;p->file_text=NULL;
    p->format_cycle=NULL;p->file_info=NULL;
    p->selection_text=NULL;
    last=CreateContext(&p->gadgets);
    if(!last)return 0;
    last=cycle(last,p);
    if(!last)return 0;
    p->mode_cycle=last;
    if(p->mode==DOLLY_MODE_PHYSICAL || p->mode==DOLLY_MODE_PARTITION) {
        WORD x,w;
        unsigned columns=p->mode==DOLLY_MODE_PHYSICAL?2u:3u;
        column_geometry(p,columns,0u,&x,&w);
        last=list(last,p,PICK_DEVICE,x,g.list_y,w,&p->devices,0);
        if(!last)return 0;
        p->device_list=last;
        column_geometry(p,columns,1u,&x,&w);
        last=list(last,p,PICK_UNIT,x,g.list_y,w,&p->units,!p->has_device);
        if(!last)return 0;
        p->unit_list=last;
        if(columns==3u) {
            column_geometry(p,columns,2u,&x,&w);
            last=list(last,p,PICK_PARTITION,x,g.list_y,w,
                      &p->partitions,!p->has_unit);
            if(!last)return 0;
            p->partition_list=last;
        }
    } else if(p->mode==DOLLY_MODE_LOGICAL) {
        last=list(last,p,PICK_LOGICAL,g.left,g.list_y,
                  g.full_width,&p->logical_devices,0);
        if(!last)return 0;
        p->logical_list=last;
    }
    if(p->mode!=DOLLY_MODE_FILE) {
        last=selection_text(last,p);
        if(!last)return 0;
        p->selection_text=last;
    }
    if(p->mode==DOLLY_MODE_FILE) {
        /* Reuse the otherwise empty LISTVIEW area. Full client width. */
        last=button(last,p,PICK_FILE,g.left,g.browse_y,
                    g.full_width,"Browse file...",0);
        if(!last)return 0;
        last=file_text(last,p);
        if(!last)return 0;
        p->file_text=last;
        if(p->role) {
            last=format_choice(last,p);
            if(!last)return 0;
            p->format_cycle=last;
        } else {
            last=details_text(last,p);
            if(!last)return 0;
            p->file_info=last;
        }
    }
    /* File mode now commits exactly like every other mode. */
    last=button(last,p,PICK_SELECT,g.left,g.action_y,
                172,"Use selection",1);
    if(!last)return 0;
    p->choose_button=last;
    last=button(last,p,PICK_CANCEL,
                (WORD)(g.right-176),g.action_y,176,"Cancel",0);
    if(!last)return 0;
    AddGList(p->window,p->gadgets,(ULONG)-1,-1,NULL);
    p->gadgets_detached=0;
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
    if(AslRequest(fr,NULL) && fr->fr_File) {
        /* ASL may return an empty drawer for a file in the current
         * directory. A relative filename is a valid DOS pathname.
         * Do not dereference fr_Drawer when NULL.
         */
        dn=fr->fr_Drawer ?
           strlen((const char *)fr->fr_Drawer) : 0u;
        fn=strlen((const char *)fr->fr_File);
        slash=dn>0u && fr->fr_Drawer[dn-1u]!=':' &&
              fr->fr_Drawer[dn-1u]!='/';
        if(fn>0u && dn+(size_t)slash+fn<sizeof(path)) {
            if(dn)memcpy(path,fr->fr_Drawer,dn);
            if(slash)path[dn++]='/';
            memcpy(path+dn,fr->fr_File,fn+1u);
            ok=dolly_endpoint_set_file(result,path);
        }
    }
    FreeAslRequest(fr);
    return ok;
}
int dolly_picker_open(DollyEndpointPicker *p,struct Screen *screen,
                      unsigned role,const DollyEndpoint *initial,
                      DollyEndpointMode preferred_mode) {
    LONG top;
    if(!p || !screen || role>1u ||
       (unsigned)preferred_mode>3u)return 0;
    if(p->window)return 1;
    memset(p,0,sizeof(*p));
    p->screen=screen;
    p->role=role;
    if(initial) {
        p->result=*initial;
        if(initial->kind==DOLLY_ENDPOINT_FILE) {
            p->pending_file=*initial;
            memcpy(p->file_display,initial->path,
                   sizeof(p->file_display));
        }
    }
    if(initial && initial->kind==DOLLY_ENDPOINT_FILE &&
       (unsigned)initial->image_format<=3u)
        p->output_format=initial->image_format;
    p->mode=(initial && initial->kind!=DOLLY_ENDPOINT_NONE) ?
            dolly_endpoint_mode(initial) : preferred_mode;
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
          WA_MinWidth,606,WA_MinHeight,195,
          WA_MaxWidth,(ULONG)screen->Width-12ul,
          WA_MaxHeight,(ULONG)screen->Height-(ULONG)top,
          WA_SizeGadget,TRUE,
          WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_CloseGadget,TRUE,
          WA_Activate,TRUE,WA_SmartRefresh,TRUE,
          WA_IDCMP,IDCMP_GADGETUP|IDCMP_CLOSEWINDOW|
                   IDCMP_REFRESHWINDOW|IDCMP_NEWSIZE|IDCMP_SIZEVERIFY,
          TAG_DONE);
    if(!p->window)return 0;
    return rebuild(p);
}
void dolly_picker_close(DollyEndpointPicker *p) {
    if(!p)return;
    if(p->window){CloseWindow(p->window);p->window=NULL;}
    p->gadgets_detached=0;
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
        if(cls==IDCMP_SIZEVERIFY && p->gadgets &&
           !p->gadgets_detached) {
            /* Prevent old-size gadgets from flashing during drag.
             * Intuition must receive the SIZEVERIFY reply promptly.
             */
            RemoveGList(p->window,p->gadgets,-1);
            p->gadgets_detached=1;
        }
        if(cls==IDCMP_REFRESHWINDOW) {
            GT_BeginRefresh(p->window);
            GT_EndRefresh(p->window,TRUE);
        }
        GT_ReplyIMsg(msg);
        if(cls==IDCMP_NEWSIZE) {
            /* Resize completed: discard old pixel positions, build
             * fresh GadTools gadgets at the newly measured geometry.
             */
            if(!rebuild(p))dolly_picker_close(p);
            return;
        }
        if(cls==IDCMP_REFRESHWINDOW && !p->gadgets_detached) {
            /* Redraw after refresh using exactly the same order
             * as mode changes; no stale LISTVIEW pixel remnants.
             */
            clear_picker_client(p);
            RefreshGList(p->gadgets,p->window,NULL,-1);
            GT_RefreshWindow(p->window,NULL);
            captions(p);
        }
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
            refresh_selection_preview(p);
        } else if(id==PICK_UNIT && code<2u && p->has_device) {
            p->unit=code;p->has_unit=1;p->has_partition=0;
            if(p->partition_list)
                GT_SetGadgetAttrs(p->partition_list,p->window,NULL,
                                  GTLV_Selected,(ULONG)~0ul,TAG_DONE);
            change_enabled(p);
            refresh_selection_preview(p);
        } else if(id==PICK_PARTITION && code<2u && p->has_unit) {
            p->partition=code;p->has_partition=1;
            change_enabled(p);
            refresh_selection_preview(p);
        } else if(id==PICK_LOGICAL && code<2u) {
            p->logical=code;p->has_logical=1;
            change_enabled(p);
            refresh_selection_preview(p);
        } else if(id==PICK_SELECT) {
            int ok=0;
            if(p->mode==DOLLY_MODE_PHYSICAL && p->has_unit)
                ok=dolly_endpoint_set_unit(&p->result,p->device,p->unit);
            else if(p->mode==DOLLY_MODE_PARTITION && p->has_partition)
                ok=dolly_endpoint_set_partition(&p->result,p->device,
                                                p->unit,p->partition);
            else if(p->mode==DOLLY_MODE_LOGICAL && p->has_logical)
                ok=dolly_endpoint_set_logical(&p->result,p->logical);
            else if(p->mode==DOLLY_MODE_FILE &&
                    p->pending_file.kind==DOLLY_ENDPOINT_FILE) {
                p->result=p->pending_file;
                if(p->role)p->result.image_format=p->output_format;
                ok=1;
            }
            if(ok){p->changed=1;close=1;}
        } else if(id==PICK_IMAGE_FORMAT && p->role && code<4u) {
            p->output_format=(DollyImageFormat)code;
        } else if(id==PICK_FILE && p->mode==DOLLY_MODE_FILE) {
            DollyEndpoint selected={0};
            if(pick_file(&selected,p->role)) {
                p->pending_file=selected;
                preview_file(p);
                file_details(p);
                if(p->file_info)
                    GT_SetGadgetAttrs(p->file_info,p->window,NULL,
                                      GTTX_Text,(ULONG)p->file_details,
                                      TAG_DONE);
                if(p->file_text)
                    GT_SetGadgetAttrs(p->file_text,p->window,NULL,
                                      GTTX_Text,(ULONG)p->file_display,
                                      TAG_DONE);
                change_enabled(p);
            }
        }
        if(close){dolly_picker_close(p);return;}
    }
}
