#include "dolly_amiga_restore_source.h"
#include "dolly_dly_reader.h"
#include "dolly_amiga_fonts.h"
/* Native AmigaOS 2.04 endpoint picker, synthetic device tree.
 * Mode-first workflow: Physical device, Logical device, Partition, File.
 * No OpenDevice(), block reads or writes.
 */
#include "dolly_amiga_endpoint_picker.h"
#include "dolly_amiga_device_query.h"
#include "dolly_amiga_io_settings.h"
#include "dolly_amiga_destructive.h"
#include "dolly_surface_log.h"
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
#include <strings.h>

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
#define PICK_FILE_INFO 511u
#define PICK_SELECTION_TEXT 512u
#define PICK_UNIT_TEXT 520u
#define PICK_PARTITION_NUMBER 521u
#define PICK_DISK_INFO_TEXT 513u
#define PICK_DISK_SIZE_TEXT 514u
#define PICK_QUERY 515u
#define PICK_PARTITION_INFO_TEXT 516u
#define PICK_GEOMETRY_INFO_TEXT 517u
#define PICK_ATA_INFO_TEXT 518u

static STRPTR mode_names[]={
    (STRPTR)"Physical device",
    (STRPTR)"Logical device",
    (STRPTR)"Partition",
    (STRPTR)"File",NULL
};
static int set_allowed_modes(DollyEndpointPicker *p,unsigned mask) {
    unsigned i;
    p->allowed_mask=mask;
    p->mode_count=0;
    p->mode_index=0;
    for(i=0;i<4u;++i) {
        if(dolly_mode_allowed(mask,(DollyEndpointMode)i)) {
            p->allowed_mode_labels[p->mode_count]=mode_names[i];
            p->allowed_mode_ids[p->mode_count]=(DollyEndpointMode)i;
            ++p->mode_count;
        }
    }
    p->allowed_mode_labels[p->mode_count]=NULL;
    return p->mode_count!=0u;
}
static void sync_mode_index(DollyEndpointPicker *p) {
    unsigned i;
    for(i=0;i<p->mode_count;++i)
        if(p->allowed_mode_ids[i]==p->mode) {
            p->mode_index=i;
            return;
        }
    p->mode_index=0;
    p->mode=p->allowed_mode_ids[0];
}

/* The driver/unit lists contain DOS startup tuples and Exec candidates.
 * No guessed devices and no fabricated RDB/MBR partition labels.
 */
/* Logical devices now come from a bounded AmigaDOS snapshot. */
/* All coordinates are window-relative. At the 606x195 baseline this
 * matches the accepted layout; on larger public screens the LISTVIEWs
 * grow vertically and each row grows horizontally. The bottom actions
 * stay accessible, regardless of how large the chooser becomes.
 */
typedef struct PickerGeometry {
    WORD left,right,full_width;
    WORD mode_x,mode_w,query_w,row_h,baseline;
    WORD list_y,list_h,action_y,preview_y,disk_info_y,partition_info_y;
    WORD geometry_info_y,size_y,ata_info_y;
    WORD browse_y,path_y,detail_y;
    WORD cycle_y,heading_y,list_label_y;
    WORD value_x,value_w,unit_x,unit_w,unit_label_x,partition_w;
    WORD name_w,geometry_w,size_label_x,size_x,size_w;
} PickerGeometry;
static LONG picker_text_span(struct RastPort *rp,const char *text,LONG minimum) {
    LONG width=TextLength(rp,(STRPTR)text,(ULONG)strlen(text))+10;
    return width>minimum?width:minimum;
}
static LONG picker_number_span(struct RastPort *rp,LONG digits,LONG minimum) {
    LONG widest=0,width;
    char digit;
    for(digit='0';digit<='9';++digit) {
        width=TextLength(rp,(STRPTR)&digit,1);
        if(width>widest)widest=width;
    }
    width=digits*widest+12;
    return width>minimum?width:minimum;
}

static LONG picker_row_height(const struct TextFont *font) {
    LONG height=(LONG)font->tf_YSize+6;
    return height>16?height:16;
}

static PickerGeometry picker_geometry(const DollyEndpointPicker *p) {
    PickerGeometry g;
    const struct Window *w=p->window;
    LONG label_width;
    LONG left=20;
    LONG right=(LONG)w->Width-(LONG)w->BorderRight-20;
    /* Classic non-square-pixel Workbench modes (e.g. 640x256)
     * benefit from tighter top/bottom margins. Keep the accepted
     * spacing for conventional 4:3 and RTG screen geometries.
     */
    int compact=(ULONG)p->screen->Width >
                2ul*(ULONG)p->screen->Height;
    LONG row=picker_row_height(w->RPort->Font);
    LONG step=row+3;
    LONG header=w->BorderTop+(compact?8:13);
    LONG list_top=header+w->RPort->TxHeight+
                  (p->mode==DOLLY_MODE_LOGICAL?4:row+13);
    LONG action=w->Height-w->BorderBottom-row-(compact?7:15);
    LONG preview=action-step;
    LONG first=action-4*step;
    LONG height=(p->mode==DOLLY_MODE_PHYSICAL ||
                 p->mode==DOLLY_MODE_PARTITION ? first : preview)-list_top-3;
    g.row_h=(WORD)row;
    g.baseline=(WORD)((row-w->RPort->TxHeight)/2+w->RPort->TxBaseline);
    g.left=(WORD)left;
    g.right=(WORD)right;
    g.full_width=(WORD)(right-left);
    /* Both role-specific captions are longer than the former
     * "Select type:"; reserve space before the CYCLE gadget.
     */
    g.mode_x=(WORD)(left+picker_text_span(w->RPort,p->role?
                  "Select Destination Medium":"Select Source Medium",230));
    g.mode_w=(WORD)(right-g.mode_x);
    g.query_w=(WORD)picker_text_span(w->RPort,"Query",80);
    g.list_y=(WORD)list_top;
    g.cycle_y=(WORD)header;
    g.heading_y=(WORD)(header+g.baseline);
    g.list_label_y=(WORD)(list_top-4-w->RPort->TxHeight+w->RPort->TxBaseline);
    g.list_h=(WORD)height;
    g.action_y=(WORD)action;
    g.preview_y=(WORD)(p->mode==DOLLY_MODE_PHYSICAL ||
                            p->mode==DOLLY_MODE_PARTITION ?
                            first:preview);
    g.disk_info_y=(WORD)((p->mode==DOLLY_MODE_PARTITION ||
                            p->mode==DOLLY_MODE_PHYSICAL) ?
                            first+step:preview);
    g.ata_info_y=(WORD)(action-2*step);
    g.geometry_info_y=(WORD)preview;
    g.partition_info_y=(WORD)preview;
    g.browse_y=(WORD)list_top;
    g.path_y=(WORD)(list_top+row+16);
    g.detail_y=(WORD)(g.path_y+row+11);
    /* A shared label column keeps the previews aligned in all
     * modes. The Size field is right-anchored and stays separate.
     */
    if(p->mode==DOLLY_MODE_PHYSICAL || p->mode==DOLLY_MODE_PARTITION) {
        label_width=picker_text_span(w->RPort,"Device",84);
        label_width=picker_text_span(w->RPort,"SCSI name",label_width);
        label_width=picker_text_span(w->RPort,"ATA IDENTIFY",label_width);
        label_width=picker_text_span(w->RPort,p->mode==DOLLY_MODE_PHYSICAL?
                                     "Geometry":"Partition",label_width);
    } else label_width=picker_text_span(w->RPort,p->mode==DOLLY_MODE_FILE?
                                           "Selected file":"Selected device",140);
    g.value_x=(WORD)(left+label_width);
    g.value_w=(WORD)(right-g.value_x);
    g.unit_w=(WORD)picker_number_span(w->RPort,10,58);
    g.partition_w=(WORD)picker_number_span(w->RPort,2,32);
    g.unit_x=(WORD)(right-g.query_w-4-g.unit_w);
    g.unit_label_x=(WORD)(g.unit_x-picker_text_span(w->RPort,"Unit",38));
    g.size_w=(WORD)picker_text_span(w->RPort,"999999 MiB",102);
    g.size_x=(WORD)(right-g.size_w);
    g.size_label_x=(WORD)(g.size_x-picker_text_span(w->RPort,"Size",46));
    g.geometry_w=(WORD)(g.size_label_x-8-g.value_x);
    g.name_w=p->mode==DOLLY_MODE_PHYSICAL?g.value_w:g.geometry_w;
    g.size_y=p->mode==DOLLY_MODE_PHYSICAL?g.geometry_info_y:g.disk_info_y;
    return g;
}
static void init_list(struct List *l) {
    l->lh_Head=(struct Node *)&l->lh_Tail;
    l->lh_Tail=NULL;
    l->lh_TailPred=(struct Node *)&l->lh_Head;
    l->lh_Type=NT_USER;l->l_pad=0;
}
/* Only called when GadTools has no references to these nodes. */
static unsigned append_device(DollyEndpointPicker *p,const char *driver) {
    unsigned i;
    for(i=0;i<p->device_count;++i)
        if(!strcasecmp(driver,p->physical_names[i]))return i;
    if(i<DOLLY_PHYS_MAX+1u)++p->device_count;
    else {
        --i;
        Remove(&p->device_nodes[i]);
    }
    strcpy(p->physical_names[i],driver);
    memset(&p->device_nodes[i],0,sizeof(p->device_nodes[i]));
    p->device_nodes[i].ln_Name=p->physical_names[i];
    AddTail(&p->devices,&p->device_nodes[i]);
    return i;
}
static void populate_devices(DollyEndpointPicker *p) {
    unsigned i;
    init_list(&p->devices);
    p->device_count=0;
    for(i=0;i<p->physical_catalog.count;++i)
        (void)append_device(p,p->physical_catalog.entries[i].driver);
}
static void insert_unit(DollyEndpointPicker *p,ULONG value) {
    unsigned at=0;
    while(at<p->unit_count && p->physical_units[at]<value)++at;
    if(at<p->unit_count && p->physical_units[at]==value)return;
    memmove(&p->physical_units[at+1],&p->physical_units[at],
            (p->unit_count-at)*sizeof(p->physical_units[0]));
    p->physical_units[at]=value;
    ++p->unit_count;
}

