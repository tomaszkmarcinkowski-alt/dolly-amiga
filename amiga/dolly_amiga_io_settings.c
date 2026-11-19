#include "dolly_amiga_fonts.h"
#include "dolly_amiga_io_settings.h"
#include "dolly_amiga_device_query.h"
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <graphics/text.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct Override {
    struct Override *next;
    DollyPhysicalEntry device;
    DollyIoHints hints;
} Override;
static Override *overrides;

static Override *find(const DollyPhysicalEntry *device) {
    Override *entry;
    for(entry=overrides;entry;entry=entry->next)
        if(entry->device.unit==device->unit &&
           strcasecmp(entry->device.driver,device->driver)==0)return entry;
    return NULL;
}
DollyIoHints dolly_amiga_io_override(const DollyPhysicalEntry *device) {
    Override *entry=find(device);
    DollyIoHints empty={0};
    return entry?entry->hints:empty;
}
static int legacy_override,verify_writes,suppress_same_unit_warning;
static int allow_outside_range;
int dolly_amiga_allow_outside_range(void) { return allow_outside_range; }
void dolly_amiga_set_allow_outside_range(int enabled) { allow_outside_range=enabled; }
int dolly_amiga_suppress_same_unit_warning(void) { return suppress_same_unit_warning; }
void dolly_amiga_set_suppress_same_unit_warning(int enabled) {
    suppress_same_unit_warning=enabled;
}
int dolly_amiga_verify_writes(void) { return verify_writes; }
void dolly_amiga_set_verify_writes(int enabled) { verify_writes=enabled; }
int dolly_amiga_legacy_override(void) { return legacy_override; }
void dolly_amiga_set_legacy_override(int enabled) { legacy_override=enabled; }

int dolly_amiga_io_store(const DollyPhysicalEntry *device,DollyIoHints hints) {
    Override **link=&overrides;
    Override *entry;
    unsigned count=0;
    while((entry=*link)!=NULL) {
        if(entry->device.unit==device->unit &&
           strcasecmp(entry->device.driver,device->driver)==0)break;
        ++count;
        link=&entry->next;
    }
    if(!hints.present) {
        if(entry) {
            *link=entry->next;
            free(entry);
        }
    } else {
        if(!entry) {
            if(count==DOLLY_PREFS_MAX)return 0;
            entry=malloc(sizeof(*entry));
            if(!entry)return 0;
            entry->device=*device;
            entry->next=NULL;
            *link=entry;
        }
        entry->hints=hints;
    }
    return 1;
}
int dolly_amiga_io_entry(unsigned index,DollyPhysicalEntry *device,DollyIoHints *hints) {
    Override *entry;
    for(entry=overrides;entry;entry=entry->next) {
        if(!entry->hints.present)continue;
        if(index--==0) {
            *device=entry->device;
            *hints=entry->hints;
            return 1;
        }
    }
    return 0;
}
void dolly_amiga_io_settings_clear(void) {
    while(overrides) {
        Override *next=overrides->next;
        free(overrides);
        overrides=next;
    }
    legacy_override=0;
    verify_writes=0;
    suppress_same_unit_warning=0;
    allow_outside_range=0;
}
static int number(const char *text,uint32_t *out) {
    uint32_t value=0,base=10,digit;
    if(text[0]=='0' && (text[1]=='x' || text[1]=='X')) {
        text+=2;
        base=16;
    }
    if(!*text)return 0;
    for(;*text;++text) {
        if(*text>='0' && *text<='9')digit=(uint32_t)(*text-'0');
        else if(*text>='a' && *text<='f')digit=(uint32_t)(*text-'a')+10u;
        else if(*text>='A' && *text<='F')digit=(uint32_t)(*text-'A')+10u;
        else return 0;
        if(digit>=base || value>(UINT32_MAX-digit)/base)return 0;
        value=value*base+digit;
    }
    *out=value;
    return 1;
}
static const char *const field_labels[]={"Mask","BufMemType","MaxTransfer"};
static const char *const check_labels[]={
    "Bypass legacy scsi.device 4 GiB limit",
    "Verify writes by reading sectors back",
    "Hide same-device speed warning",
    "Allow partitions outside range"
};
static const char *const button_labels[]={"Save","Reset Auto","Cancel"};
static const char *const help_text[]={
    "Mask: limits the device's DMA addresses.",
    "BufMemType: selects memory for I/O buffers.",
    "MaxTransfer: caps one device request.",
    "Use READ64 when an older driver advertises it.",
    "Compare written sectors with the original data.",
    "Hide the speed notice for the same unit.",
    "Allow reading source partitions beyond driver capacity.",
    "Save changes to the preferences file.",
    "Clear overrides for this unit. Save to apply.",
    "Close Settings and discard your edits."
};
typedef struct SettingsHelpArea {
    WORD left,top,right,bottom;
} SettingsHelpArea;

