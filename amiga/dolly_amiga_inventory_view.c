/* Sheep Inventory: device snapshot and explicit RDB backup/restore.
 * Only the selected device/unit from the established picker is queried.
 * Registered AmigaDOS entries do not guarantee usable media.
 */
#include "dolly_amiga_inventory_view.h"
#include "dolly_amiga_device_query.h"
#include "dolly_amiga_clipboard.h"
#include "dolly_amiga_destructive.h"
#include "dolly_amiga_physical_catalog.h"
#include <exec/types.h>
#include <dos/dosextens.h>
#include <libraries/asl.h>
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <stdio.h>
#include <string.h>

#define INV_CLOSE_ID 601u
#define INV_PART_ID 602u
#define INV_BACKUP_ID 603u
#define INV_RESTORE_ID 604u
#define INV_DETAIL_ID 605u
#define INV_HEADER_ID 606u
#define INV_COPY_MENU FULLMENUNUM(0,0,NOSUB)
#define INV_MAX_PARTITIONS DOLLY_PART_MAX
#define INV_WIDTH 606
#define INV_MIN_WIDTH 504
#define INV_HEIGHT 246
#define INV_MIN_HEIGHT 230
#define INV_MARGIN 16
#define INV_LIST_TOP 65
#define INV_BUTTON_WIDTH 136
#define INV_STATUS_HEIGHT 16
extern struct Library *AslBase;
extern struct DosLibrary *DOSBase;

static LONG list_width(const DollyNativeInventoryView *v) {
    const struct Window *w=v->window;
    LONG width=(LONG)w->Width-w->BorderLeft-w->BorderRight-2*INV_MARGIN;
    return v->map.format==DOLLY_NATIVE_PARTITION_MBR?width/2:width*2/5;
}

static LONG details_x(const DollyNativeInventoryView *v) {
    return (LONG)v->window->BorderLeft+INV_MARGIN+list_width(v);
}

static LONG client_bottom(const struct Window *w) {
    return (LONG)w->Height-w->BorderBottom-1;
}
static LONG status_top(const struct Window *w) {
    return client_bottom(w)-18-4-INV_STATUS_HEIGHT;
}
static void format_chs_number(char *text,ULONG value) {
    if(value)sprintf(text,"%lu",(unsigned long)value);
    else strcpy(text,"?");
}

