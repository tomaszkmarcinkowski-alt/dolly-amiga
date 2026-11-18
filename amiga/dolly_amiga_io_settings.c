#include "dolly_amiga_io_settings.h"
#include "dolly_amiga_device_query.h"
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/gadtools.h>
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
static int legacy_override;
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
void dolly_amiga_io_settings(struct Screen *screen,const DollyEndpoint *endpoint) {
    static const char *const labels[]={"Mask","BufMemType","MaxTransfer"};
    static const unsigned bits[]={DOLLY_IO_MASK,DOLLY_IO_BUF_MEM,DOLLY_IO_MAX_TRANSFER};
    struct Gadget *gadgets=NULL,*last,*fields[3],*status;
    struct Window *window=NULL;
    struct NewGadget ng={0};
    DollyPhysicalEntry device={0};
    DollyIoHints current;
    uint64_t start,length;
    ULONG sector;
    APTR visual;
    unsigned i;
    char title[94],initial[3][11];
    uint32_t values[3];
    int done=0,has_device=0,legacy=legacy_override;
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
    visual=GetVisualInfo(screen,TAG_DONE);
    if(!visual)return;
    window=OpenWindowTags(NULL,WA_PubScreen,(ULONG)screen,WA_Title,(ULONG)title,
        WA_Width,490UL,WA_Height,165UL,WA_Activate,TRUE,WA_CloseGadget,TRUE,
        WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SmartRefresh,TRUE,
        WA_IDCMP,IDCMP_GADGETUP|IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW,TAG_DONE);
    if(!window)goto cleanup;
    last=CreateContext(&gadgets);
    if(!last)goto cleanup;
    ng.ng_VisualInfo=visual;
    ng.ng_TextAttr=screen->Font;
    ng.ng_LeftEdge=132;ng.ng_Width=160;ng.ng_Height=15;
    for(i=0;i<3;++i) {
        initial[i][0]='\0';
        if(current.present&bits[i])sprintf(initial[i],"0x%08lX",(unsigned long)values[i]);
        ng.ng_TopEdge=(WORD)(window->BorderTop+5+20*i);
        ng.ng_GadgetText=(STRPTR)labels[i];ng.ng_Flags=PLACETEXT_LEFT;
        ng.ng_GadgetID=(UWORD)(i+1);
        last=CreateGadget(STRING_KIND,last,&ng,GTST_String,(ULONG)initial[i],
                          GTST_MaxChars,11UL,GA_Disabled,(ULONG)!has_device,TAG_DONE);
        if(!last)goto cleanup;
        fields[i]=last;
    }
    ng.ng_LeftEdge=window->BorderLeft+6;
    ng.ng_TopEdge=window->BorderTop+67;
    ng.ng_Width=22;
    ng.ng_GadgetID=20;
    ng.ng_GadgetText=(STRPTR)"Bypass legacy scsi.device 4 GiB limit";
    ng.ng_Flags=PLACETEXT_RIGHT;
    last=CreateGadget(CHECKBOX_KIND,last,&ng,GTCB_Checked,(ULONG)legacy,TAG_DONE);
    if(!last)goto cleanup;
    ng.ng_TopEdge=window->BorderTop+88;
    ng.ng_Width=465;ng.ng_GadgetText=NULL;
    ng.ng_Flags=0;ng.ng_GadgetID=0;
    last=CreateGadget(TEXT_KIND,last,&ng,
        GTTX_Text,(ULONG)(has_device?"Blank = Auto. Decimal or 0x hex.":"Select a source to edit its I/O parameters."),
        GTTX_Border,TRUE,TAG_DONE);
    if(!last)goto cleanup;
    status=last;
    ng.ng_TopEdge=window->BorderTop+111;ng.ng_Width=100;
    for(i=0;i<4;++i) {
        static const char *const buttons[]={"Save","Use","Reset Auto","Cancel"};
        ng.ng_GadgetID=(UWORD)(10+i);
        ng.ng_GadgetText=(STRPTR)buttons[i];
        last=CreateGadget(BUTTON_KIND,last,&ng,GA_Disabled,(ULONG)(i==2 && !has_device),TAG_DONE);
        if(!last)goto cleanup;
        ng.ng_LeftEdge+=106;
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
            if(cls==IDCMP_GADGETUP && msg->IAddress)
                id=((struct Gadget *)msg->IAddress)->GadgetID;
            if(cls==IDCMP_REFRESHWINDOW) {
                GT_BeginRefresh(window);
                GT_EndRefresh(window,TRUE);
            }
            GT_ReplyIMsg(msg);
            if(cls==IDCMP_CLOSEWINDOW || id==13)done=1;
            else if(id==20)legacy=code!=0;
            else if(id==12) {
                for(i=0;i<3;++i)
                    GT_SetGadgetAttrs(fields[i],window,NULL,GTST_String,(ULONG)"",TAG_DONE);
            } else if(id==10 || id==11) {
                DollyIoHints next={0};
                uint32_t *const fields_out[]={&next.mask,&next.buffer_memory,&next.max_transfer};
                int valid=1;
                if(has_device)for(i=0;i<3;++i) {
                    const char *text=(const char *)((struct StringInfo *)fields[i]->SpecialInfo)->Buffer;
                    if(!*text)continue;
                    next.present|=bits[i];
                    if(!number(text,fields_out[i]))valid=0;
                }
                if(valid && dolly_io_hints_valid(next) &&
                   (!has_device || dolly_amiga_io_store(&device,next))) {
                    int previous=legacy_override;
                    legacy_override=legacy;
                    if(id==11 || dolly_amiga_prefs_save())done=1;
                    else {
                        legacy_override=previous;
                        if(has_device)dolly_amiga_io_store(&device,current);
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