static void populate_units(DollyEndpointPicker *p,int probe) {
    unsigned i;
    init_list(&p->units);
    p->unit_count=0;
    if(probe)p->probed_count=0;
    if(!p->has_device || p->device>=p->device_count)return;
    for(i=0;i<p->physical_catalog.count;++i) {
        const DollyPhysicalEntry *e=&p->physical_catalog.entries[i];
        if(e->unit_registered && !strcasecmp(e->driver,p->physical_names[p->device]))
            insert_unit(p,e->unit);
    }
    if(probe)
        p->probed_count=dolly_physical_probe_units(p->physical_names[p->device],p->probed_units);
    for(i=0;i<p->probed_count;++i)insert_unit(p,p->probed_units[i]);
    if(p->has_manual_unit)insert_unit(p,p->manual_unit);
    for(i=0;i<p->unit_count;++i) {
        sprintf(p->unit_names[i],"Unit %lu",(unsigned long)p->physical_units[i]);
        memset(&p->unit_nodes[i],0,sizeof(p->unit_nodes[i]));
        p->unit_nodes[i].ln_Name=p->unit_names[i];
        AddTail(&p->units,&p->unit_nodes[i]);
    }

}
/* Prepare the OTHER bank, never touching LISTVIEW-owned nodes.
 * Output labels are bounded by construction (max RDB label 31 bytes).
 */