static LONG text_width(struct Screen *screen,const char *text) {
    struct RastPort rp=screen->RastPort;
    SetFont(&rp,dolly_amiga_font(0));
    return TextLength(&rp,(STRPTR)text,(ULONG)strlen(text));
}
static LONG widest(struct Screen *screen,const char *const *items,unsigned count) {
    unsigned i;
    LONG width=0;
    for(i=0;i<count;++i) {
        LONG value=text_width(screen,items[i]);
        if(value>width)width=value;
    }
    return width;
}
static const char *settings_help(const SettingsHelpArea *areas,WORD x,WORD y) {
    unsigned i;
    for(i=0;i<sizeof(help_text)/sizeof(help_text[0]);++i)
        if(x>=areas[i].left && x<areas[i].right &&
           y>=areas[i].top && y<areas[i].bottom)return help_text[i];
    return NULL;
}

static void format_font(char *label,size_t size,const DollyFontChoice *font) {
    if(font->name[0])snprintf(label,size,"%s / %u",font->name,(unsigned)font->size);
    else snprintf(label,size,"Screen font");
}

void dolly_amiga_io_settings(struct Screen *screen,const DollyEndpoint *endpoint) {
    static const unsigned bits[]={DOLLY_IO_MASK,DOLLY_IO_BUF_MEM,DOLLY_IO_MAX_TRANSFER};
    struct Gadget *gadgets=NULL,*last,*fields[3],*status,*font_display;
    struct Window *window=NULL;
    struct NewGadget ng={0};
    DollyPhysicalEntry device={0};
    DollyIoHints current;
    uint64_t start,length;
    ULONG sector;
    LONG left,y,client_width,client_height,row_height,field_x,field_width,button_width;
    LONG label_width,content_width,check_width;
    SettingsHelpArea areas[sizeof(help_text)/sizeof(help_text[0])];
    const char *legend;
    APTR visual;
    unsigned i;
    char title[94],initial[3][11],font_label[DOLLY_FONT_NAME_BYTES+20];
    DollyFontChoice font=dolly_amiga_font_choice;
    static const char *const font_buttons[]={"Choose font...", "Screen font"};
    LONG font_button_width=widest(screen,font_buttons,2)+18;
    uint32_t values[3];
    int done=0,has_device=0,legacy=legacy_override;
    int verify=verify_writes,suppress_warning=suppress_same_unit_warning;
    int outside_range=allow_outside_range;
    const char *hover=NULL;
    static const char help_default[]="Point to a control to see its purpose.";
    if(endpoint->kind==DOLLY_ENDPOINT_LOGICAL)
        has_device=dolly_logical_extent(endpoint->path,&device,&start,&length,&sector,NULL);
    else if(endpoint->kind==DOLLY_ENDPOINT_UNIT || endpoint->kind==DOLLY_ENDPOINT_PARTITION)
        has_device=dolly_amiga_parse_unit(endpoint->path,device.driver,sizeof(device.driver),
                                         &device.unit)!=NULL;
    current=dolly_amiga_io_override(&device);
    values[0]=current.mask;values[1]=current.buffer_memory;values[2]=current.max_transfer;
    if(has_device)
        snprintf(title,sizeof(title),"Dolly Settings - %s / Unit %lu",device.driver,
                 (unsigned long)device.unit);
    else strcpy(title,"Dolly Settings");
    legend=has_device?"Blank = Auto. Decimal or 0x hex.":"Select a source to edit its I/O parameters.";
    row_height=dolly_amiga_font(0)->tf_YSize+5;
    label_width=widest(screen,field_labels,3);
    field_width=text_width(screen,"0x00000000")+14;
    button_width=widest(screen,button_labels,3)+18;
    check_width=28+widest(screen,check_labels,sizeof(check_labels)/sizeof(check_labels[0]));
    content_width=widest(screen,help_text,sizeof(help_text)/sizeof(help_text[0]))+12;
    if(content_width<text_width(screen,legend))content_width=text_width(screen,legend);
    if(content_width<check_width)content_width=check_width;
    if(content_width<label_width+12+field_width)content_width=label_width+12+field_width;
    if(content_width<3*button_width+12)content_width=3*button_width+12;
    if(content_width<2*font_button_width+12+text_width(screen,"Screen font"))
        content_width=2*font_button_width+12+text_width(screen,"Screen font");
    format_font(font_label,sizeof(font_label),&font);
    if(content_width>(LONG)screen->Width-36)content_width=(LONG)screen->Width-36;
    client_width=content_width+12;
    client_height=11*row_height+10*3+8;
    visual=GetVisualInfo(screen,TAG_DONE);
    if(!visual)return;
    window=OpenWindowTags(NULL,WA_PubScreen,(ULONG)screen,WA_Title,(ULONG)title,
        WA_InnerWidth,(ULONG)client_width,WA_InnerHeight,(ULONG)client_height,WA_Activate,TRUE,WA_CloseGadget,TRUE,
        WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SmartRefresh,TRUE,WA_ReportMouse,TRUE,
        WA_IDCMP,IDCMP_GADGETUP|IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW|IDCMP_MOUSEMOVE,TAG_DONE);
    if(!window)goto cleanup;
    last=CreateContext(&gadgets);
    if(!last)goto cleanup;
    ng.ng_VisualInfo=visual;
    dolly_amiga_window_font(window,0);
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    left=window->BorderLeft+6;
    y=window->BorderTop+4;
    field_x=left+label_width+12;
    ng.ng_Height=(WORD)row_height;
    for(i=0;i<3;++i) {
        initial[i][0]='\0';
        if(current.present&bits[i])sprintf(initial[i],"0x%08lX",(unsigned long)values[i]);
        ng.ng_LeftEdge=(WORD)field_x;ng.ng_TopEdge=(WORD)y;
        ng.ng_Width=(WORD)field_width;
        ng.ng_GadgetText=(STRPTR)field_labels[i];ng.ng_Flags=PLACETEXT_LEFT;
        ng.ng_GadgetID=(UWORD)(i+1);
        last=CreateGadget(STRING_KIND,last,&ng,GTST_String,(ULONG)initial[i],
                          GTST_MaxChars,11UL,GA_Disabled,(ULONG)!has_device,TAG_DONE);
        if(!last)goto cleanup;
        fields[i]=last;
        areas[i]=(SettingsHelpArea){left,y,field_x+field_width,y+row_height};
        y+=row_height+3;
    }
    ng.ng_LeftEdge=(WORD)left;ng.ng_TopEdge=(WORD)y;ng.ng_Width=(WORD)content_width;
    ng.ng_GadgetText=NULL;ng.ng_Flags=0;ng.ng_GadgetID=0;
    last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)legend,TAG_DONE);
    if(!last)goto cleanup;
    y+=row_height+3;
    {
        const int checked[]={legacy,verify,suppress_warning,outside_range};
        for(i=0;i<sizeof(checked)/sizeof(checked[0]);++i) {
            ng.ng_TopEdge=(WORD)y;ng.ng_Width=22;
            ng.ng_GadgetID=(UWORD)(20+i);
            ng.ng_GadgetText=(STRPTR)check_labels[i];ng.ng_Flags=PLACETEXT_RIGHT;
            last=CreateGadget(CHECKBOX_KIND,last,&ng,GTCB_Checked,(ULONG)checked[i],TAG_DONE);
            if(!last)goto cleanup;
            areas[3+i]=(SettingsHelpArea){left,y,left+28+text_width(screen,check_labels[i]),y+row_height};
            y+=row_height+3;
        }
    }
    ng.ng_TopEdge=(WORD)y;ng.ng_Width=(WORD)content_width;
    ng.ng_GadgetText=NULL;ng.ng_Flags=0;ng.ng_GadgetID=0;
    for(i=0;i<2;++i) {
        ng.ng_LeftEdge=(WORD)(left+(LONG)i*(font_button_width+6));
        ng.ng_Width=(WORD)font_button_width;
        ng.ng_GadgetText=(STRPTR)font_buttons[i];ng.ng_GadgetID=(UWORD)(30+i);
        last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);
        if(!last)goto cleanup;
    }
    ng.ng_LeftEdge=(WORD)(left+2*(font_button_width+6));
    ng.ng_Width=(WORD)(content_width-2*(font_button_width+6));
    ng.ng_GadgetText=NULL;ng.ng_GadgetID=0;
    font_display=last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)font_label,
        GTTX_Clipped,TRUE,TAG_DONE);
    if(!last)goto cleanup;
    y+=row_height+3;
    ng.ng_LeftEdge=(WORD)left;ng.ng_TopEdge=(WORD)y;ng.ng_Width=(WORD)content_width;
    status=last=CreateGadget(TEXT_KIND,last,&ng,
        GTTX_Text,(ULONG)help_default,GTTX_Border,TRUE,GTTX_Clipped,TRUE,TAG_DONE);
    if(!last)goto cleanup;
    y+=row_height+3;
    ng.ng_TopEdge=(WORD)y;ng.ng_Width=(WORD)button_width;
    for(i=0;i<3;++i) {
        LONG x=left+(LONG)i*(button_width+6);
        ng.ng_LeftEdge=(WORD)x;ng.ng_GadgetID=(UWORD)(10+i);
        ng.ng_GadgetText=(STRPTR)button_labels[i];
        last=CreateGadget(BUTTON_KIND,last,&ng,GA_Disabled,(ULONG)(i==1 && !has_device),TAG_DONE);
        if(!last)goto cleanup;
        areas[7+i]=(SettingsHelpArea){x,y,x+button_width,y+row_height};
    }
    AddGList(window,gadgets,(ULONG)-1,-1,NULL);
    RefreshGList(gadgets,window,NULL,-1);
    GT_RefreshWindow(window,NULL);
    while(!done) {
        struct IntuiMessage *msg;
        WaitPort(window->UserPort);
        while(!done && (msg=GT_GetIMsg(window->UserPort))!=NULL) {
            ULONG cls=msg->Class;
            UWORD id=0,code=msg->Code;
            WORD mouse_x=msg->MouseX,mouse_y=msg->MouseY;
            if(cls==IDCMP_GADGETUP && msg->IAddress)
                id=((struct Gadget *)msg->IAddress)->GadgetID;
            if(cls==IDCMP_REFRESHWINDOW) {
                GT_BeginRefresh(window);
                GT_EndRefresh(window,TRUE);
            }
            GT_ReplyIMsg(msg);
            if(cls==IDCMP_CLOSEWINDOW || id==12)done=1;
            else if(cls==IDCMP_MOUSEMOVE) {
                const char *next=settings_help(areas,mouse_x,mouse_y);
                if(next!=hover) {
                    hover=next;
                    GT_SetGadgetAttrs(status,window,NULL,GTTX_Text,
                                      (ULONG)(hover?hover:help_default),TAG_DONE);
                }
            }
            else if(id==20)legacy=code!=0;
            else if(id==21)verify=code!=0;
            else if(id==22)suppress_warning=code!=0;
            else if(id==23)outside_range=code!=0;
            else if(id==30 || id==31) {
                if(id==31)memset(&font,0,sizeof(font));
                if(id==31 || dolly_amiga_choose_font(window,&font)) {
                    format_font(font_label,sizeof(font_label),&font);
                    GT_SetGadgetAttrs(font_display,window,NULL,GTTX_Text,(ULONG)font_label,TAG_DONE);
                }
            }
            else if(id==11) {
                for(i=0;i<3;++i)
                    GT_SetGadgetAttrs(fields[i],window,NULL,GTST_String,(ULONG)"",TAG_DONE);
            } else if(id==10) {
                DollyIoHints next={0};
                uint32_t *const fields_out[]={&next.mask,&next.buffer_memory,&next.max_transfer};
                Override *existing=has_device?find(&device):NULL;
                int valid=1;
                if(has_device)for(i=0;i<3;++i) {
                    const char *text=(const char *)((struct StringInfo *)fields[i]->SpecialInfo)->Buffer;
                    if(!*text)continue;
                    next.present|=bits[i];
                    if(!number(text,fields_out[i]))valid=0;
                }
                if(valid && dolly_io_hints_valid(next) &&
                   (!has_device || existing || dolly_amiga_io_store(&device,next))) {
                    DollyFontChoice previous_font=dolly_amiga_font_choice;
                    int previous=legacy_override,previous_verify=verify_writes;
                    int previous_warning=suppress_same_unit_warning;
                    int previous_outside_range=allow_outside_range;
                    if(existing)existing->hints=next;
                    legacy_override=legacy;
                    verify_writes=verify;
                    suppress_same_unit_warning=suppress_warning;
                    allow_outside_range=outside_range;
                    dolly_amiga_font_choice=font;
                    if(dolly_amiga_prefs_save()) {
                        if(existing && !next.present)dolly_amiga_io_store(&device,next);
                        done=1;
                    }
                    else {
                        dolly_amiga_font_choice=previous_font;
                        legacy_override=previous;
                        verify_writes=previous_verify;
                        suppress_same_unit_warning=previous_warning;
                        allow_outside_range=previous_outside_range;
                        if(existing)existing->hints=current;
                        else if(has_device)dolly_amiga_io_store(&device,current);
                        GT_SetGadgetAttrs(status,window,NULL,
                            GTTX_Text,(ULONG)"Could not save preferences.",TAG_DONE);
                    }
                } else GT_SetGadgetAttrs(status,window,NULL,
                        GTTX_Text,(ULONG)"Invalid or unavailable I/O settings.",TAG_DONE);
            }
        }
    }
cleanup:
    if(window)CloseWindow(window);
    if(gadgets)FreeGadgets(gadgets);
    FreeVisualInfo(visual);
}
