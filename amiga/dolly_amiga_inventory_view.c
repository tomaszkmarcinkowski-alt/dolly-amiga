/* Sheep Inventory M1.45: native read-only device snapshot.
 * Only the selected device/unit from the established picker is queried.
 * Registered AmigaDOS entries do not guarantee usable media.
 */
#include "dolly_amiga_inventory_view.h"
#include "dolly_amiga_device_query.h"
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <stdio.h>
#include <string.h>

#define INV_CLOSE_ID 601u
#define INV_PART_ID 602u
#define INV_MAX_PARTITIONS DOLLY_PART_MAX

static void init_rows(DollyNativeInventoryView *v) {
    v->rows.lh_Head=(struct Node *)&v->rows.lh_Tail;
    v->rows.lh_Tail=NULL;
    v->rows.lh_TailPred=(struct Node *)&v->rows.lh_Head;
    v->rows.lh_Type=NT_USER;
    v->rows.l_pad=0;
    v->count=0;
}
/* Explicit bounded copies avoid strncpy truncation diagnostics with
 * -Werror while still providing guaranteed NUL termination.
 */
static void copy_text(char *dst,unsigned size,const char *s) {
    unsigned i=0;
    if(!size)return;
    while(i+1u<size && s[i]) {
        dst[i]=s[i];
        ++i;
    }
    dst[i]='\0';
}
static void add_row(DollyNativeInventoryView *v,const char *description) {
    struct Node *n;
    unsigned i=v->count;
    if(i>=INV_MAX_PARTITIONS)return;
    copy_text(v->row_text[i],sizeof(v->row_text[i]),description);
    n=&v->nodes[i];
    memset(n,0,sizeof(*n));
    n->ln_Name=v->row_text[i];
    AddTail(&v->rows,n);
    v->count++;
}
/* Endpoint.path is constructed by our picker as "driver / Unit N".
 * The endpoint's byte-sized unit field is only an index, NOT the
 * actual controller unit number; never pass it to OpenDevice.
 */