static int populate_partitions(DollyEndpointPicker *p) {
    unsigned i,bank=p->partition_bank^1u;
    struct List *items=&p->partition_lists[bank];
    if(p->partition_count>DOLLY_PART_MAX)return 0;
    init_list(items);
    for(i=0;i<p->partition_count;++i) {
        const char *name=p->partition_map.parts[i].label;
        char *dst=p->partition_list_labels[bank][i];
        struct Node *node=&p->partition_nodes[bank][i];
        unsigned n=0,j=0;
        /* Emit "1: " through "16: " without varargs or any
         * possible sprintf source/destination alias.
         */
        if(i+1u>=10u)dst[n++]=(char)('0'+(i+1u)/10u);
        dst[n++]=(char)('0'+(i+1u)%10u);
        dst[n++]=':';
        dst[n++]=' ';
        if(p->partition_map.parts[i].outside_media) {
            dst[n++]='!';
            dst[n++]=' ';
        }
        while(j<sizeof(p->partition_map.parts[i].label) &&
              name[j] && n+1u<48u)
            dst[n++]=name[j++];
        dst[n]='\0';
        memset(node,0,sizeof(*node));
        node->ln_Name=dst;
        AddTail(items,node);
    }
    p->partition_bank=bank;
    return 1;
}
static int physical_choice(const DollyEndpointPicker *p) {
    return p->has_device && p->device<p->device_count &&
           p->has_unit && p->unit<p->unit_count;
}
static void invalidate_query(DollyEndpointPicker *p) {
    p->queried=0;
    p->query_attempted=0;
    p->has_partition=0;
    p->partition_count=0;
    p->partition_scanned=0;
    memset(&p->media_geometry,0,sizeof(p->media_geometry));
    memset(&p->partition_map,0,sizeof(p->partition_map));
    p->partition_info_display[0]='\0';
    p->geometry_info_display[0]='\0';
    strcpy(p->ata_identify,"Not queried");
    (void)populate_partitions(p);
}
static void make_logical_list(DollyEndpointPicker *p) {
    struct List *l=&p->logical_devices;
    unsigned i;
    l->lh_Head=(struct Node *)&l->lh_Tail;
    l->lh_Tail=NULL;
    l->lh_TailPred=(struct Node *)&l->lh_Head;
    l->lh_Type=NT_USER;
    l->l_pad=0;
    for(i=0;i<p->logical_count;++i) {
        struct Node *n=&p->logical_nodes[i];
        memset(n,0,sizeof(*n));
        n->ln_Name=p->dos_devices.names[i];
        AddTail(l,n);
    }
}
static struct Gadget *button(struct Gadget *last,DollyEndpointPicker *p,
                             UWORD id,WORD x,WORD y,WORD w,
                             const char *name,int disabled) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    ng.ng_LeftEdge=x;ng.ng_TopEdge=y;
    ng.ng_Width=w;ng.ng_Height=picker_geometry(p).row_h;
    ng.ng_GadgetID=id;ng.ng_GadgetText=(STRPTR)name;
    return CreateGadget(BUTTON_KIND,last,&ng,
                        GA_Disabled,(ULONG)disabled,TAG_DONE);
}
static const char *read_device_field(const DollyEndpointPicker *p) {
    const struct StringInfo *info;
    const char *name;
    size_t i,length;
    if(!p->selection_text)return NULL;
    info=(const struct StringInfo *)p->selection_text->SpecialInfo;
    name=(const char *)info->Buffer;
    length=strlen(name);
    if(length<=7u || length>=DOLLY_PHYS_DRIVER_LEN ||
       strcmp(name+length-7u,".device") || !strcasecmp(name,"trackdisk.device"))
        return NULL;
    for(i=0;i<length;++i)
        if((unsigned char)name[i]<=32u || (unsigned char)name[i]>126u ||
           name[i]=='/' || name[i]==':')return NULL;
    return name;
}
static int read_unit_field(const DollyEndpointPicker *p,ULONG *unit) {
    const struct StringInfo *info;
    const char *digit;
    ULONG number=0;
    if(!p->unit_text)return 0;
    info=(const struct StringInfo *)p->unit_text->SpecialInfo;
    digit=(const char *)info->Buffer;
    if(!*digit)return 0;
    for(;*digit>='0' && *digit<='9';++digit) {
        ULONG value=(ULONG)(*digit-'0');
        if(number>(0xfffffffful-value)/10ul)return 0;
        number=10ul*number+value;
    }
    if(*digit)return 0;
    *unit=number;
    return 1;
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
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    ng.ng_LeftEdge=x;ng.ng_TopEdge=y;
    ng.ng_Width=w;
    ng.ng_Height=picker_geometry(p).list_h;
    ng.ng_GadgetID=id;ng.ng_GadgetText=(STRPTR)"";
    /* Without GTLV_ShowSelected, LISTVIEW_KIND only flashes the
     * active row while the mouse is down. Explicitly requesting a
     * selection display makes it persistent: V39+ highlights the
     * row, while V37 shows the selected text below the list.
     * GTLV_Selected now also restores selection after rebuilding.
     */
    return CreateGadget(LISTVIEW_KIND,last,&ng,
                        GTLV_Labels,(ULONG)items,
                        GTLV_ShowSelected,(ULONG)NULL,
                        GTLV_Selected,selected,
                        GA_Disabled,(ULONG)disabled,TAG_DONE);
}
/* Read the gadget's live buffer before committing or rebuilding it. */
static void capture_file_text(DollyEndpointPicker *p) {
    const struct StringInfo *info;
    if(!p->file_text)return;
    info=(const struct StringInfo *)p->file_text->SpecialInfo;
    if(!info || !info->Buffer)return;
    if(!dolly_endpoint_set_file(&p->pending_file,(const char *)info->Buffer))
        dolly_endpoint_clear(&p->pending_file);
}
static int physical_mode(const DollyEndpointPicker *p) {
    return p->mode==DOLLY_MODE_PHYSICAL || p->mode==DOLLY_MODE_PARTITION;
}
static void update_selection_preview(DollyEndpointPicker *p) {
    const char *name="";
    if(p->mode==DOLLY_MODE_LOGICAL &&
       p->has_logical && p->logical<p->logical_count)
        name=p->dos_devices.names[p->logical];
    if(physical_mode(p) && p->has_device && p->device<p->device_count)
        name=p->physical_names[p->device];
    strcpy(p->selection_display,name);
    strcpy(p->unit_display,"0");
    if(physical_choice(p))
        snprintf(p->unit_display,sizeof(p->unit_display),"%lu",
                 (unsigned long)p->physical_units[p->unit]);
}
static struct Gadget *selection_text(struct Gadget *last,DollyEndpointPicker *p,
                                     int unit) {
    struct NewGadget ng={0};
    PickerGeometry g=picker_geometry(p);
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    ng.ng_LeftEdge=unit?g.unit_x:g.value_x;
    ng.ng_TopEdge=g.preview_y;
    ng.ng_Width=unit?g.unit_w:
                physical_mode(p)?g.unit_label_x-g.value_x-6:g.value_w;
    ng.ng_Height=g.row_h;
    ng.ng_GadgetID=unit?PICK_UNIT_TEXT:PICK_SELECTION_TEXT;
    if(unit)
        return CreateGadget(STRING_KIND,last,&ng,
                            GTST_String,(ULONG)p->unit_display,GTST_MaxChars,10UL,
                            GA_Disabled,(ULONG)!p->has_device,TAG_DONE);
    return CreateGadget(STRING_KIND,last,&ng,
                        GTST_String,(ULONG)p->selection_display,
                        GTST_MaxChars,(ULONG)(physical_mode(p)?
                            DOLLY_PHYS_DRIVER_LEN-1u:DOLLY_DOS_NAME_BYTES-1u),TAG_DONE);
}
static struct Gadget *partition_number(struct Gadget *last,DollyEndpointPicker *p) {
    struct NewGadget ng={0};
    PickerGeometry g=picker_geometry(p);
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    ng.ng_LeftEdge=g.value_x;
    ng.ng_TopEdge=g.partition_info_y;
    ng.ng_Width=g.partition_w;
    ng.ng_Height=g.row_h;
    ng.ng_GadgetID=PICK_PARTITION_NUMBER;
    return CreateGadget(INTEGER_KIND,last,&ng,
                        GTIN_Number,p->has_partition?(ULONG)p->partition+1:0UL,
                        GTIN_MaxChars,2UL,
                        GA_Disabled,(ULONG)!p->partition_count,TAG_DONE);
}
static void refresh_selection_preview(DollyEndpointPicker *p) {
    update_selection_preview(p);
    if(p->selection_text)
        GT_SetGadgetAttrs(p->selection_text,p->window,NULL,
                          GTST_String,
                          (ULONG)p->selection_display,TAG_DONE);
    if(p->unit_text)
        GT_SetGadgetAttrs(p->unit_text,p->window,NULL,
                          GTST_String,(ULONG)p->unit_display,TAG_DONE);
    if(p->partition_number)
        GT_SetGadgetAttrs(p->partition_number,p->window,NULL,
                          GTIN_Number,p->has_partition?(ULONG)p->partition+1:0UL,
                          GA_Disabled,(ULONG)!p->partition_count,TAG_DONE);
}
/* A listed unit is only a candidate. Explicit Query verifies geometry. */
static void update_disk_info(DollyEndpointPicker *p) {
    if(!p->query_attempted)strcpy(p->ata_identify,"Not queried");
    if(!physical_choice(p)) {
        strcpy(p->disk_info_display,"Select unit...");
        strcpy(p->disk_size_display,"Unknown");
    } else if(!p->query_attempted) {
        strcpy(p->disk_info_display,"Press Query to read device info");
        strcpy(p->disk_size_display,"Not verified");
    } else if(!p->media_geometry.capacity_valid) {
        /* Do not mistake a successful INQUIRY for proof of media. */
        strcpy(p->disk_size_display,"Unknown");
        if(!p->queried)
            strcpy(p->disk_info_display,"Query failed / no media");
    }
    if(!physical_choice(p))
        strcpy(p->geometry_info_display,"Select unit...");
    else if(!p->query_attempted)
        strcpy(p->geometry_info_display,"Press Query");
    else if(p->media_geometry.capacity_valid) {
        const char *source=p->media_geometry.capacity_source==
           DOLLY_GEOMETRY_TRACKDISK ? "TD_GETGEOMETRY" :
           p->media_geometry.capacity_source==
           DOLLY_GEOMETRY_SCSI_CAPACITY10 ? "SCSI READ CAPACITY(10)" :
           "Unknown source";
        sprintf(p->geometry_info_display,"%lu B/sector | %s",
                (unsigned long)p->media_geometry.block_bytes,source);
    } else strcpy(p->geometry_info_display,"Unavailable");
    /* Successful Query results remain in view-owned buffers through
     * resize; changing the selection invalidates them.
     */
}
static struct Gadget *disk_info_text(struct Gadget *last,
                                     DollyEndpointPicker *p) {
    struct NewGadget ng;
    PickerGeometry g=picker_geometry(p);
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    ng.ng_LeftEdge=g.value_x;ng.ng_TopEdge=g.disk_info_y;
    ng.ng_Width=g.name_w;ng.ng_Height=g.row_h;
    ng.ng_GadgetID=PICK_DISK_INFO_TEXT;
    ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(TEXT_KIND,last,&ng,
                        GTTX_Text,(ULONG)p->disk_info_display,
                        GTTX_Border,TRUE,GTTX_Clipped,TRUE,TAG_DONE);
}
static struct Gadget *disk_size_text(struct Gadget *last,
                                     DollyEndpointPicker *p) {
    struct NewGadget ng;
    PickerGeometry g=picker_geometry(p);
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    ng.ng_LeftEdge=g.size_x;ng.ng_TopEdge=g.size_y;
    ng.ng_Width=g.size_w;ng.ng_Height=g.row_h;
    ng.ng_GadgetID=PICK_DISK_SIZE_TEXT;
    ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(TEXT_KIND,last,&ng,
                        GTTX_Text,(ULONG)p->disk_size_display,
                        GTTX_Border,TRUE,TAG_DONE);
}
static struct Gadget *geometry_info_text(struct Gadget *last,
                                         DollyEndpointPicker *p,int ata) {
    struct NewGadget ng;
    PickerGeometry g=picker_geometry(p);
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    ng.ng_LeftEdge=g.value_x;ng.ng_TopEdge=ata?g.ata_info_y:g.geometry_info_y;
    ng.ng_Width=ata?g.value_w:g.geometry_w;ng.ng_Height=g.row_h;
    ng.ng_GadgetID=ata?PICK_ATA_INFO_TEXT:PICK_GEOMETRY_INFO_TEXT;
    ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(TEXT_KIND,last,&ng,
                        GTTX_Text,(ULONG)(ata?p->ata_identify:p->geometry_info_display),
                        GTTX_Border,TRUE,GTTX_Clipped,TRUE,TAG_DONE);
}
static void refresh_disk_info(DollyEndpointPicker *p) {
    if(p->mode!=DOLLY_MODE_PHYSICAL &&
       p->mode!=DOLLY_MODE_PARTITION)return;
    update_disk_info(p);
    if(p->disk_info_text && p->window)
        GT_SetGadgetAttrs(p->disk_info_text,p->window,NULL,
                          GTTX_Text,(ULONG)p->disk_info_display,TAG_DONE);
    if(p->disk_size_text && p->window)
        GT_SetGadgetAttrs(p->disk_size_text,p->window,NULL,
                          GTTX_Text,(ULONG)p->disk_size_display,TAG_DONE);
    if(p->ata_info_text && p->window)
        GT_SetGadgetAttrs(p->ata_info_text,p->window,NULL,
                          GTTX_Text,(ULONG)p->ata_identify,TAG_DONE);
    if(p->geometry_info_text && p->window)
        GT_SetGadgetAttrs(p->geometry_info_text,p->window,NULL,
                          GTTX_Text,(ULONG)p->geometry_info_display,TAG_DONE);
}
static void partition_gap(const DollyEndpointPicker *p,char sectors[21],char size[32]) {
    const DollyNativePartition *part=&p->partition_map.parts[p->partition];
    static const char *const units[]={"KiB","MiB","GiB"};
    uint64_t bytes=part->start_bytes+part->length_bytes-
                   p->media_geometry.block_count*p->media_geometry.block_bytes;
    uint64_t scale=1024u;
    unsigned unit=0,fraction;
    char whole[21];
    dolly_surface_log_decimal(sectors,bytes/p->media_geometry.block_bytes);
    while(unit<2 && bytes>=scale*1024u) {scale*=1024u;++unit;}
    dolly_surface_log_decimal(whole,bytes/scale);
    fraction=(unsigned)((bytes%scale)*100u/scale);
    if(!fraction)snprintf(size,32,"%s %s",whole,units[unit]);
    else if(fraction%10u==0)
        snprintf(size,32,"%s.%c %.3s",whole,'0'+(int)(fraction/10u),units[unit]);
    else snprintf(size,32,"%s.%c%c %.3s",whole,
                  '0'+(int)(fraction/10u),'0'+(int)(fraction%10u),units[unit]);
}
static void update_partition_details(DollyEndpointPicker *p) {
    const char *text;
    char warning[128],sectors[21],size[32];
    ULONG n;
    if(p->mode!=DOLLY_MODE_PARTITION)return;
    if(p->has_partition && p->partition<p->partition_count) {
        if(p->partition_map.parts[p->partition].outside_media) {
            partition_gap(p,sectors,size);
            snprintf(warning,sizeof(warning),"Outside driver reported sector count by %s sectors (%s)",
                     sectors,size);
            text=warning;
        } else text=p->partition_map.parts[p->partition].summary;
    }
    else if(p->partition_map.rdb_capacity_mismatch)
        text=p->partition_map.status;
    else if(p->query_attempted && !p->partition_scanned)
        text=p->partition_map.status;
    else if(p->partition_scanned)
        text="Select a partition...";
    else text="Press Query to read RDB/MBR...";
    strncpy(p->partition_info_display,text,
            sizeof(p->partition_info_display)-1u);
    p->partition_info_display[sizeof(p->partition_info_display)-1u]='\0';
    if(!p->window)return;
    if(text==warning) {
        PickerGeometry g=picker_geometry(p);
        if(TextLength(p->window->RPort,(STRPTR)warning,strlen(warning))>
           g.value_w-g.partition_w-22) {
            snprintf(warning,sizeof(warning),"Beyond driver limit: %s sectors (%s)",sectors,size);
            strcpy(p->partition_info_display,warning);
        }
    }
    n=(ULONG)strlen(p->partition_info_display);
    while(n>3u &&
          (LONG)TextLength(p->window->RPort,
                           (STRPTR)p->partition_info_display,n)>
          (LONG)picker_geometry(p).value_w-picker_geometry(p).partition_w-22)
        p->partition_info_display[--n]='\0';
    if(n<(ULONG)strlen(text) && n>=3u) {
        p->partition_info_display[n-3u]='.';
        p->partition_info_display[n-2u]='.';
        p->partition_info_display[n-1u]='.';
    }
}
static struct Gadget *partition_info_text(struct Gadget *last,
                                          DollyEndpointPicker *p) {
    struct NewGadget ng;
    PickerGeometry g=picker_geometry(p);
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    ng.ng_LeftEdge=g.value_x+g.partition_w+6;
    ng.ng_TopEdge=g.partition_info_y;
    ng.ng_Width=g.value_w-g.partition_w-6;
    ng.ng_Height=g.row_h;
    ng.ng_GadgetID=PICK_PARTITION_INFO_TEXT;
    ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(TEXT_KIND,last,&ng,
                        GTTX_Text,(ULONG)p->partition_info_display,
                        GTTX_Border,TRUE,TAG_DONE);
}
static void refresh_partition_details(DollyEndpointPicker *p) {
    update_partition_details(p);
    if(p->partition_info_text && p->window)
        GT_SetGadgetAttrs(p->partition_info_text,p->window,NULL,
                          GTTX_Text,(ULONG)p->partition_info_display,
                          TAG_DONE);
}
static struct Gadget *file_text(struct Gadget *last,DollyEndpointPicker *p) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    ng.ng_LeftEdge=picker_geometry(p).value_x;
    ng.ng_TopEdge=picker_geometry(p).path_y;
    ng.ng_Width=picker_geometry(p).value_w;ng.ng_Height=picker_geometry(p).row_h;
    ng.ng_GadgetID=PICK_FILE_TEXT;
    ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(STRING_KIND,last,&ng,
                        GTST_String,(ULONG)p->pending_file.path,
                        GTST_MaxChars,(ULONG)(sizeof(p->pending_file.path)-1u),TAG_DONE);
}
static struct Gadget *details_text(struct Gadget *last,DollyEndpointPicker *p) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=p->visual;
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    ng.ng_LeftEdge=picker_geometry(p).value_x;
    ng.ng_TopEdge=picker_geometry(p).detail_y;
    ng.ng_Width=picker_geometry(p).value_w;ng.ng_Height=picker_geometry(p).row_h;
    ng.ng_GadgetID=PICK_FILE_INFO;ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(TEXT_KIND,last,&ng,
                        GTTX_Text,(ULONG)p->file_details,
                        GTTX_Border,TRUE,TAG_DONE);
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
static int describe_dly_header(const unsigned char *header,size_t bytes,uint64_t file_bytes,
                               char *out,size_t capacity) {
    DollyDlySetInfo set;
    uint32_t form_length;
    int embedded;
    char total[21];
    if(bytes>=sizeof(DOLLY_DLY_SET_MAGIC)-1u &&
       !memcmp(header,DOLLY_DLY_SET_MAGIC,sizeof(DOLLY_DLY_SET_MAGIC)-1u)) {
        if(bytes!=file_bytes ||
           dolly_dly_set_parse((const char *)header,bytes,&set)!=DOLLY_BLOCK_OK)
            snprintf(out,capacity,"Invalid IFF DOLY manifest.");
        else {
            dolly_surface_log_decimal(total,set.sectors*set.sector_bytes);
            snprintf(out,capacity,"IFF DOLY: %lu parts, %s bytes decoded.",
                     (unsigned long)set.parts,total);
        }
        return 1;
    }
    if(bytes<12u || memcmp(header,"FORM",4) || memcmp(header+8,"DOLY",4))return 0;
    form_length=((uint32_t)header[4]<<24)|((uint32_t)header[5]<<16)|
                ((uint32_t)header[6]<<8)|header[7];
    embedded=bytes>=DOLLY_DLY_GEOM_END+4u &&
             !memcmp(header+DOLLY_DLY_GEOM_END,"MSET",4);
    if((uint64_t)form_length+8u!=file_bytes || bytes<DOLLY_DLY_GEOM_END ||
       (embedded && dolly_dly_set_unpack(header,bytes,&set)!=DOLLY_BLOCK_OK)) {
        snprintf(out,capacity,"Invalid IFF DOLY header.");
        return 1;
    }
    if(!embedded) {
        snprintf(out,capacity,"IFF DOLY: %lu file bytes.",(unsigned long)file_bytes);
        return 1;
    }
    dolly_surface_log_decimal(total,set.sectors*set.sector_bytes);
    snprintf(out,capacity,"IFF DOLY part %lu/%lu: %s bytes decoded.",
             (unsigned long)(set.part?set.part:1u),(unsigned long)set.parts,total);
    return 1;
}

