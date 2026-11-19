#include "dolly_amiga_fonts.h"
#include "dolly_amiga_log_view.h"
#include "dolly_amiga_clipboard.h"
#include <dos/dos.h>
#include <exec/lists.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/asl.h>
#include <libraries/gadtools.h>
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <graphics/text.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <string.h>

extern struct Library *AslBase;
enum { LOG_LIST=1, LOG_PREVIOUS, LOG_NEXT, LOG_SAVE, LOG_CLOSE };

static struct {
    struct Window *window;
    struct Gadget *gadgets,*list,*previous,*next,*status;
    APTR visual;
    const DollySurfaceScan *scan;
    const DollySurfaceLogPage *page;
    const char *outcome;
    struct List rows;
    struct Node nodes[DOLLY_LOG_PAGE_SIZE];
    char lines[DOLLY_LOG_PAGE_SIZE][DOLLY_LOG_LINE_SIZE];
    char status_text[96];
    unsigned long page_number;
} view;

static void init_rows(void) {
    view.rows.lh_Head=(struct Node *)&view.rows.lh_Tail;
    view.rows.lh_Tail=NULL;
    view.rows.lh_TailPred=(struct Node *)&view.rows.lh_Head;
}
static int path_unused(STRPTR path) {
    BPTR lock=Lock(path,ACCESS_READ);
    if(lock) {
        UnLock(lock);
        return 0;
    }
    return IoErr()==ERROR_OBJECT_NOT_FOUND;
}
static void status(const char *message) {
    snprintf(view.status_text,sizeof(view.status_text),"%s",message);
    GT_SetGadgetAttrs(view.status,view.window,NULL,
                      GTTX_Text,(ULONG)view.status_text,TAG_DONE);
}
static int write_line(BPTR file,const char *line) {
    LONG length=(LONG)strlen(line);
    return Write(file,(APTR)line,length)==length && Write(file,(APTR)"\n",1)==1;
}
static int write_log(BPTR file) {
    const DollySurfaceScan *scan=view.scan;
    const DollySurfaceLogPage *page;
    char summary[384],first[21],total[21],processed[21],lost[21],blocks[21],bytes[21];
    char line[DOLLY_LOG_LINE_SIZE];
    unsigned i;
    dolly_surface_log_decimal(first,scan->first_lba);
    dolly_surface_log_decimal(total,scan->total);
    dolly_surface_log_decimal(processed,scan->processed);
    dolly_surface_log_decimal(lost,scan->log.omitted);
    snprintf(summary,sizeof(summary),"Source: %s",scan->source_path);
    if(!write_line(file,"Dolly Surface Scan log") || !write_line(file,summary))
        return 0;
    snprintf(summary,sizeof(summary),"Device Name: %s",scan->device_name);
    if(!write_line(file,summary))return 0;
    snprintf(summary,sizeof(summary),"ATA IDENTIFY: %s",scan->ata_identify);
    if(!write_line(file,summary))return 0;
    if(scan->outside_range==DOLLY_RANGE_ZERO_FILL &&
       !write_line(file,"Outside driver range: reported as unreadable, not scanned"))return 0;
    if(scan->outside_range==DOLLY_RANGE_AS_IS &&
       !write_line(file,"Outside driver range: attempted reads after timed approval"))return 0;
    dolly_surface_log_geometry(line,&scan->geometry);
    if(!write_line(file,line))return 0;
    dolly_surface_log_decimal(blocks,scan->geometry.block_count);
    dolly_surface_log_decimal(bytes,scan->geometry.block_count*scan->geometry.block_bytes);
    snprintf(summary,sizeof(summary),"Device capacity: %s sectors / %s bytes (%s)\n"
             "CHS numbering: cylinder and head start at 0, sector starts at 1",
             blocks,bytes,scan->geometry.capacity_source==DOLLY_GEOMETRY_TRACKDISK?
             "TD_GETGEOMETRY":"SCSI READ CAPACITY(10)");
    if(!write_line(file,summary))return 0;
    snprintf(summary,sizeof(summary),
             "Result: %s\nFirst LBA: %s\nSector size: %lu bytes\n"
             "Sectors examined: %s / %s\nRetries: %lu\nUnreadable sectors: %lu",
             view.outcome,first,(unsigned long)scan->sector_bytes,processed,total,
             (unsigned long)scan->retries,(unsigned long)scan->errors);
    if(!write_line(file,summary))return 0;
    if(scan->log.omitted) {
        snprintf(summary,sizeof(summary),"Incomplete log: %s events omitted (out of memory)",lost);
        if(!write_line(file,summary))return 0;
    }
    if(!write_line(file,""))return 0;
    if(!scan->log.first && !scan->log.omitted)
        return write_line(file,"No retry or read error events.");
    for(page=scan->log.first;page;page=page->next) {
        for(i=0;i<page->count;++i) {
            dolly_surface_log_line(line,&page->entries[i],&scan->geometry);
            if(!write_line(file,line))return 0;
        }
    }
    return 1;
}
static void save_log_file(const char *path) {
    char partial[256+sizeof(".part")-1u];
    BPTR handle;
    int ok;
    if(!path_unused((STRPTR)path)) {
        status("Destination exists or inaccessible");
        return;
    }
    strcpy(partial,path);
    strcat(partial,".part");
    if(!path_unused((STRPTR)partial) ||
       !(handle=Open((STRPTR)partial,MODE_NEWFILE))) {
        status("Could not create temporary log file");
        return;
    }
    ok=write_log(handle);
    if(!Close(handle))ok=0;
    if(ok && path_unused((STRPTR)path) && Rename((STRPTR)partial,(STRPTR)path))
        status("Surface Scan log saved");
    else if(DeleteFile((STRPTR)partial))
        status("Could not save Surface Scan log");
    else
        status("Save failed. .part file remains");
}
static void save_log(void) {
    struct FileRequester *request;
    char path[256];
    const char *drawer,*file;
    int ok;
    request=(struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
        ASL_FuncFlags,(ULONG)FILF_SAVE,ASLFR_Window,(ULONG)view.window,
        ASLFR_TitleText,(ULONG)"Save Surface Scan log",
        ASLFR_InitialFile,(ULONG)"dolly-surface-scan.txt",TAG_DONE);
    if(!request) {
        status("Could not open save requester");
        return;
    }
    if(!AslRequest(request,NULL)) {
        FreeAslRequest(request);
        return;
    }
    drawer=request->fr_Drawer?(const char *)request->fr_Drawer:"";
    file=(const char *)request->fr_File;
    ok=strlen(drawer)<sizeof(path) && file && *file && !strpbrk(file,":/");
    if(ok) {
        strcpy(path,drawer);
        ok=AddPart((STRPTR)path,(STRPTR)file,sizeof(path));
    }
    FreeAslRequest(request);
    if(!ok) {
        status("Invalid log filename or drawer");
        return;
    }
    save_log_file(path);
}