static void init_list(struct List *list) {
    list->lh_Head=(struct Node *)&list->lh_Tail;
    list->lh_Tail=NULL;
    list->lh_TailPred=(struct Node *)&list->lh_Head;
    list->lh_Type=NT_USER;
    list->l_pad=0;
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
static void populate_header(DollyNativeInventoryView *v) {
    unsigned i;
    init_list(&v->header_rows);
    if(v->driver[0])
        sprintf(v->header_text[0],"Driver: %s  |  Unit %lu",
                v->driver,(unsigned long)v->unit);
    else strcpy(v->header_text[0],"Driver: Unavailable");
    sprintf(v->header_text[1],"Device Name: %s",v->device_info);
    sprintf(v->header_text[2],"Geometry: %s",v->chs_info);
    sprintf(v->header_text[3],"Capacity: %s",v->geometry_info);
    for(i=0;i<DOLLY_INVENTORY_HEADER_ROWS;++i) {
        v->header_nodes[i].ln_Name=v->header_text[i];
        AddTail(&v->header_rows,&v->header_nodes[i]);
    }
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
static struct Gadget *create_button(struct Gadget *previous,
                                    DollyNativeInventoryView *v,
                                    struct Screen *screen,LONG x,LONG y,
                                    LONG width,UWORD id,STRPTR label,
                                    ULONG disabled) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=v->visual;
    ng.ng_TextAttr=screen->Font;
    ng.ng_LeftEdge=(WORD)x;ng.ng_TopEdge=(WORD)y;
    ng.ng_Width=(WORD)width;ng.ng_Height=16;
    ng.ng_GadgetID=id;
    ng.ng_GadgetText=label;
    return CreateGadget(BUTTON_KIND,previous,&ng,GA_Disabled,disabled,TAG_DONE);
}
static struct Gadget *create_list(struct Gadget *previous,
                                  DollyNativeInventoryView *v,
                                  struct Screen *screen,
                                  LONG x,LONG y,LONG w,LONG h,
                                  UWORD id,struct List *rows,
                                  unsigned selection,unsigned count) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=v->visual;
    ng.ng_TextAttr=screen->Font;
    ng.ng_LeftEdge=(WORD)x;ng.ng_TopEdge=(WORD)y;
    ng.ng_Width=(WORD)w;ng.ng_Height=(WORD)h;
    ng.ng_GadgetID=id;
    ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(LISTVIEW_KIND,previous,&ng,
                        GTLV_Labels,(ULONG)rows,
                        id==INV_HEADER_ID?TAG_IGNORE:GTLV_ShowSelected,(ULONG)NULL,
                        id==INV_HEADER_ID?TAG_IGNORE:GTLV_Selected,(ULONG)selection,
                        GTLV_Top,(ULONG)(selection<count?selection:0u),
                        TAG_DONE);
}
static void draw_text(struct Window *w,struct DrawInfo *info,
                      LONG x,LONG y,LONG right,const char *s) {
    struct RastPort *rp=w->RPort;
    ULONG length=(ULONG)strlen(s);
    LONG width=right-x;
    while(length && TextLength(rp,(STRPTR)s,length)>width)
        --length;
    if(!length)return;
    SetDrMd(rp,JAM1);
    SetAPen(rp,info->dri_Pens[TEXTPEN]);
    Move(rp,x,y);
    Text(rp,(STRPTR)s,length);
}
static const char *mbr_type_name(UBYTE type) {
    switch(type) {
    case 0x01: return "FAT12";
    case 0x04: return "FAT16 (<32 MiB)";
    case 0x05: return "Extended (CHS)";
    case 0x06: return "FAT16";
    case 0x07: return "HPFS / NTFS / exFAT";
    case 0x0b: return "FAT32";
    case 0x0c: return "FAT32 (LBA)";
    case 0x0e: return "FAT16 (LBA)";
    case 0x0f: return "Extended (LBA)";
    case 0x11: return "Hidden FAT12";
    case 0x14: return "Hidden FAT16 (<32 MiB)";
    case 0x16: return "Hidden FAT16";
    case 0x17: return "Hidden HPFS / NTFS";
    case 0x1b: return "Hidden FAT32";
    case 0x1c: return "Hidden FAT32 (LBA)";
    case 0x1e: return "Hidden FAT16 (LBA)";
    case 0x27: return "Windows recovery";
    case 0x42: return "Windows dynamic disk";
    case 0x76: return "Amiga RDB container";
    case 0x82: return "Linux swap / Solaris";
    case 0x83: return "Linux filesystem";
    case 0x85: return "Linux extended";
    case 0x8e: return "Linux LVM";
    case 0xa5: return "FreeBSD";
    case 0xa6: return "OpenBSD";
    case 0xa9: return "NetBSD";
    case 0xab: return "Apple boot";
    case 0xaf: return "Apple HFS / HFS+";
    case 0xef: return "EFI System Partition";
    case 0xfd: return "Linux RAID";
    default:   return "Unknown";
    }
}
static const char *mbr_role(const DollyNativePartition *part) {
    if(!part->mbr_container)return "Primary";
    return part->mbr_type==0x76u?"RDB container":"Extended";
}
static void dos_type_ascii(char ascii[17],ULONG type) {
    unsigned i,used=0;
    for(i=0;i<4u;++i) {
        unsigned byte=(unsigned)((type>>(24u-8u*i))&0xffu);
        if(byte>=32u && byte<=126u && byte!='\\')
            ascii[used++]=(char)byte;
        else if(byte<10u) {
            ascii[used++]='\\';
            ascii[used++]=(char)('0'+byte);
        } else {
            static const char hex[]="0123456789ABCDEF";
            ascii[used++]='\\';
            ascii[used++]='x';
            ascii[used++]=hex[byte>>4u];
            ascii[used++]=hex[byte&15u];
        }
    }
    ascii[used]='\0';
}
static const char *rdb_filesystem_name(ULONG type) {
    switch(type) {
    case 0x444f5300ul: return "OldFileSystem";
    case 0x444f5301ul: return "FastFileSystem";
    case 0x444f5302ul: return "OldFileSystem (international)";
    case 0x444f5303ul: return "FastFileSystem (international)";
    case 0x444f5304ul: return "OldFileSystem (directory cache)";
    case 0x444f5305ul: return "FastFileSystem (directory cache)";
    case 0x444f5306ul: return "OldFileSystem (long filenames)";
    case 0x444f5307ul: return "FastFileSystem (long filenames)";
    case 0x53465300ul: return "SmartFileSystem";
    case 0x53465302ul: return "SmartFileSystem 2";
    case 0x50465300ul:
    case 0x50465301ul: return "ProfessionalFileSystem";
    case 0x50465302ul: return "ProfessionalFileSystem 2";
    case 0x50465303ul: return "ProfessionalFileSystem 3";
    case 0x50445300ul:
    case 0x50445301ul:
    case 0x50445302ul: return "ProfessionalFileSystem (direct SCSI)";
    case 0x50445303ul: return "ProfessionalFileSystem 3 (SCSI)";
    case 0x4d534400ul: return "CrossDOS (MS-DOS FAT)";
    default: return "Unknown filesystem type";
    }
}
/* Right-hand partition details reuse the already validated snapshot.
 * Selecting a LISTVIEW row never sends a device I/O request.
 */