void dolly_amiga_describe_file(const DollyEndpoint *file,char *out,size_t capacity) {
    const char *hint="unknown";
    unsigned char header[256];
    BPTR handle;
    uint64_t size;
    if(file->kind!=DOLLY_ENDPOINT_FILE) {
        snprintf(out,capacity,"No source file selected.");
        return;
    }
    if(suffix_is(file->path,".adf"))hint="ADF (extension)";
    else if(suffix_is(file->path,".hdf"))hint="HDF (extension)";
    else if(suffix_is(file->path,".raw") || suffix_is(file->path,".img"))
        hint="RAW (extension)";
    else if(suffix_is(file->path,".dly") || suffix_is(file->path,".doly") || suffix_is(file->path,".iff"))
        hint="IFF? (extension)";
    snprintf(out,capacity,"Type: %s | Size: unavailable",hint);
    if(!DOSBase)return;
    handle=Open((STRPTR)file->path,MODE_OLDFILE);
    if(!handle)return;
    if(dolly_amiga_image_file_size(handle,&size)==DOLLY_BLOCK_OK) {
        LONG length=(LONG)(size<sizeof(header)?size:sizeof(header));
        int read_ok=Read(handle,header,length)==length;
        if(!read_ok || !describe_dly_header(header,(size_t)length,size,out,capacity)) {
            if(read_ok) {
                DollyAppleMedia media=dolly_amiga_probe_apple_file(handle,size);
                if(media!=DOLLY_APPLE_UNKNOWN)hint=dolly_apple_media_name(media);
            }
            snprintf(out,capacity,"Type: %s | Size: %lu bytes",hint,(unsigned long)size);
        }
    }
    Close(handle);
}