static void show_page(void) {
    unsigned i,count=view.page?view.page->count:0;
    GT_SetGadgetAttrs(view.list,view.window,NULL,GTLV_Labels,~0UL,TAG_DONE);
    init_rows();
    if(!count) {
        snprintf(view.lines[0],sizeof(view.lines[0]),"%s",view.scan->log.omitted?
                 "No entries retained." : "No retry or read error events.");
        count=1;
    }
    for(i=0;i<count;++i) {
        if(view.page)dolly_surface_log_line(view.lines[i],&view.page->entries[i],
                                           &view.scan->geometry);
        view.nodes[i].ln_Name=view.lines[i];
        AddTail(&view.rows,&view.nodes[i]);
    }
    GT_SetGadgetAttrs(view.list,view.window,NULL,
        GTLV_Labels,(ULONG)&view.rows,GTLV_Top,0UL,GTLV_Selected,~0UL,TAG_DONE);
    GT_SetGadgetAttrs(view.previous,view.window,NULL,
        GA_Disabled,(ULONG)(!view.page || !view.page->previous),TAG_DONE);
    GT_SetGadgetAttrs(view.next,view.window,NULL,
        GA_Disabled,(ULONG)(!view.page || !view.page->next),TAG_DONE);
    snprintf(view.status_text,sizeof(view.status_text),"Page %lu  |  %s",
             view.page_number,view.scan->log.omitted?"Incomplete log (out of memory)":view.outcome);
    GT_SetGadgetAttrs(view.status,view.window,NULL,
                      GTTX_Text,(ULONG)view.status_text,TAG_DONE);
}
static struct Gadget *button(struct Gadget *previous,struct NewGadget *ng,
                             WORD x,UWORD id,const char *label,int disabled) {
    ng->ng_LeftEdge=x;
    ng->ng_GadgetID=id;
    ng->ng_GadgetText=(STRPTR)label;
    return CreateGadget(BUTTON_KIND,previous,ng,GA_Disabled,(ULONG)disabled,TAG_DONE);
}
void dolly_amiga_log_view_close(void) {
    if(view.window)CloseWindow(view.window);
    if(view.gadgets)FreeGadgets(view.gadgets);
    if(view.visual)FreeVisualInfo(view.visual);
    memset(&view,0,sizeof(view));
}
int dolly_amiga_log_view_open(struct Screen *screen,const DollySurfaceScan *scan,
                              const char *outcome) {
    struct NewGadget ng={0};
    struct Gadget *last;
    WORD left,top,width,height,row,button_width;
    if(view.window) {
        WindowToFront(view.window);
        ActivateWindow(view.window);
        return 1;
    }
    view.scan=scan;
    view.outcome=outcome;
    view.page=scan->log.first;
    view.page_number=1;
    view.visual=GetVisualInfo(screen,TAG_DONE);
    if(!view.visual)goto fail;
    view.window=OpenWindowTags(NULL,WA_PubScreen,(ULONG)screen,
        WA_Title,(ULONG)"Dolly - Surface Scan Log",
        WA_Width,600UL,WA_Height,190UL,
        WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_CloseGadget,TRUE,
        WA_SmartRefresh,TRUE,WA_Activate,TRUE,
        WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_GADGETUP|IDCMP_REFRESHWINDOW,TAG_DONE);
    if(!view.window)goto fail;
    left=view.window->BorderLeft+4;
    top=view.window->BorderTop+4;
    width=view.window->Width-view.window->BorderRight-4-left;
    height=view.window->Height-view.window->BorderBottom-4-top;
    last=CreateContext(&view.gadgets);
    if(!last)goto fail;
    init_rows();
    ng.ng_VisualInfo=view.visual;
    dolly_amiga_window_font(view.window,1);
    ng.ng_TextAttr=dolly_amiga_text_attr(1);
    ng.ng_LeftEdge=left;
    ng.ng_TopEdge=top;
    ng.ng_Width=width;
    row=(WORD)(dolly_amiga_font(1)->tf_YSize+6);
    button_width=(WORD)(TextLength(view.window->RPort,(STRPTR)"Previous",8)+20);
    if(button_width<76)button_width=76;
    if(4*button_width+18>width)goto fail;
    ng.ng_Height=height-2*row-8;
    ng.ng_GadgetID=LOG_LIST;
    last=CreateGadget(LISTVIEW_KIND,last,&ng,GTLV_Labels,(ULONG)&view.rows,TAG_DONE);
    if(!last)goto fail;
    view.list=last;
    ng.ng_TopEdge=top+height-2*row-4;
    ng.ng_Height=row;
    ng.ng_GadgetID=0;
    last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)view.status_text,
                      GTTX_Border,TRUE,TAG_DONE);
    if(!last)goto fail;
    view.status=last;
    ng.ng_TopEdge=top+height-row;
    ng.ng_Height=row;
    ng.ng_Width=button_width;
    last=button(last,&ng,left,LOG_PREVIOUS,"Previous",1);
    if(!last)goto fail;
    view.previous=last;
    last=button(last,&ng,left+button_width+6,LOG_NEXT,"Next",1);
    if(!last)goto fail;
    view.next=last;
    last=button(last,&ng,left+2*(button_width+6),LOG_SAVE,"Save Log",!AslBase);
    if(!last)goto fail;
    last=button(last,&ng,left+width-button_width,LOG_CLOSE,"Close",0);
    if(!last)goto fail;
    AddGList(view.window,view.gadgets,(ULONG)-1,-1,NULL);
    RefreshGList(view.gadgets,view.window,NULL,-1);
    GT_RefreshWindow(view.window,NULL);
    show_page();
    return 1;