static void add_detail(DollyNativeInventoryView *v,const char *text) {
    unsigned i=v->detail_count;
    struct Node *node;
    if(i>=DOLLY_INVENTORY_DETAILS)return;
    node=&v->detail_nodes[i];
    memset(node,0,sizeof(*node));
    copy_text(v->detail_text[i],sizeof(v->detail_text[i]),text);
    node->ln_Name=v->detail_text[i];
    AddTail(&v->details,node);
    ++v->detail_count;
}
static void populate_details(DollyNativeInventoryView *v) {
    const DollyNativePartition *p;
    char line[DOLLY_INVENTORY_DETAIL_BYTES];
    init_list(&v->details);
    v->detail_count=0;
    v->selected_detail=~0u;
    if(v->selected_partition>=v->map.count)return;
    p=&v->map.parts[v->selected_partition];
    sprintf(line,"Partition: %s",p->label);
    add_detail(v,line);
    if(p->is_rdb) {
        if(p->dos_type_present) {
            char ascii[17];
            dos_type_ascii(ascii,p->dos_type);
            sprintf(line,"DosType: 0x%08lx  %s",(unsigned long)p->dos_type,ascii);
        } else strcpy(line,"DosType: unavailable");
        add_detail(v,line);
        add_detail(v,p->dos_type_present?
                  rdb_filesystem_name(p->dos_type):"Filesystem type unavailable");
        if(p->boot_priority_present)
            sprintf(line,"BootPri: %ld",(long)p->boot_priority);
        else strcpy(line,"BootPri: unavailable");
        add_detail(v,line);
        sprintf(line,"Bootable: %s | Automount: %s",
                p->rdb_bootable?"Yes":"No",p->rdb_automount?"Yes":"No");
        add_detail(v,line);
        if(p->max_transfer_present)
            sprintf(line,"MaxTransfer: 0x%08lx",
                    (unsigned long)p->max_transfer);
        else strcpy(line,"MaxTransfer: unavailable");
        add_detail(v,line);
        if(p->dma_mask_present)
            sprintf(line,"Mask: 0x%08lx",(unsigned long)p->dma_mask);
        else strcpy(line,"Mask: unavailable");
        add_detail(v,line);
        sprintf(line,"FS block: %lu B, SPB: %lu",
                (unsigned long)p->fs_block_bytes,
                (unsigned long)p->sectors_per_block);
        add_detail(v,line);
        sprintf(line,"Cylinders: %lu - %lu",
                (unsigned long)p->low_cyl,
                (unsigned long)p->high_cyl);
        add_detail(v,line);
    } else {
        unsigned long first=(unsigned long)(p->start_bytes/512u);
        unsigned long count=(unsigned long)(p->length_bytes/512u);
        sprintf(line,"Role: %s | Active: %s",mbr_role(p),p->mbr_active?"Yes":"No");
        add_detail(v,line);
        sprintf(line,"Type 0x%02lx: %s",(unsigned long)p->mbr_type,
                mbr_type_name(p->mbr_type));
        add_detail(v,line);
        sprintf(line,"First LBA: %lu",first);
        add_detail(v,line);
        sprintf(line,"Last LBA: %lu",first+(count-1u));
        add_detail(v,line);
        sprintf(line,"Sectors: %lu",count);
        add_detail(v,line);
        add_detail(v,p->mbr_container?
                  "Contents not scanned":"Type declared by MBR");
    }
    if(p->length_bytes<1048576u)
        sprintf(line,"Size: %lu B",(unsigned long)p->length_bytes);
    else
        sprintf(line,"Size: %lu MiB",
                (unsigned long)(p->length_bytes/1048576u));
    add_detail(v,line);
}
static void paint_status(DollyNativeInventoryView *v,struct DrawInfo *info) {
    struct Window *w=v->window;
    LONG left=(LONG)w->BorderLeft+INV_MARGIN;
    LONG right=(LONG)w->Width-w->BorderRight-INV_MARGIN;
    LONG y=status_top(w);
    struct TagItem tags[]={
        {GT_VisualInfo,(ULONG)v->visual},
        {GTBB_Recessed,TRUE},
        {TAG_DONE,0}
    };
    SetDrMd(w->RPort,JAM1);
    SetAPen(w->RPort,info->dri_Pens[BACKGROUNDPEN]);
    RectFill(w->RPort,left,y,right-1,y+INV_STATUS_HEIGHT-1);
    DrawBevelBoxA(w->RPort,left,y,right-left,INV_STATUS_HEIGHT,tags);
    draw_text(w,info,left+6,y+11,right-6,v->status);
}
static void paint(DollyNativeInventoryView *v,struct Screen *screen) {
    struct DrawInfo *info;
    struct Window *w=v->window;
    LONG top,left;
    char line[88];
    if(!w)return;
    info=GetScreenDrawInfo(screen);
    if(!info)return;
    top=(LONG)w->BorderTop;
    left=(LONG)w->BorderLeft+INV_MARGIN;
    sprintf(line,"%lu %sPartitions:",(unsigned long)v->count,
            v->map.format==DOLLY_NATIVE_PARTITION_MBR?"MBR ":
            v->map.format==DOLLY_NATIVE_PARTITION_RDB?"RDB ":"");
    draw_text(w,info,left,top+60,details_x(v)-4,line);
    if(v->count)
        draw_text(w,info,details_x(v),top+60,w->Width-w->BorderRight-INV_MARGIN,
                  v->detail_count?"Partition details":"Select a partition");
    if(!v->count)
        draw_text(w,info,left+4,top+78,w->Width-w->BorderRight-INV_MARGIN,
                  "No partitions to display");
    paint_status(v,info);
    FreeScreenDrawInfo(screen,info);
}
static int path_is_unused(STRPTR path) {
    BPTR lock=Lock(path,ACCESS_READ);
    if(lock) {
        UnLock(lock);
        return 0;
    }
    return IoErr()==ERROR_OBJECT_NOT_FOUND;
}
static void set_status(DollyNativeInventoryView *v,const char *message) {
    struct Window *w=v->window;
    struct DrawInfo *info=GetScreenDrawInfo(w->WScreen);
    copy_text(v->status,sizeof(v->status),message);
    if(!info)return;
    paint_status(v,info);
    FreeScreenDrawInfo(w->WScreen,info);
}
static void copy_selected_row(DollyNativeInventoryView *v) {
    if(v->clipboard_row)
        set_status(v,dolly_amiga_copy_text(v->clipboard_row)?
                   "Copied to Clipboard":"Clipboard copy failed");
}
static int backup_available(const DollyNativeInventoryView *v) {
    return AslBase && (v->map.mbr_backup_available ||
                       v->map.rdb_backup_blocks!=0);
}
static int restore_available(const DollyNativeInventoryView *v) {
    return AslBase && v->verified && v->map.format==DOLLY_NATIVE_PARTITION_RDB &&
           v->map.rdb_backup_blocks>=16 &&
           v->map.rdb_backup_blocks<=DOLLY_RDB_RESTORE_MAX_BLOCKS &&
           strcmp(v->driver,"scsi.device")==0;
}
static int rebuild_gadgets(DollyNativeInventoryView *v,int details_changed) {
    struct Window *w=v->window;
    struct Screen *screen=w->WScreen;
    struct Gadget *last;
    struct DrawInfo *info;
    LONG left=(LONG)w->BorderLeft+INV_MARGIN;
    LONG right=(LONG)w->Width-w->BorderRight-INV_MARGIN;
    LONG bottom=client_bottom(w);
    LONG list_top=(LONG)w->BorderTop+INV_LIST_TOP;
    LONG list_height=status_top(w)-5-list_top;
    LONG button_top=bottom-18;
    LONG button_width=TextLength(w->RPort,(STRPTR)"Restore RDB",11)+16;
    LONG backup_width=TextLength(w->RPort,(STRPTR)"Backup MBR",10)+16;
    if(button_width<backup_width)button_width=backup_width;
    if(button_width<INV_BUTTON_WIDTH)button_width=INV_BUTTON_WIDTH;
    if(button_width>(right-left-12)/3)button_width=(right-left-12)/3;
    if(v->gadgets) {
        RemoveGList(w,v->gadgets,-1);
        FreeGadgets(v->gadgets);
        v->gadgets=NULL;
    }
    if(details_changed) {
        if(v->selected_detail<v->detail_count &&
           v->clipboard_row==v->detail_text[v->selected_detail]) {
            v->clipboard_row=NULL;
            if(v->menu)OffMenu(w,INV_COPY_MENU);
        }
        /* GadTools no longer holds these nodes, so they can be reused. */
        populate_details(v);
    }
    last=CreateContext(&v->gadgets);
    if(!last)return 0;
    last=create_list(last,v,screen,left,(LONG)w->BorderTop+5,right-left,46,
                     INV_HEADER_ID,&v->header_rows,~0u,DOLLY_INVENTORY_HEADER_ROWS);
    if(!last)return 0;
    if(v->count) {
        LONG x=details_x(v);
        last=create_list(last,v,screen,left,list_top,list_width(v),list_height,
                         INV_PART_ID,&v->rows,v->selected_partition,v->count);
        if(!last)return 0;
        last=create_list(last,v,screen,x,list_top,right-x,list_height,
                         INV_DETAIL_ID,&v->details,v->selected_detail,v->detail_count);
        if(!last)return 0;
    }
    {
        int mbr=v->map.format==DOLLY_NATIVE_PARTITION_MBR;
        last=create_button(last,v,screen,left,button_top,button_width,
                           INV_BACKUP_ID,(STRPTR)(mbr?"Backup MBR":"Backup RDB"),
                           (ULONG)!backup_available(v));
        if(!last)return 0;
        last=create_button(last,v,screen,(left+right-button_width)/2,
                           button_top,button_width,INV_RESTORE_ID,
                           (STRPTR)"Restore RDB",FALSE);
        if(!last)return 0;
    }
    last=create_button(last,v,screen,right-button_width,button_top,button_width,
                       INV_CLOSE_ID,(STRPTR)"Close",FALSE);
    if(!last)return 0;
    info=GetScreenDrawInfo(screen);
    if(info) {
        SetDrMd(w->RPort,JAM1);
        SetAPen(w->RPort,info->dri_Pens[BACKGROUNDPEN]);
        RectFill(w->RPort,w->BorderLeft,w->BorderTop,
                 (LONG)w->Width-w->BorderRight-1,bottom);
        FreeScreenDrawInfo(screen,info);
    }
    /* Smart refresh can retain pixels from the old footer in the frame. */
    RefreshWindowFrame(w);
    AddGList(w,v->gadgets,(ULONG)-1,-1,NULL);
    RefreshGList(v->gadgets,w,NULL,-1);
    GT_RefreshWindow(w,NULL);
    paint(v,screen);
    return 1;
}
static void backup_partition_table(DollyNativeInventoryView *v) {
    struct FileRequester *fr;
    char path[256],partial[sizeof(path)+sizeof(".part")-1u];
    const char *drawer,*file;
    BPTR handle;
    int ok;
    int mbr=v->map.format==DOLLY_NATIVE_PARTITION_MBR;
    if(!backup_available(v))return;
    fr=(struct FileRequester *)AllocAslRequestTags(
        ASL_FileRequest,ASL_FuncFlags,(ULONG)FILF_SAVE,
        ASLFR_Window,(ULONG)v->window,
        ASLFR_TitleText,(ULONG)(mbr?"Save MBR backup":"Save RDB backup"),
        ASLFR_InitialFile,(ULONG)(mbr?"dolly-mbr.bin":"dolly-rdb.bin"),TAG_DONE);
    if(!fr) {
        set_status(v,"Could not open save requester");
        return;
    }
    if(!AslRequest(fr,NULL)) {
        FreeAslRequest(fr);
        return;
    }
    drawer=fr->fr_Drawer?(const char *)fr->fr_Drawer:"";
    file=(const char *)fr->fr_File;
    copy_text(path,sizeof(path),drawer);
    ok=strlen(drawer)<sizeof(path) && file && *file &&
       !strpbrk(file,":/") && AddPart((STRPTR)path,(STRPTR)file,sizeof(path));
    FreeAslRequest(fr);
    if(!ok) {
        set_status(v,"Invalid backup filename or drawer");
        return;
    }
    if(!path_is_unused((STRPTR)path)) {
        set_status(v,"Destination exists or inaccessible");
        return;
    }
    strcpy(partial,path);
    strcat(partial,".part");
    if(!path_is_unused((STRPTR)partial)) {
        set_status(v,"Temporary backup file unavailable");
        return;
    }
    handle=Open((STRPTR)partial,MODE_NEWFILE);
    if(!handle) {
        set_status(v,"Could not create backup file");
        return;
    }
    ok=dolly_amiga_partition_backup(v->driver,v->unit,&v->geometry,
                                   v->map.format,handle);
    if(!Close(handle))ok=0;
    if(ok && path_is_unused((STRPTR)path) &&
       Rename((STRPTR)partial,(STRPTR)path)) {
        set_status(v,mbr?"MBR backup saved":"RDB backup saved");
    } else if(DeleteFile((STRPTR)partial)) {
        set_status(v,"Partition table backup failed");
    } else {
        set_status(v,"Backup failed. .part file remains");
    }
}
static int choose_restore_file(struct Window *window,int save,char path[256]) {
    struct FileRequester *fr=(struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
        ASL_FuncFlags,(ULONG)(save?FILF_SAVE:0),ASLFR_Window,(ULONG)window,
        ASLFR_TitleText,(ULONG)(save?"Save current RDB recovery copy":"Select RDB backup"),
        ASLFR_InitialFile,(ULONG)(save?"dolly-rdb-before-restore.bin":"dolly-rdb.bin"),
        TAG_DONE);
    int ok=0;
    if(!fr)return 0;
    if(AslRequest(fr,NULL)) {
        const char *drawer=fr->fr_Drawer?(const char *)fr->fr_Drawer:"";
        const char *name=(const char *)fr->fr_File;
        if(strlen(drawer)<256 && name && *name && !strpbrk(name,":/") &&
           strcmp(name,".") && strcmp(name,"..")) {
            strcpy(path,drawer);
            ok=AddPart((STRPTR)path,(STRPTR)name,256)!=0;
        }
    }
    FreeAslRequest(fr);
    return ok;
}
static int save_rdb_recovery(void *context,const void *data,ULONG bytes) {
    const char *path=context;
    char drawer[272],temporary[278];
    size_t prefix=(size_t)((const char *)FilePart((STRPTR)path)-path);
    BPTR lock,handle;
    int ok=0;
    if(!path_is_unused((STRPTR)path))return 0;
    memcpy(drawer,path,prefix);
    strcpy(drawer+prefix,".dolly-rdb");
    strcpy(temporary,drawer);
    strcat(temporary,"/data");
    lock=CreateDir((STRPTR)drawer);
    if(!lock)return 0;
    UnLock(lock);
    handle=Open((STRPTR)temporary,MODE_NEWFILE);
    if(handle) {
        ok=Write(handle,(APTR)data,bytes)==(LONG)bytes;
        if(!Close(handle))ok=0;
        if(ok)ok=path_is_unused((STRPTR)path) &&
                 Rename((STRPTR)temporary,(STRPTR)path);
    }
    if(!ok)DeleteFile((STRPTR)temporary);
    DeleteFile((STRPTR)drawer);
    return ok;
}
static void show_restore_failure(DollyNativeInventoryView *v,const char *reason) {
    struct EasyStruct request={sizeof(struct EasyStruct),0,
        (STRPTR)"Dolly - Restore RDB",(STRPTR)"%s",(STRPTR)"OK"};
    ULONG args[]={(ULONG)reason};
    char first_line[112];
    size_t len=strcspn(reason,"\n");
    if(len>=sizeof(first_line))len=sizeof(first_line)-1;
    memcpy(first_line,reason,len);
    first_line[len]=0;
    set_status(v,first_line);
    if(strcmp(reason,"Restore RDB cancelled.")!=0)
        EasyRequestArgs(v->window,&request,NULL,args);
}
static void restore_partition_table(DollyNativeInventoryView *v) {
    char input[256],recovery[256],warning[128];
    BPTR file;
    const char *reason;
    if(v->driver[0])
        snprintf(warning,sizeof(warning),"Restore RDB on %s / Unit %lu.",
                 v->driver,(unsigned long)v->unit);
    else snprintf(warning,sizeof(warning),"Restore RDB on the selected device.");
    if(!dolly_amiga_confirm_destructive(v->window,warning))return;
    if(!restore_available(v)) {
        reason=!AslBase?"The ASL file requester is unavailable.":
               !v->verified?"Cannot verify the target device and geometry.":
               v->map.format==DOLLY_NATIVE_PARTITION_MBR?
                   "The target contains MBR. Restore RDB requires an existing RDB.":
               v->map.format!=DOLLY_NATIVE_PARTITION_RDB?
                   "No compatible RDB was found on the target.":
               strcmp(v->driver,"scsi.device")!=0?
                   "Restore RDB requires ROM scsi.device.":
                   "The target's reserved RDB area is outside the supported range.";
        show_restore_failure(v,reason);
        return;
    }
    if(dolly_amiga_unit_registered(v->driver,v->unit)!=0) {
        show_restore_failure(v,"Restore RDB requires an offline target.\nNo AmigaDOS device registration may refer to it.");
        return;
    }
    if(!choose_restore_file(v->window,0,input) ||
       !choose_restore_file(v->window,1,recovery))return;
    file=Open((STRPTR)input,MODE_OLDFILE);
    if(!file) {
        show_restore_failure(v,"Could not open the RDB backup.");
        return;
    }
    reason=dolly_amiga_restore_rdb(v->window,v->driver,v->unit,&v->geometry,
                 v->map.rdb_backup_blocks,file,save_rdb_recovery,recovery);
    Close(file);
    if(reason)show_restore_failure(v,reason);
    else set_status(v,"RDB checked. Reopen Inventory to refresh the device snapshot.");
}