static int parse_unit(const DollyEndpoint *source,char *driver,
                      unsigned capacity,ULONG *unit) {
    const char *marker,*digits,*p;
    ULONG number=0;
    size_t n;
    if(!source || !driver || !unit ||
       (source->kind!=DOLLY_ENDPOINT_UNIT &&
        source->kind!=DOLLY_ENDPOINT_PARTITION))return 0;
    marker=strstr(source->path," / Unit ");
    if(!marker)return 0;
    n=(size_t)(marker-source->path);
    if(!n || n>=capacity || n<7u)return 0;
    /* Verify the driver suffix, no implicit device-name guesses. */
    if(strncmp(marker-7,".device",7)!=0)return 0;
    memcpy(driver,source->path,n);
    driver[n]='\0';
    digits=marker+8;
    if(*digits<'0' || *digits>'9')return 0;
    p=digits;
    while(*p>='0' && *p<='9') {
        ULONG digit=(ULONG)(*p-'0');
        if(number>(0xfffffffful-digit)/10ul)return 0;
        number=number*10ul+digit;
        ++p;
    }
    if(*p!='\0' && strncmp(p," / ",3)!=0)return 0;
    *unit=number;
    return 1;
}
static struct Gadget *create_button(struct Gadget *previous,
                                    DollyNativeInventoryView *v,
                                    struct Screen *screen,LONG x,LONG y,
                                    LONG width) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=v->visual;
    ng.ng_TextAttr=screen->Font;
    ng.ng_LeftEdge=(WORD)x;ng.ng_TopEdge=(WORD)y;
    ng.ng_Width=(WORD)width;ng.ng_Height=16;
    ng.ng_GadgetID=INV_CLOSE_ID;
    ng.ng_GadgetText=(STRPTR)"Close";
    return CreateGadget(BUTTON_KIND,previous,&ng,TAG_DONE);
}
static struct Gadget *create_list(struct Gadget *previous,
                                  DollyNativeInventoryView *v,
                                  struct Screen *screen,
                                  LONG x,LONG y,LONG w,LONG h) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=v->visual;
    ng.ng_TextAttr=screen->Font;
    ng.ng_LeftEdge=(WORD)x;ng.ng_TopEdge=(WORD)y;
    ng.ng_Width=(WORD)w;ng.ng_Height=(WORD)h;
    ng.ng_GadgetID=INV_PART_ID;
    ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(LISTVIEW_KIND,previous,&ng,
                        GTLV_Labels,(ULONG)&v->rows,
                        GTLV_ShowSelected,(ULONG)NULL,
                        GTLV_Selected,(ULONG)~0ul,
                        GA_Disabled,TRUE,
                        TAG_DONE);
}
static void draw_text(struct Window *w,struct DrawInfo *info,
                      LONG x,LONG y,const char *s) {
    struct RastPort *rp=w->RPort;
    SetDrMd(rp,JAM1);
    SetAPen(rp,info->dri_Pens[TEXTPEN]);
    Move(rp,x,y);
    Text(rp,(STRPTR)s,(ULONG)strlen(s));
}
static void paint(DollyNativeInventoryView *v,struct Screen *screen) {
    struct DrawInfo *info;
    struct Window *w=v->window;
    LONG left,right,top,bottom;
    struct RastPort *rp;
    if(!w)return;
    info=GetScreenDrawInfo(screen);
    if(!info)return;
    rp=w->RPort;
    left=(LONG)w->BorderLeft;
    right=(LONG)w->Width-(LONG)w->BorderRight-1;
    top=(LONG)w->BorderTop;
    bottom=(LONG)w->Height-(LONG)w->BorderBottom-1;
    SetDrMd(rp,JAM1);
    SetAPen(rp,info->dri_Pens[BACKGROUNDPEN]);
    RectFill(rp,left,top,right,bottom);
    draw_text(w,info,20,top+12,"Device:");
    draw_text(w,info,94,top+12,v->device_info);
    draw_text(w,info,20,top+27,"Capacity:");
    draw_text(w,info,94,top+27,v->disk_size);
    draw_text(w,info,244,top+27,v->geometry_info);
    draw_text(w,info,20,top+42,"Partitions (read-only snapshot):");
    if(!v->count)
        draw_text(w,info,24,top+60,"No partitions to display");
    draw_text(w,info,20,bottom-29,v->status);
    RefreshGList(v->gadgets,w,NULL,-1);
    GT_RefreshWindow(w,NULL);
    FreeScreenDrawInfo(screen,info);
}
void dolly_inventory_view_close(DollyNativeInventoryView *v) {
    if(!v)return;
    if(v->window){CloseWindow(v->window);v->window=NULL;}
    if(v->gadgets){FreeGadgets(v->gadgets);v->gadgets=NULL;}
    if(v->visual){FreeVisualInfo(v->visual);v->visual=NULL;}
}
int dolly_inventory_view_open(DollyNativeInventoryView *v,
                               struct Screen *screen,
                               const DollyEndpoint *source) {
    char driver[64];
    ULONG unit=0;
    LONG top=(LONG)screen->BarHeight+1;
    struct Gadget *last;
    unsigned i;
    if(!v || !screen || !source)return 0;
    if(v->window)return 1;
    memset(v,0,sizeof(*v));
    init_rows(v);
    copy_text(v->device_info,sizeof(v->device_info),"Unavailable");
    copy_text(v->disk_size,sizeof(v->disk_size),"Unknown");
    copy_text(v->geometry_info,sizeof(v->geometry_info),"Not queried");
    if(parse_unit(source,driver,sizeof(driver),&unit)) {
        char device_name[112],size[40],line[88];
        int queried=dolly_amiga_device_query(driver,unit,
                           device_name,sizeof(device_name),
                           size,sizeof(size),&v->geometry);
        if(queried && v->geometry.capacity_valid) {
            v->verified=1;
            copy_text(v->device_info,sizeof(v->device_info),device_name);
            copy_text(v->disk_size,sizeof(v->disk_size),size);
            sprintf(v->geometry_info,"%lu B/sector (%s)",
                    (unsigned long)v->geometry.block_bytes,
                    v->geometry.capacity_source==DOLLY_GEOMETRY_TRACKDISK?
                    "TD geometry":"SCSI capacity");
            if(v->geometry.block_bytes==512ul &&
               dolly_amiga_partition_scan(driver,unit,
                                          &v->geometry,&v->map)) {
                for(i=0;i<v->map.count && i<DOLLY_PART_MAX;++i) {
                    const DollyNativePartition *p=&v->map.parts[i];
                    sprintf(line,"%u: %s",i+1u,p->label);
                    add_row(v,line);
                }
                {
                    char suffix[40];
                    unsigned used;
                    copy_text(v->status,sizeof(v->status),v->map.status);
                    used=(unsigned)strlen(v->status);
                    /* Short numeric suffix; bounded append of both parts. */
                    sprintf(suffix,": %u partition(s) declared",v->map.count);
                    copy_text(v->status+used,
                              (unsigned)sizeof(v->status)-used,suffix);
                }
            } else {
                if(v->map.status[0])
                    copy_text(v->status,sizeof(v->status),v->map.status);
                else
                    copy_text(v->status,sizeof(v->status),
                              "Partition scan unavailable");
            }
        } else {
            copy_text(v->device_info,sizeof(v->device_info),driver);
            copy_text(v->status,sizeof(v->status),
                      "Query failed: media capacity not verified");
        }
    } else {
        copy_text(v->device_info,sizeof(v->device_info),
                  source->path[0]?source->path:"No source");
        copy_text(v->status,sizeof(v->status),
                  "Inventory of this source type is not yet supported");
    }
    /* A zero-partition result is NOT a LISTVIEW with placeholder
     * nodes. Display that state as plain text instead. */
    v->visual=GetVisualInfo(screen,TAG_DONE);
    if(!v->visual)return 0;
    v->window=OpenWindowTags(NULL,
                  WA_PubScreen,(ULONG)screen,
                  WA_Title,(ULONG)"Dolly - Sheep Inventory",
                  WA_Left,12,WA_Top,(ULONG)top,
                  WA_Width,606,WA_Height,204,
                  WA_MinWidth,606,WA_MinHeight,204,
                  WA_MaxWidth,606,WA_MaxHeight,204,
                  WA_DragBar,TRUE,WA_DepthGadget,TRUE,
                  WA_CloseGadget,TRUE,WA_Activate,TRUE,
                  WA_SmartRefresh,TRUE,
                  WA_IDCMP,LISTVIEWIDCMP|BUTTONIDCMP|
                           IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW,
                  TAG_DONE);
    if(!v->window){dolly_inventory_view_close(v);return 0;}
    last=CreateContext(&v->gadgets);
    if(!last){dolly_inventory_view_close(v);return 0;}
    if(v->count) {
        last=create_list(last,v,screen,20,
                         (LONG)v->window->BorderTop+47,548,93);
        if(!last){dolly_inventory_view_close(v);return 0;}
    }
    last=create_button(last,v,screen,392,
                       (LONG)v->window->Height-(LONG)v->window->BorderBottom-19,
                       176);
    if(!last){dolly_inventory_view_close(v);return 0;}
    AddGList(v->window,v->gadgets,(ULONG)-1,-1,NULL);
    paint(v,screen);
    return 1;
}
void dolly_inventory_view_pump(DollyNativeInventoryView *v) {
    struct IntuiMessage *msg;
    if(!v || !v->window)return;
    while((msg=GT_GetIMsg(v->window->UserPort))!=NULL) {
        ULONG cls=msg->Class;
        UWORD id=0;
        if(cls==IDCMP_GADGETUP && msg->IAddress)
            id=((struct Gadget *)msg->IAddress)->GadgetID;
        if(cls==IDCMP_REFRESHWINDOW){
            GT_BeginRefresh(v->window);
            GT_EndRefresh(v->window,TRUE);
        }
        GT_ReplyIMsg(msg);
        if(cls==IDCMP_CLOSEWINDOW || id==INV_CLOSE_ID) {
            dolly_inventory_view_close(v);return;
        }
        if(cls==IDCMP_REFRESHWINDOW)
            paint(v,v->window->WScreen);
    }
}