fail:
    dolly_amiga_log_view_close();
    return 0;
}
ULONG dolly_amiga_log_view_signal(void) {
    return view.window && view.window->UserPort ? 1UL<<view.window->UserPort->mp_SigBit : 0;
}

void dolly_amiga_log_view_pump(void) {
    struct IntuiMessage *msg;
    if(!view.window)return;
    while((msg=GT_GetIMsg(view.window->UserPort))!=NULL) {
        ULONG cls=msg->Class;
        UWORD code=msg->Code,id=0;
        if(cls==IDCMP_GADGETUP && msg->IAddress)
            id=((struct Gadget *)msg->IAddress)->GadgetID;
        if(cls==IDCMP_REFRESHWINDOW) {
            GT_BeginRefresh(view.window);
            GT_EndRefresh(view.window,TRUE);
        }
        GT_ReplyIMsg(msg);
        if(cls==IDCMP_CLOSEWINDOW || id==LOG_CLOSE) {
            dolly_amiga_log_view_close();
            return;
        }
        if(id==LOG_PREVIOUS && view.page && view.page->previous) {
            view.page=view.page->previous;
            --view.page_number;
            show_page();
        } else if(id==LOG_NEXT && view.page && view.page->next) {
            view.page=view.page->next;
            ++view.page_number;
            show_page();
        } else if(id==LOG_SAVE && AslBase) {
            save_log();
        } else if(id==LOG_LIST && code<(view.page?view.page->count:1u)) {
            status(dolly_amiga_copy_text(view.lines[code])?
                   "Copied to Clipboard":"Clipboard copy failed");
        }
    }
}