void dolly_inventory_view_close(DollyNativeInventoryView *v) {
    if(!v)return;
    if(v->window) {
        if(v->menu)ClearMenuStrip(v->window);
        CloseWindow(v->window);
        v->window=NULL;
    }
    if(v->menu){FreeMenus(v->menu);v->menu=NULL;}
    if(v->gadgets){FreeGadgets(v->gadgets);v->gadgets=NULL;}
    if(v->visual){FreeVisualInfo(v->visual);v->visual=NULL;}
}
int dolly_inventory_view_open(DollyNativeInventoryView *v,
                               struct Screen *screen,
                               const DollyEndpoint *source) {
    LONG top;
    LONG width,height;
    unsigned i;
    static struct NewMenu menu_items[]={
        {NM_TITLE,(STRPTR)"Edit",NULL,0,0,NULL},
        {NM_ITEM,(STRPTR)"Copy",(STRPTR)"C",NM_ITEMDISABLED,0,NULL},
        {NM_END,NULL,NULL,0,0,NULL}
    };
    if(!v || !screen || !source)return 0;
    if(v->window)return 1;
    top=(LONG)screen->BarHeight+1;
    width=screen->Width<INV_WIDTH?screen->Width:INV_WIDTH;
    height=(LONG)screen->Height-top;
    if(height>INV_HEIGHT)height=INV_HEIGHT;
    memset(v,0,sizeof(*v));
    init_list(&v->rows);
    v->selected_partition=~0u;
    copy_text(v->device_info,sizeof(v->device_info),"Unavailable");
    copy_text(v->chs_info,sizeof(v->chs_info),"CHS not reported");
    copy_text(v->geometry_info,sizeof(v->geometry_info),"Not queried");
    if((source->kind==DOLLY_ENDPOINT_UNIT || source->kind==DOLLY_ENDPOINT_PARTITION) &&
       dolly_amiga_parse_unit(source->path,v->driver,sizeof(v->driver),&v->unit)) {
        char device_name[112],size[40],line[88];
        int queried=dolly_amiga_device_query(v->driver,v->unit,
                           device_name,sizeof(device_name),
                           size,sizeof(size),&v->geometry,1);
        if(queried && v->geometry.capacity_valid) {
            v->verified=1;
            copy_text(v->device_info,sizeof(v->device_info),device_name);
            sprintf(v->geometry_info,"%s | %lu B/sector | %lu sectors (%s)",size,
                    (unsigned long)v->geometry.block_bytes,
                    (unsigned long)v->geometry.block_count,
                    v->geometry.capacity_source==DOLLY_GEOMETRY_TRACKDISK?
                    "TD":"SCSI");
            if(v->geometry.cylinders || v->geometry.heads ||
               v->geometry.sectors_per_track) {
                char cylinders[11],heads[11],spt[11];
                format_chs_number(cylinders,v->geometry.cylinders);
                format_chs_number(heads,v->geometry.heads);
                format_chs_number(spt,v->geometry.sectors_per_track);
                sprintf(v->chs_info,"Cyl: %s | Heads: %s | SPT: %s (%s)",
                        cylinders,heads,spt,
                        v->geometry.capacity_source==DOLLY_GEOMETRY_TRACKDISK?
                        "TD":"SCSI");
            }
            if(v->geometry.block_bytes==512ul &&
               dolly_amiga_inventory_scan(v->driver,v->unit,
                                          &v->geometry,&v->map)) {
                printf("INVENTORY: scanner=%s count=%lu\n",
                       v->map.status,(unsigned long)v->map.count);
                for(i=0;i<v->map.count && i<DOLLY_PART_MAX;++i) {
                    const DollyNativePartition *p=&v->map.parts[i];
                    if(p->is_rdb)
                        sprintf(line,"%lu: %s",(unsigned long)(i+1u),p->label);
                    else
                        sprintf(line,"MBR Partition (%s) %lu",
                                p->mbr_type==0x76u?"RDB":mbr_role(p),
                                (unsigned long)p->mbr_slot);
                    printf("INVENTORY: PART %lu label='%s'\n",
                           (unsigned long)(i+1u),p->label);
                    add_row(v,line);
                }
            } else {
                if(v->map.status[0])
                    copy_text(v->status,sizeof(v->status),v->map.status);
                else
                    copy_text(v->status,sizeof(v->status),
                              "Partition scan unavailable");
            }
        } else {
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
    populate_header(v);
    if(!v->status[0])
        copy_text(v->status,sizeof(v->status),"Click a header or detail row to copy it");
    v->visual=GetVisualInfo(screen,TAG_DONE);
    if(!v->visual)return 0;
    v->window=OpenWindowTags(NULL,
                  WA_PubScreen,(ULONG)screen,
                  WA_Title,(ULONG)"Dolly - Sheep Inventory",
                  WA_Left,(ULONG)((screen->Width-width)/2),WA_Top,(ULONG)top,
                  WA_Width,(ULONG)width,WA_Height,(ULONG)height,
                  WA_MinWidth,INV_MIN_WIDTH,WA_MinHeight,INV_MIN_HEIGHT,
                  WA_MaxWidth,(ULONG)screen->Width,WA_MaxHeight,(ULONG)screen->Height,
                  WA_SizeGadget,TRUE,WA_SizeBBottom,TRUE,
                  WA_DragBar,TRUE,WA_DepthGadget,TRUE,
                  WA_CloseGadget,TRUE,WA_Activate,TRUE,
                  WA_SmartRefresh,TRUE,
                  WA_IDCMP,LISTVIEWIDCMP|BUTTONIDCMP|
                           IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW|IDCMP_NEWSIZE|
                           IDCMP_MENUPICK,
                  TAG_DONE);
    if(!v->window){dolly_inventory_view_close(v);return 0;}
    v->menu=CreateMenus(menu_items,TAG_DONE);
    if(!v->menu || !LayoutMenus(v->menu,v->visual,TAG_DONE) ||
       !SetMenuStrip(v->window,v->menu) || !rebuild_gadgets(v,1)) {
        dolly_inventory_view_close(v);
        return 0;
    }
    return 1;
}
void dolly_inventory_view_pump(DollyNativeInventoryView *v) {
    struct IntuiMessage *msg;
    int relayout=0,details_changed=0;
    if(!v || !v->window)return;
    while((msg=GT_GetIMsg(v->window->UserPort))!=NULL) {
        ULONG cls=msg->Class;
        UWORD id=0,code=msg->Code;
        const char *row=NULL;
        if(cls==IDCMP_GADGETUP && msg->IAddress)
            id=((struct Gadget *)msg->IAddress)->GadgetID;
        if(cls==IDCMP_NEWSIZE)relayout=1;
        if(cls==IDCMP_REFRESHWINDOW){
            GT_BeginRefresh(v->window);
            if(!relayout && !details_changed)paint(v,v->window->WScreen);
            GT_EndRefresh(v->window,TRUE);
        }
        GT_ReplyIMsg(msg);
        if(cls==IDCMP_CLOSEWINDOW || id==INV_CLOSE_ID) {
            dolly_inventory_view_close(v);return;
        }
        if(!relayout && id==INV_BACKUP_ID)backup_partition_table(v);
        if(id==INV_RESTORE_ID)restore_partition_table(v);
        if(id==INV_HEADER_ID && (unsigned)code<DOLLY_INVENTORY_HEADER_ROWS) {
            row=v->header_text[code];
        } else if(id==INV_PART_ID && (unsigned)code<v->count) {
            if((unsigned)code!=v->selected_partition) {
                v->selected_partition=(unsigned)code;
                details_changed=1;
            }
        } else if(!details_changed && id==INV_DETAIL_ID &&
                  (unsigned)code<v->detail_count) {
            v->selected_detail=(unsigned)code;
            row=v->detail_text[code];
        }
        if(row) {
            v->clipboard_row=row;
            OnMenu(v->window,INV_COPY_MENU);
            copy_selected_row(v);
        }
        if(cls==IDCMP_MENUPICK && code==INV_COPY_MENU)
            copy_selected_row(v);
    }
    if((relayout || details_changed) && !rebuild_gadgets(v,details_changed))
        dolly_inventory_view_close(v);
}