static void file_details(DollyEndpointPicker *p) {
    ULONG len;
    if(p->role)return;
    dolly_amiga_describe_file(&p->pending_file,p->file_details,sizeof(p->file_details));
    /* Clip only the display string, not the stored filename/path. */
    if(p->window) {
        LONG available=(LONG)picker_geometry(p).value_w-16;
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
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    ng.ng_LeftEdge=picker_geometry(p).mode_x;
    ng.ng_TopEdge=picker_geometry(p).cycle_y;
    ng.ng_Width=picker_geometry(p).mode_w;ng.ng_Height=picker_geometry(p).row_h;
    ng.ng_GadgetID=PICK_MODE;ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(CYCLE_KIND,last,&ng,
                        GTCY_Labels,(ULONG)p->allowed_mode_labels,
                        GTCY_Active,(ULONG)p->mode_index,TAG_DONE);
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
    PickerGeometry g;
    if(!p->window)return;
    rp=p->window->RPort;
    g=picker_geometry(p);
    info=GetScreenDrawInfo(p->screen);
    SetDrMd(rp,JAM1);
    SetAPen(rp,info?info->dri_Pens[TEXTPEN]:1u);
    if(p->mode!=DOLLY_MODE_LOGICAL) {
        Move(rp,20,g.heading_y);
        if(p->role)
            Text(rp,(STRPTR)"Select Destination Medium",25);
        else
            Text(rp,(STRPTR)"Select Source Medium",20);
        if(p->mode_count==1u) {
            STRPTR name=p->allowed_mode_labels[0];
            Move(rp,g.mode_x,g.heading_y);
            Text(rp,name,strlen((const char *)name));
        }
    }
    if(p->mode==DOLLY_MODE_PHYSICAL || p->mode==DOLLY_MODE_PARTITION) {
        WORD x,w;
        unsigned columns=p->mode==DOLLY_MODE_PHYSICAL?2u:3u;
        column_geometry(p,columns,0u,&x,&w);
        Move(rp,x,g.list_label_y);Text(rp,(STRPTR)"Device",6);
        column_geometry(p,columns,1u,&x,&w);
        Move(rp,x,g.list_label_y);Text(rp,(STRPTR)"Unit",4);
        if(columns==3u) {
            column_geometry(p,columns,2u,&x,&w);
            Move(rp,x,g.list_label_y);
            if(TextLength(rp,(STRPTR)"Partition (RDB/MBR)",19)<=w-4)
                Text(rp,(STRPTR)"Partition (RDB/MBR)",19);
            else Text(rp,(STRPTR)"Partition",9);
        }
    } else if(p->mode==DOLLY_MODE_LOGICAL) {
        Move(rp,20,g.list_label_y);
        Text(rp,(STRPTR)"Registered DOS devices (read-only)",34);
    }
    /* Labels and recessed fields share a row, not separate vertical
     * lines. The same placement works on the reference 640x256 screen.
     */
    if(p->mode==DOLLY_MODE_FILE) {
        Move(rp,g.left,(LONG)g.path_y+g.baseline);
        Text(rp,(STRPTR)"Selected file",13);
        if(!p->role) {
            Move(rp,g.left,(LONG)g.detail_y+g.baseline);
            Text(rp,(STRPTR)"File info",9);
        }
    } else {
        Move(rp,g.left,(LONG)g.preview_y+g.baseline);
        if(physical_mode(p))Text(rp,(STRPTR)"Device",6);
        else Text(rp,(STRPTR)"Selected device",15);
        if(physical_mode(p)) {
            Move(rp,g.unit_label_x,(LONG)g.preview_y+g.baseline);
            Text(rp,(STRPTR)"Unit",4);
        }
        if(p->mode==DOLLY_MODE_PHYSICAL ||
           p->mode==DOLLY_MODE_PARTITION) {
            Move(rp,g.left,(LONG)g.disk_info_y+g.baseline);
            Text(rp,(STRPTR)"SCSI name",9);
            Move(rp,g.size_label_x,(LONG)g.size_y+g.baseline);
            Text(rp,(STRPTR)"Size",4);
            Move(rp,g.left,(LONG)g.ata_info_y+g.baseline);
            Text(rp,(STRPTR)"ATA IDENTIFY",12);
        }
        if(p->mode==DOLLY_MODE_PHYSICAL) {
            Move(rp,g.left,(LONG)g.geometry_info_y+g.baseline);
            Text(rp,(STRPTR)"Geometry",8);
        }
        if(p->mode==DOLLY_MODE_PARTITION) {
            Move(rp,g.left,(LONG)g.partition_info_y+g.baseline);
            Text(rp,(STRPTR)"Partition",9);
        }
    }
    if(info)FreeScreenDrawInfo(p->screen,info);
}
static void change_enabled(DollyEndpointPicker *p) {
    if(!p->window)return;
    if(p->unit_list)
        GT_SetGadgetAttrs(p->unit_list,p->window,NULL,
                          GA_Disabled,(ULONG)!p->has_device ||
                                      !p->unit_count,TAG_DONE);
    if(p->partition_list)
        GT_SetGadgetAttrs(p->partition_list,p->window,NULL,
                          GA_Disabled,(ULONG)!p->partition_count,
                          TAG_DONE);
    if(p->choose_button) {
        int ready=(p->mode==DOLLY_MODE_PHYSICAL &&
                   physical_choice(p) && p->queried &&
                   p->media_geometry.capacity_valid) ||
                  (p->mode==DOLLY_MODE_LOGICAL && p->logical_count) ||
                  (p->mode==DOLLY_MODE_PARTITION &&
                   physical_choice(p) && p->queried &&
                   p->media_geometry.capacity_valid &&
                   p->partition_scanned && p->partition_count &&
                   (!p->has_partition || !p->partition_map.parts[p->partition].outside_media ||
                    (!p->role && dolly_amiga_allow_outside_range()))) ||
                  p->mode==DOLLY_MODE_FILE;
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
    WORD cancel_width;
    if(!p->window)return 0;
    g=picker_geometry(p);
    if(physical_mode(p) && (g.unit_label_x-g.value_x<44 || g.geometry_w<24))
        return 0;
    if(p->mode!=DOLLY_MODE_FILE && g.list_h<p->window->RPort->TxHeight+3)
        return 0;
    capture_file_text(p);
    file_details(p);
    if(p->mode==DOLLY_MODE_LOGICAL && p->gadgets && p->selection_text) {
        const struct StringInfo *info=p->selection_text->SpecialInfo;
        strcpy(p->selection_display,(const char *)info->Buffer);
    } else if(p->mode!=DOLLY_MODE_FILE)update_selection_preview(p);
    if(p->mode==DOLLY_MODE_PHYSICAL ||
       p->mode==DOLLY_MODE_PARTITION)update_disk_info(p);
    if(p->mode==DOLLY_MODE_PARTITION)update_partition_details(p);
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
    p->choose_button=NULL;p->file_text=NULL;p->mode_cycle=NULL;
    p->file_info=NULL;
    p->selection_text=NULL;p->unit_text=NULL;p->partition_number=NULL;
    p->disk_info_text=NULL;
    p->disk_size_text=NULL;
    p->partition_info_text=NULL;
    p->geometry_info_text=NULL;p->ata_info_text=NULL;
    last=CreateContext(&p->gadgets);
    if(!last)return 0;
    if(p->mode_count>1u && p->mode!=DOLLY_MODE_LOGICAL) {
        last=cycle(last,p);
        if(!last)return 0;
        p->mode_cycle=last;
    }
    if(p->mode==DOLLY_MODE_PHYSICAL || p->mode==DOLLY_MODE_PARTITION) {
        WORD x,w;
        unsigned columns=p->mode==DOLLY_MODE_PHYSICAL?2u:3u;
        column_geometry(p,columns,0u,&x,&w);
        last=list(last,p,PICK_DEVICE,x,g.list_y,w,&p->devices,
                  !p->device_count);
        if(!last)return 0;
        p->device_list=last;
        column_geometry(p,columns,1u,&x,&w);
        last=list(last,p,PICK_UNIT,x,g.list_y,w,&p->units,
                  !p->has_device || !p->unit_count);
        if(!last)return 0;
        p->unit_list=last;
        if(columns==3u) {
            column_geometry(p,columns,2u,&x,&w);
            last=list(last,p,PICK_PARTITION,x,g.list_y,w,
                      &p->partition_lists[p->partition_bank],
                      !p->partition_count);
            if(!last)return 0;
            p->partition_list=last;
        }
    } else if(p->mode==DOLLY_MODE_LOGICAL) {
        last=list(last,p,PICK_LOGICAL,g.left,g.list_y,
                  g.full_width,&p->logical_devices,
                  !p->logical_count);
        if(!last)return 0;
        p->logical_list=last;
    }
    if(p->mode!=DOLLY_MODE_FILE) {
        last=selection_text(last,p,0);
        if(!last)return 0;
        p->selection_text=last;
        if(physical_mode(p)) {
            last=selection_text(last,p,1);
            if(!last)return 0;
            p->unit_text=last;
            last=button(last,p,PICK_QUERY,
                        (WORD)(g.right-g.query_w),g.preview_y,
                        g.query_w,"Query",0);
            if(!last)return 0;
        }
        if(p->mode==DOLLY_MODE_PHYSICAL ||
           p->mode==DOLLY_MODE_PARTITION) {
            last=disk_info_text(last,p);
            if(!last)return 0;
            p->disk_info_text=last;
            last=disk_size_text(last,p);
            if(!last)return 0;
            p->disk_size_text=last;
            last=geometry_info_text(last,p,1);
            if(!last)return 0;
            p->ata_info_text=last;
        }
    }
    if(p->mode==DOLLY_MODE_PHYSICAL) {
        last=geometry_info_text(last,p,0);
        if(!last)return 0;
        p->geometry_info_text=last;
    }
    if(p->mode==DOLLY_MODE_PARTITION) {
        last=partition_number(last,p);
        if(!last)return 0;
        p->partition_number=last;
        last=partition_info_text(last,p);
        if(!last)return 0;
        p->partition_info_text=last;
    }
    if(p->mode==DOLLY_MODE_FILE) {
        /* Reuse the otherwise empty LISTVIEW area. Full client width. */
        last=button(last,p,PICK_FILE,g.left,g.browse_y,
                    g.full_width,"Browse file...",0);
        if(!last)return 0;
        last=file_text(last,p);
        if(!last)return 0;
        p->file_text=last;
        if(!p->role) {
            last=details_text(last,p);
            if(!last)return 0;
            p->file_info=last;
        }
    }
    /* File mode now commits exactly like every other mode. */
    last=button(last,p,PICK_SELECT,g.left,g.action_y,
                (WORD)picker_text_span(p->window->RPort,"Use selection",172),"Use selection",1);
    if(!last)return 0;
    p->choose_button=last;
    cancel_width=(WORD)picker_text_span(p->window->RPort,"Cancel",176);
    last=button(last,p,PICK_CANCEL,
                (WORD)(g.right-cancel_width),g.action_y,cancel_width,"Cancel",0);
    if(!last)return 0;
    AddGList(p->window,p->gadgets,(ULONG)-1,-1,NULL);
    p->gadgets_detached=0;
    RefreshGList(p->gadgets,p->window,NULL,-1);
    GT_RefreshWindow(p->window,NULL);
    change_enabled(p);
    captions(p);
    return 1;
}
static int pick_file(struct Window *parent,DollyEndpoint *result,unsigned destination,
                     const char *title,const char *name) {
    struct FileRequester *fr;
    char path[256];
    size_t dn,fn;
    int ok=0,slash;
    if(!AslBase)return 0;
    fr=(struct FileRequester *)AllocAslRequestTags(
        ASL_FileRequest,ASL_FuncFlags,(ULONG)(destination?FILF_SAVE:0),
        ASLFR_Window,(ULONG)parent,
        title?ASLFR_TitleText:TAG_IGNORE,(ULONG)title,
        name?ASLFR_InitialFile:TAG_IGNORE,(ULONG)name,TAG_DONE);
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
int dolly_amiga_pick_file(struct Window *parent,DollyEndpoint *result,unsigned destination) {
    return pick_file(parent,result,destination,NULL,NULL);
}
int dolly_amiga_pick_titled_file(struct Window *parent,DollyEndpoint *result,const char *title) {
    return pick_file(parent,result,0,title,NULL);
}
int dolly_amiga_pick_named_file(struct Window *parent,DollyEndpoint *result,const char *name) {
    char title[312];
    snprintf(title,sizeof(title),"Locate image part: %s",name);
    return pick_file(parent,result,0,title,name);
}
int dolly_amiga_pick_titled_drawer(struct Window *parent,char path[256],const char *title) {
    struct FileRequester *fr;
    char selected[256];
    int ok=0;
    if(!AslBase)return 0;
    fr=(struct FileRequester *)AllocAslRequestTags(
        ASL_FileRequest,ASLFR_Flags2,(ULONG)FRF_DRAWERSONLY,
        ASLFR_Window,(ULONG)parent,
        ASLFR_TitleText,(ULONG)title,TAG_DONE);
    if(!fr)return 0;
    if(AslRequest(fr,NULL)) {
        BPTR lock=Lock(fr->fr_Drawer?fr->fr_Drawer:(STRPTR)"",ACCESS_READ);
        if(lock) {
            ok=NameFromLock(lock,(STRPTR)selected,sizeof(selected))!=0;
            if(ok)strcpy(path,selected);
            UnLock(lock);
        }
    }
    FreeAslRequest(fr);
    return ok;
}
int dolly_amiga_pick_drawer(struct Window *parent,char path[256]) {
    return dolly_amiga_pick_titled_drawer(parent,path,"Select recovery destination drawer");
}
static LONG picker_initial_width(struct Screen *screen,unsigned role) {
    struct RastPort measure=screen->RastPort;
    LONG labels,fields,header;
    SetFont(&measure,dolly_amiga_font(0));
    labels=picker_text_span(&measure,"ATA IDENTIFY",84);
    labels=picker_text_span(&measure,"SCSI name",labels);
    labels=picker_text_span(&measure,"Geometry",labels);
    labels=picker_text_span(&measure,"Partition",labels);
    fields=labels+44+picker_text_span(&measure,"Unit",38)+
           picker_number_span(&measure,10,58)+4+
           picker_text_span(&measure,"Query",80);
    header=picker_text_span(&measure,role?"Select Destination Medium":
                                         "Select Source Medium",230)+
           picker_text_span(&measure,"Physical device",0)+30;
    if(fields<header)fields=header;
    fields+=64; /* Horizontal margins and Intuition resize border. */
    return fields>606?fields:606;
}

int dolly_picker_open(DollyEndpointPicker *p,struct Screen *screen,
                      unsigned role,const DollyEndpoint *initial,
                      DollyEndpointMode preferred_mode,
                      unsigned allowed_mask) {
    LONG top,minheight,available,width;
    if(!p || !screen || role>1u ||
       (unsigned)preferred_mode>3u)return 0;
    if(p->window)return 1;
    memset(p,0,sizeof(*p));
    p->screen=screen;
    p->role=role;
    if(!set_allowed_modes(p,allowed_mask))return 0;
    if(initial) {
        p->result=*initial;
        if(initial->kind==DOLLY_ENDPOINT_FILE) {
            p->pending_file=*initial;
        }
    }
    p->mode=(initial && initial->kind!=DOLLY_ENDPOINT_NONE &&
             dolly_mode_allowed(p->allowed_mask,
                                dolly_endpoint_mode(initial))) ?
            dolly_endpoint_mode(initial) : preferred_mode;
    sync_mode_index(p);
    dolly_physical_catalog_snapshot(&p->physical_catalog);
    populate_devices(p);
    populate_units(p,1);
    (void)populate_partitions(p);
    dolly_amiga_discover_dos_devices(&p->dos_devices);
    p->logical_count=p->dos_devices.count;
    make_logical_list(p);
    if(initial && (initial->kind==DOLLY_ENDPOINT_UNIT ||
                   initial->kind==DOLLY_ENDPOINT_PARTITION)) {
        unsigned u;
        ULONG actual;
        char driver[DOLLY_PHYS_DRIVER_LEN];
        if(physical_mode(p) &&
           dolly_amiga_parse_unit(initial->path,driver,sizeof(driver),&actual)) {
            p->device=append_device(p,driver);
            p->has_device=1;
            p->manual_unit=actual;
            p->has_manual_unit=1;
            populate_units(p,1);
            for(u=0;u<p->unit_count;++u)
                if(p->physical_units[u]==actual) {
                    p->unit=u;p->has_unit=1;
                    break;
                }
        }
    }
    if(initial && initial->kind==DOLLY_ENDPOINT_LOGICAL &&
       initial->path[0]) {
        unsigned i;
        for(i=0;i<p->logical_count;++i) {
            if(strcmp(initial->path,p->dos_devices.names[i])==0) {
                p->logical=i;
                p->has_logical=1;
                break;
            }
        }
    }
    p->visual=GetVisualInfo(screen,TAG_DONE);
    if(!p->visual)return 0;
    top=(LONG)screen->BarHeight+1;
    /* Reserve room for taller fields, then fit inside the public screen. */
    minheight=(ULONG)screen->Width>2ul*(ULONG)screen->Height?
              214:231;
    available=(LONG)screen->Height-top;
    minheight+=6*(picker_row_height(dolly_amiga_font(0))-16);
    if(minheight>available)minheight=available;
    width=picker_initial_width(screen,role);
    if(width>screen->Width-12)width=screen->Width-12;
    p->window=OpenWindowTags(NULL,
          WA_PubScreen,(ULONG)screen,
          WA_Title,(ULONG)(role?"Dolly - Destination":"Dolly - Source"),
          WA_Left,12,WA_Top,(ULONG)top,
          WA_Width,(ULONG)width,WA_Height,(ULONG)minheight,
          WA_MinWidth,(ULONG)width,WA_MinHeight,(ULONG)minheight,
          WA_MaxWidth,(ULONG)screen->Width-12ul,
          WA_MaxHeight,(ULONG)screen->Height-(ULONG)top,
          WA_SizeGadget,TRUE,
          WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_CloseGadget,TRUE,
          WA_Activate,TRUE,WA_SmartRefresh,TRUE,
          WA_IDCMP,LISTVIEWIDCMP|CYCLEIDCMP|BUTTONIDCMP|
                   IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW|
                   IDCMP_NEWSIZE|IDCMP_SIZEVERIFY,
          TAG_DONE);
    if(!p->window)return 0;
    dolly_amiga_window_font(p->window,0);
    return rebuild(p);
}
void dolly_picker_refresh_font(DollyEndpointPicker *p) {
    if(!p->window)return;
    dolly_amiga_window_font(p->window,0);
    if(!rebuild(p))dolly_picker_close(p);
}

void dolly_picker_close(DollyEndpointPicker *p) {
    if(!p)return;
    if(p->window){CloseWindow(p->window);p->window=NULL;}
    p->gadgets_detached=0;
    if(p->gadgets){FreeGadgets(p->gadgets);p->gadgets=NULL;}
    if(p->visual){FreeVisualInfo(p->visual);p->visual=NULL;}
}
/* All commit methods (button or list double-click) use one path.
 * changed/result deliberately survive dolly_picker_close(), so the
 * main event loop can copy the endpoint after the chooser disappears.
 */
static int capture_partition_number(DollyEndpointPicker *p) {
    const struct StringInfo *info;
    const char *digit;
    unsigned number=0;
    if(!p->partition_number || !p->partition_scanned || !p->partition_count)
        return 0;
    info=(const struct StringInfo *)p->partition_number->SpecialInfo;
    digit=(const char *)info->Buffer;
    for(;*digit>='0' && *digit<='9';++digit)
        number=number*10u+(unsigned)(*digit-'0');
    if(*digit || number==0 || number>p->partition_count) {
        DisplayBeep(p->screen);
        return 0;
    }
    p->partition=number-1u;
    p->has_partition=1;
    GT_SetGadgetAttrs(p->partition_list,p->window,NULL,
                      GTLV_Selected,(ULONG)p->partition,
                      GTLV_MakeVisible,(ULONG)p->partition,TAG_DONE);
    refresh_partition_details(p);
    return 1;
}
static int commit_picker(DollyEndpointPicker *p) {
    int ok=0;
    DollyOutsideRange policy=DOLLY_RANGE_STRICT;
    if(physical_mode(p)) {
        ULONG unit;
        const char *driver=read_device_field(p);
        if(!physical_choice(p) || !driver ||
           strcasecmp(driver,p->physical_names[p->device]) ||
           !read_unit_field(p,&unit) ||
           unit!=p->physical_units[p->unit]) {
            DisplayBeep(p->screen);
            return 0;
        }
    }
    if(p->mode==DOLLY_MODE_PARTITION) {
        if(!capture_partition_number(p))return 0;
        if(p->partition_map.parts[p->partition].outside_media) {
            const DollyNativePartition *part=&p->partition_map.parts[p->partition];
            char driver_last[21],part_last[21],sectors[21],size[32],reason[400];
            if(p->role || !dolly_amiga_allow_outside_range()) {
                change_enabled(p);
                return 0;
            }
            partition_gap(p,sectors,size);
            dolly_surface_log_decimal(driver_last,p->media_geometry.block_count-1u);
            dolly_surface_log_decimal(part_last,(part->start_bytes+part->length_bytes)/
                                      p->media_geometry.block_bytes-1u);
            snprintf(reason,sizeof(reason),
                "Partition %s extends beyond the driver-reported range.\n"
                "Last driver LBA: %s. Last RDB partition LBA: %s.\n"
                "Outside reported range: %s sectors (%s).\n"
                "Fill gap with 0s substitutes zeros for the missing data.\n"
                "Use as-is attempts a read and may fail or return incorrect data.",
                part->label,driver_last,part_last,sectors,size);
            policy=(DollyOutsideRange)dolly_amiga_confirm_outside_range(p->window,reason);
            if(policy==DOLLY_RANGE_STRICT)return 0;
        }
    }
    if(p->mode==DOLLY_MODE_PHYSICAL && physical_choice(p) &&
       p->queried && p->media_geometry.capacity_valid) {
        ok=dolly_endpoint_set_unit(&p->result,p->device,p->unit);
        if(ok)sprintf(p->result.path,"%s / Unit %lu",
                      p->physical_names[p->device],
                      (unsigned long)p->physical_units[p->unit]);
    }
    else if(p->mode==DOLLY_MODE_PARTITION &&
            physical_choice(p) && p->queried &&
            p->partition_scanned && p->has_partition &&
            p->partition<p->partition_count) {
        ok=dolly_endpoint_set_partition(&p->result,p->device,
                                        p->unit,p->partition);
        if(ok) {
            const DollyNativePartition *part=&p->partition_map.parts[p->partition];
            sprintf(p->result.path,"%s / Unit %lu / %s",
                    p->physical_names[p->device],
                    (unsigned long)p->physical_units[p->unit],part->label);
            p->result.partition_start_bytes=part->start_bytes;
            p->result.partition_length_bytes=part->length_bytes;
            p->result.outside_range=policy;
            if(policy!=DOLLY_RANGE_STRICT)
                p->result.confirmed_capacity_bytes=p->media_geometry.block_count*
                                                    p->media_geometry.block_bytes;
        }
    }
    else if(p->mode==DOLLY_MODE_LOGICAL) {
        const struct StringInfo *info=p->selection_text->SpecialInfo;
        const char *name=(const char *)info->Buffer;
        size_t length=strlen(name);
        unsigned i;
        if(length && name[length-1u]==':')--length;
        for(i=0;i<p->logical_count;++i) {
            const char *registered=p->dos_devices.names[i];
            size_t registered_length=strlen(registered);
            if(registered_length && registered[registered_length-1u]==':')
                --registered_length;
            if(length && length==registered_length && !strncasecmp(name,registered,length)) {
                ok=dolly_endpoint_set_logical(&p->result,i);
                if(ok)strcpy(p->result.path,registered);
                break;
            }
        }
        if(!ok)DisplayBeep(p->screen);
    } else if(p->mode==DOLLY_MODE_FILE) {
        capture_file_text(p);
        if(p->pending_file.kind==DOLLY_ENDPOINT_FILE) {
            p->result=p->pending_file;
            ok=1;
        } else DisplayBeep(p->screen);
    }
    if(ok)p->changed=1;
    return ok;
}
/* Release GadTools' references to LISTVIEW node chains BEFORE
 * repopulating lists. Removing gadgets from the Window alone does
 * not terminate the references held by the LISTVIEW objects.
 * This helper is invoked after all queued IDCMP messages are replied.
 */
static void release_picker_gadgets(DollyEndpointPicker *p) {
    if(!p->gadgets)return;
    if(!p->gadgets_detached)
        RemoveGList(p->window,p->gadgets,-1);
    FreeGadgets(p->gadgets);
    p->gadgets=NULL;
    p->file_text=NULL;
    p->gadgets_detached=1;
}
static void query_picker_unit(DollyEndpointPicker *p) {
    p->query_attempted=1;
    p->queried=(unsigned char)dolly_amiga_device_query(
          p->physical_names[p->device],
          p->physical_units[p->unit],
          p->disk_info_display,sizeof(p->disk_info_display),
          p->disk_size_display,sizeof(p->disk_size_display),
          &p->media_geometry,0,p->ata_identify,sizeof(p->ata_identify));
    if(p->mode!=DOLLY_MODE_PARTITION)return;
    p->has_partition=0;
    p->partition_scanned=0;
    p->partition_count=0;
    if(p->queried && p->media_geometry.capacity_valid) {
        p->partition_scanned=(unsigned char)
            dolly_amiga_partition_scan(
                p->physical_names[p->device],
                p->physical_units[p->unit],
                &p->media_geometry,
                &p->partition_map);
        if(p->partition_scanned)
            p->partition_count=p->partition_map.count;
    } else {
        strcpy(p->partition_map.status,
               "Capacity unavailable; no partition scan");
    }
    if(!populate_partitions(p)) {
        p->partition_count=0;
        p->partition_scanned=0;
        strcpy(p->partition_map.status,"Too many partitions");
        (void)populate_partitions(p);
    }
}
/* Rebuild ONLY after draining and replying to all GadTools messages.
 * V39 GTLV_ShowSelected makes list gadgets retain extra selection state;
 * destroying/relinking an active LISTVIEW during its GADGETUP handler
 * can leave old gadget/list pointers in pending messages.
 */
void dolly_picker_pump(DollyEndpointPicker *p) {
    struct IntuiMessage *msg;
    enum {CHANGE_NONE,CHANGE_MODE,CHANGE_DEVICE,CHANGE_UNIT,RUN_QUERY,SELECT_SOURCE_FILE,
          MANUAL_UNIT,MANUAL_UNIT_QUERY,MANUAL_DEVICE,MANUAL_DEVICE_QUERY};
    unsigned deferred=CHANGE_NONE;
    char pending_driver[DOLLY_PHYS_DRIVER_LEN]={0};
    if(!p || !p->window)return;
    while((msg=GT_GetIMsg(p->window->UserPort))!=NULL) {
        ULONG cls=msg->Class;
        UWORD id=0,code=msg->Code;
        int close=0;
        if(cls==IDCMP_GADGETUP && msg->IAddress)
            id=((struct Gadget *)msg->IAddress)->GadgetID;
        /* All messages still refer to the existing gadget context.
         * Drain and reply without acting on further, now-stale choices.
         */
        if(deferred) {
            GT_ReplyIMsg(msg);
            continue;
        }
        if(cls==IDCMP_SIZEVERIFY && p->gadgets &&
           !p->gadgets_detached) {
            RemoveGList(p->window,p->gadgets,-1);
            p->gadgets_detached=1;
        }
        if(cls==IDCMP_REFRESHWINDOW) {
            GT_BeginRefresh(p->window);
            GT_EndRefresh(p->window,TRUE);
        }
        GT_ReplyIMsg(msg);
        if(cls==IDCMP_NEWSIZE) {
            if(!rebuild(p))dolly_picker_close(p);
            return;
        }
        if(cls==IDCMP_REFRESHWINDOW && !p->gadgets_detached) {
            clear_picker_client(p);
            RefreshGList(p->gadgets,p->window,NULL,-1);
            GT_RefreshWindow(p->window,NULL);
            captions(p);
        }
        if(cls==IDCMP_CLOSEWINDOW || id==PICK_CANCEL)close=1;
        else if(id==PICK_MODE && (unsigned)code<p->mode_count) {
            if(!p->role && AslBase && p->allowed_mode_ids[code]==DOLLY_MODE_FILE) {
                deferred=SELECT_SOURCE_FILE;
                continue;
            }
            capture_file_text(p);
            p->mode_index=code;
            p->mode=p->allowed_mode_ids[code];
            p->has_device=0;p->has_unit=0;p->has_manual_unit=0;
            p->has_partition=0;p->has_logical=0;
            deferred=CHANGE_MODE;
        } else if(id==PICK_DEVICE && (unsigned)code<p->device_count) {
            if(!p->has_device || p->device!=code)p->has_manual_unit=0;
            p->device=code;p->has_device=1;
            p->has_unit=0;p->has_partition=0;
            deferred=CHANGE_DEVICE;
        } else if(id==PICK_UNIT &&
                  (unsigned)code<p->unit_count && p->has_device) {
            p->unit=code;p->has_unit=1;
            deferred=CHANGE_UNIT;
        } else if(id==PICK_PARTITION &&
                  p->mode==DOLLY_MODE_PARTITION &&
                  (unsigned)code<p->partition_count &&
                  p->partition_scanned && p->queried) {
            p->partition=code;p->has_partition=1;
            change_enabled(p);
            refresh_selection_preview(p);
            refresh_partition_details(p);
        } else if(id==PICK_LOGICAL && (unsigned)code<p->logical_count) {
            p->logical=code;p->has_logical=1;
            change_enabled(p);
            refresh_selection_preview(p);
        } else if((id==PICK_QUERY || id==PICK_UNIT_TEXT ||
                   id==PICK_SELECTION_TEXT) && physical_mode(p)) {
            const char *driver=read_device_field(p);
            ULONG unit=0;
            int have_unit=read_unit_field(p,&unit);
            if(!driver)DisplayBeep(p->screen);
            else if(!p->has_device || strcasecmp(driver,p->physical_names[p->device])) {
                strcpy(pending_driver,driver);
                p->has_manual_unit=id==PICK_QUERY && have_unit;
                if(p->has_manual_unit)p->manual_unit=unit;
                deferred=p->has_manual_unit?MANUAL_DEVICE_QUERY:MANUAL_DEVICE;
            } else if(id!=PICK_SELECTION_TEXT) {
                if(!have_unit)DisplayBeep(p->screen);
                else if(!physical_choice(p) || unit!=p->physical_units[p->unit]) {
                    p->manual_unit=unit;
                    p->has_manual_unit=1;
                    deferred=id==PICK_QUERY?MANUAL_UNIT_QUERY:MANUAL_UNIT;
                } else if(id==PICK_QUERY)deferred=RUN_QUERY;
            }
        } else if(id==PICK_PARTITION_NUMBER) {
            (void)capture_partition_number(p);
            change_enabled(p);
        } else if(id==PICK_SELECT ||
                  (id==PICK_SELECTION_TEXT && p->mode==DOLLY_MODE_LOGICAL)) {
            close=commit_picker(p);
        } else if(id==PICK_FILE_TEXT && p->mode==DOLLY_MODE_FILE) {
            capture_file_text(p);
            file_details(p);
            if(p->file_info)
                GT_SetGadgetAttrs(p->file_info,p->window,NULL,
                                  GTTX_Text,(ULONG)p->file_details,TAG_DONE);
        } else if(id==PICK_FILE && p->mode==DOLLY_MODE_FILE) {
            DollyEndpoint selected={0};
            if(dolly_amiga_pick_file(p->window,&selected,p->role)) {
                p->pending_file=selected;
                file_details(p);
                if(p->file_info)
                    GT_SetGadgetAttrs(p->file_info,p->window,NULL,
                                      GTTX_Text,(ULONG)p->file_details,
                                      TAG_DONE);
                if(p->file_text)
                    GT_SetGadgetAttrs(p->file_text,p->window,NULL,
                                      GTST_String,(ULONG)p->pending_file.path,
                                      TAG_DONE);
                change_enabled(p);
            }
        }
        if(close){dolly_picker_close(p);return;}
    }
    if(deferred && p->window) {
        /* No IntuiMessage can refer to the old gadgets after this
         * point. Detach BEFORE changing list nodes and label buffers.
         */
        if(deferred==SELECT_SOURCE_FILE) {
            if(dolly_amiga_pick_file(p->window,&p->result,0)) {
                p->changed=1;
                dolly_picker_close(p);
            } else if(p->mode_cycle)
                GT_SetGadgetAttrs(p->mode_cycle,p->window,NULL,
                                  GTCY_Active,(ULONG)p->mode_index,TAG_DONE);
            return;
        }
        if(deferred==RUN_QUERY) {
            if(p->partition_list)
                GT_SetGadgetAttrs(p->partition_list,p->window,NULL,
                                  GTLV_Labels,(ULONG)~0ul,TAG_DONE);
            query_picker_unit(p);
            if(p->partition_list)
                GT_SetGadgetAttrs(p->partition_list,p->window,NULL,
                                  GTLV_Labels,(ULONG)&p->partition_lists[p->partition_bank],
                                  GTLV_Selected,(ULONG)~0ul,TAG_DONE);
            refresh_disk_info(p);
            refresh_selection_preview(p);
            refresh_partition_details(p);
            change_enabled(p);
            return;
        }
        release_picker_gadgets(p);
        if(deferred==MANUAL_DEVICE || deferred==MANUAL_DEVICE_QUERY) {
            p->device=append_device(p,pending_driver);
            p->has_device=1;
            p->has_unit=0;
        }
        if(deferred==CHANGE_MODE || deferred==CHANGE_DEVICE || deferred==MANUAL_DEVICE) {
            invalidate_query(p);
            populate_units(p,1);
        } else if(deferred==CHANGE_UNIT) {
            invalidate_query(p);
        } else if(deferred==MANUAL_UNIT || deferred==MANUAL_UNIT_QUERY ||
                  deferred==MANUAL_DEVICE_QUERY) {
            unsigned i;
            invalidate_query(p);
            populate_units(p,deferred==MANUAL_DEVICE_QUERY);
            for(i=0;i<p->unit_count;++i)
                if(p->physical_units[i]==p->manual_unit) {
                    p->unit=i;p->has_unit=1;
                    break;
                }
            if(deferred!=MANUAL_UNIT)query_picker_unit(p);
        }
        if(!rebuild(p))
            dolly_picker_close(p);
    }
}
