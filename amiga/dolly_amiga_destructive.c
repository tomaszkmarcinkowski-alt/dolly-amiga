#include "dolly_amiga_fonts.h"
#include "dolly_amiga_destructive.h"
#include "dolly_amiga_warnings.h"
#include "dolly_amiga_io_settings.h"
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <graphics/text.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>

static void start_second(struct timerequest *timer) {
    timer->tr_node.io_Command=TR_ADDREQUEST;
    timer->tr_time.tv_secs=1;
    timer->tr_time.tv_micro=0;
    SendIO(&timer->tr_node);
}

/* Encode role words as single tokens so wrapping cannot split a badge. */
static int mark_roles(const char *text,char marked[768]) {
    size_t n=0;
    int quoted=0;
    while(*text) {
        size_t word=0;
        if(!quoted && (!strncmp(text,"**Rename**",10) || !strncmp(text,"**Keep**",8))) {
            if(n+1>=768)return 0;
            int rename=text[2]=='R';
            marked[n++]=rename?3:4;
            text+=rename?10:8;
            continue;
        }
        while(isalpha((unsigned char)text[word]))++word;
        if(*text=='"')quoted=!quoted;
        if(!quoted && ((word==6 && !strncasecmp(text,"source",6)) ||
           (word==11 && !strncasecmp(text,"destination",11)) ||
           (word==6 && !strncasecmp(text,"target",6)))) {
            if(n+1>=768)return 0;
            marked[n++]=tolower((unsigned char)*text)=='s'?1:2;
        } else {
            if(!word)word=1;
            if(n+word>=768)return 0;
            memcpy(marked+n,text,word);
            n+=word;
        }
        text+=word;
    }
    marked[n]=0;
    return 1;
}

static LONG role_text(struct RastPort *rp,const char *text,size_t n,int draw) {
    LONG width=0;
    while(n) {
        const char *span=text;
        size_t length=1,used=1;
        static const char *const bold_words[]={
            "** SOURCE **","** DESTINATION **","Rename","Keep"
        };
        unsigned token=(unsigned char)*text;
        int bold=token>=1u && token<=4u;
        if(bold) {
            span=bold_words[token-1u];
            length=strlen(span);
        } else {
            while(used<n && ((unsigned char)text[used]<1u ||
                            (unsigned char)text[used]>4u))++used;
            length=used;
        }
        SetSoftStyle(rp,bold?FSF_BOLD:0,FSF_BOLD);
        if(draw)Text(rp,(STRPTR)span,(ULONG)length);
        else width+=TextLength(rp,(STRPTR)span,(ULONG)length);
        text+=used;
        n-=used;
    }
    SetSoftStyle(rp,0,FSF_BOLD);
    return width;
}

static int wrap_lines(struct RastPort *rp,const char *body,LONG width,
                      char lines[][97],unsigned *count,unsigned max_lines) {
    char marked[768];
    const char *text=marked;
    if(!mark_roles(body,marked))return 0;
    while(*text) {
        size_t limit=strcspn(text,"\n"),n=limit<96?limit:96,word;
        if(*count>=max_lines)return 0;
        while(n && role_text(rp,text,n,0)>width)--n;
        if(!n && limit)return 0;
        if(n<limit) {
            word=n;
            while(word && text[word]!=' ')--word;
            if(word)n=word;
        }
        if(lines) {
            memcpy(lines[*count],text,n);
            lines[*count][n]=0;
        }
        ++*count;
        text+=n;
        while(*text==' ')++text;
        if(*text=='\n')++text;
    }
    return 1;
}

static void paint_body(struct Window *w,const struct DrawInfo *info,
                        const char lines[][97],unsigned count,LONG row) {
    unsigned i;
    SetDrMd(w->RPort,JAM1);
    SetAPen(w->RPort,info->dri_Pens[TEXTPEN]);
    for(i=0;i<count;++i) {
        Move(w->RPort,w->BorderLeft+6,w->BorderTop+4+w->RPort->TxBaseline+(LONG)i*row);
        role_text(w->RPort,lines[i],strlen(lines[i]),1);
    }
}

static int run_request(struct Window *parent,const char *title,const char *body,
                       const char *const *buttons,unsigned count,int timed,int destructive) {
    struct MsgPort *port=NULL;
    struct timerequest *timer=NULL;
    struct Window *window=NULL;
    struct Gadget *gadgets=NULL,*last,*choices[3],*clock=NULL;
    struct NewGadget ng={0};
    struct Screen *screen;
    struct DrawInfo *info=NULL;
    struct RastPort measure;
    APTR visual=NULL;
    int opened=0,pending=0,done=0,result=0;
    unsigned seconds=timed?10u:0u,i,line_count=0,rows;
    LONG row,width,button_width=0,left;
    char (*lines)[97],label[40];
    if(!parent || !body || !*body || !count || count>3)return 0;
    screen=parent->WScreen;
    width=(LONG)screen->Width-24;
    if(width>580)width=580;
    if(width<220)return 0;
    measure=screen->RastPort;
    SetFont(&measure,dolly_amiga_font(0));
    row=measure.Font->tf_YSize+4;
    for(i=0;i<count;++i) {
        LONG needed=TextLength(&measure,(STRPTR)buttons[i],strlen(buttons[i]))+20;
        if(needed>button_width)button_width=needed;
    }
    if((LONG)count*button_width+((LONG)count-1)*6>width)return 0;
    lines=malloc(18u*sizeof(*lines));
    if(!lines)return 0;
    if(!wrap_lines(&measure,body,width,lines,&line_count,18))goto out;
    if(destructive && !wrap_lines(&measure,
        "You are continuing at your own risk.\n"
        "Incorrect use can cause data loss. Back up the destination first.",
        width,lines,&line_count,18))goto out;
    rows=line_count+(timed?2u:1u);
    if((LONG)rows*row+30>screen->Height-screen->BarHeight)goto out;
    if(timed) {
        port=CreateMsgPort();
        if(!port)goto out;
        timer=(struct timerequest *)CreateIORequest(port,sizeof(*timer));
        if(!timer || OpenDevice(TIMERNAME,UNIT_VBLANK,&timer->tr_node,0))goto out;
        opened=1;
    }
    visual=GetVisualInfo(screen,TAG_DONE);
    info=GetScreenDrawInfo(screen);
    if(!visual || !info)goto out;
    window=OpenWindowTags(NULL,WA_PubScreen,(ULONG)screen,
        WA_Title,(ULONG)title,WA_InnerWidth,(ULONG)(width+12),
        WA_InnerHeight,(ULONG)((LONG)rows*row+16),
        WA_Left,4UL,WA_Top,(ULONG)(screen->BarHeight+3),
        WA_Activate,TRUE,WA_CloseGadget,TRUE,WA_DragBar,TRUE,WA_SmartRefresh,TRUE,
        WA_IDCMP,IDCMP_GADGETUP|IDCMP_CLOSEWINDOW|IDCMP_RAWKEY|IDCMP_REFRESHWINDOW,TAG_DONE);
    if(!window)goto out;
    last=CreateContext(&gadgets);
    if(!last)goto out;
    dolly_amiga_window_font(window,0);
    ng.ng_VisualInfo=visual;ng.ng_TextAttr=dolly_amiga_text_attr(0);
    left=window->BorderLeft+6;
    ng.ng_LeftEdge=(WORD)left;ng.ng_Width=(WORD)width;ng.ng_Height=(WORD)row;
    ng.ng_TopEdge=(WORD)(window->BorderTop+7+(LONG)line_count*row);
    if(timed) {
        strcpy(label,count==3?"Choices available in 10 s":"Proceed available in 10 s");
        clock=last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)label,TAG_DONE);
        if(!last)goto out;
        ng.ng_TopEdge+=(WORD)(row+2);
    }
    ng.ng_Width=(WORD)button_width;
    for(i=0;i<count;++i) {
        ng.ng_LeftEdge=(WORD)(left+(count==1?(width-button_width)/2:
                              (LONG)i*(width-button_width)/(LONG)(count-1)));
        ng.ng_GadgetID=(UWORD)(i+1u);
        ng.ng_GadgetText=(STRPTR)buttons[i];
        choices[i]=last=CreateGadget(BUTTON_KIND,last,&ng,
                           GA_Disabled,(ULONG)(timed && i+1u<count),TAG_DONE);
        if(!last)goto out;
    }
    AddGList(window,gadgets,(ULONG)-1,-1,NULL);
    RefreshGList(gadgets,window,NULL,-1);
    GT_RefreshWindow(window,NULL);
    paint_body(window,info,(const char (*)[97])lines,line_count,row);
    if(timed){start_second(timer);pending=1;}
    while(!done) {
        struct IntuiMessage *msg;
        Wait((1UL<<window->UserPort->mp_SigBit)|(port?(1UL<<port->mp_SigBit):0));
        while((msg=GT_GetIMsg(window->UserPort))!=NULL) {
            ULONG cls=msg->Class;
            UWORD id=cls==IDCMP_GADGETUP && msg->IAddress?
                     ((struct Gadget *)msg->IAddress)->GadgetID:0;
            UWORD code=msg->Code;
            if(cls==IDCMP_REFRESHWINDOW) {
                GT_BeginRefresh(window);
                paint_body(window,info,(const char (*)[97])lines,line_count,row);
                GT_EndRefresh(window,TRUE);
            }
            GT_ReplyIMsg(msg);
            if(done)continue;
            if(cls==IDCMP_CLOSEWINDOW || (cls==IDCMP_RAWKEY && code==0x45))
                done=1;
            else if(id && id<=count && (!seconds || id==count)) {
                result=id==count && count>1?0:id;
                done=1;
            }
        }
        if(!done && pending && CheckIO(&timer->tr_node)) {
            WaitIO(&timer->tr_node);pending=0;
            if(timer->tr_node.io_Error)break;
            --seconds;
            snprintf(label,sizeof(label),count==3?"Choices available in %u s":"Proceed available in %u s",seconds);
            GT_SetGadgetAttrs(clock,window,NULL,GTTX_Text,(ULONG)label,TAG_DONE);
            if(seconds) {
                start_second(timer);pending=1;
            } else for(i=0;i+1u<count;++i)
                GT_SetGadgetAttrs(choices[i],window,NULL,GA_Disabled,FALSE,TAG_DONE);
        }
    }
out:
    if(pending) {
        if(!CheckIO(&timer->tr_node))AbortIO(&timer->tr_node);
        WaitIO(&timer->tr_node);
    }
    if(opened)CloseDevice(&timer->tr_node);
    if(timer)DeleteIORequest(&timer->tr_node);
    if(port)DeleteMsgPort(port);
    if(window)CloseWindow(window);
    if(gadgets)FreeGadgets(gadgets);
    if(info)FreeScreenDrawInfo(screen,info);
    if(visual)FreeVisualInfo(visual);
    free(lines);
    return result;
}

int dolly_amiga_role_request(struct Window *parent,const char *title,const char *body,
                            const char *first,const char *second,const char *third) {
    const char *buttons[]={first,second,third};
    return run_request(parent,title,body,buttons,third?3u:second?2u:1u,0,0);
}
int dolly_amiga_confirm_destructive(struct Window *parent,const char *reason) {
    const char *buttons[]={"Proceed","Cancel"};
    return run_request(parent,"Confirm destructive operation",reason,buttons,2,1,1);
}
int dolly_amiga_confirm_outside_range(struct Window *parent,const char *reason) {
    const char *buttons[]={"Fill gap with 0s","Use as-is","Cancel"};
    return run_request(parent,"Partition outside driver range",reason,buttons,3,1,0);
}

void dolly_warnings_clear(DollyWarnings *warnings) {
    unsigned i;
    for(i=0;i<warnings->count;++i) {
        free(warnings->items[i]->text);
        free(warnings->items[i]);
    }
    warnings->count=0;
}

DollyWarning *dolly_warning_add(DollyWarnings *warnings,const char *title,
                               const char *details,const char *button) {
    DollyWarning *warning;
    if(warnings->count==DOLLY_WARNING_MAX)return NULL;
    warning=calloc(1,sizeof(*warning));
    if(!warning)return NULL;
    warning->text=malloc(strlen(details)+1);
    if(!warning->text) {
        free(warning);
        return NULL;
    }
    strcpy(warning->text,details);
    snprintf(warning->row,sizeof(warning->row),"  %s",title);
    warning->node.ln_Name=warning->row;
    warning->button=button;
    warnings->items[warnings->count++]=warning;
    return warning;
}

int dolly_warning_risk(DollyWarning *warning,const char *details) {
    char *text=malloc(strlen(details)+1);
    if(!text)return -1;
    strcpy(text,details);
    free(warning->text);
    warning->text=text;
    warning->button="Proceed";
    warning->action=NULL;
    return 0;
}

enum { WARN_LIST=1,WARN_ACTION,WARN_SETTINGS,WARN_NEXT,WARN_CANCEL,WARN_KEEP,WARN_MOUNT };
typedef struct WarningView {
    struct Window *window;
    struct DrawInfo *info;
    struct Gadget *gadgets,*listview,*action,*settings,*next;
    struct List labels;
    APTR visual;
    LONG x,y,width,height,row,button_y,list_min,detail_min;
    unsigned selected,page,lines_per_page,linked_warnings;
    unsigned remaining;
    char timed_label[40];
    int mount_checked;
} WarningView;

static struct Gadget *warning_button(WarningView *v,struct Gadget *last,UWORD id,
                                    LONG x,LONG y,LONG width,const char *label) {
    struct NewGadget ng={(WORD)x,(WORD)y,(WORD)width,(WORD)v->row,(STRPTR)label,
                         dolly_amiga_text_attr(0),id,0,v->visual,NULL};
    return CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);
}

static int warning_controls(WarningView *v,DollyWarnings *warnings,const DollyWarning *warning,
                            unsigned remaining) {
    unsigned i;
    struct Gadget *last;
    struct NewGadget ng={0};
    LONG left=v->window->BorderLeft+6;
    const char *button=warning->button;
    LONG action_width=TextLength(v->window->RPort,(STRPTR)button,strlen(button))+14;
    if(warning->delay_seconds) {
        if(remaining) {
            snprintf(v->timed_label,sizeof(v->timed_label),"%.24s (%u)",button,remaining);
            button=v->timed_label;
        }
        action_width+=TextLength(v->window->RPort,(STRPTR)" (10)",5);
    }
    LONG more_width=TextLength(v->window->RPort,(STRPTR)"More",4)+14;
    const char *alternate=warning->can_keep?"Keep":warning->settings?"Settings":NULL;
    LONG alternate_width=alternate && !warning->confirmed?
                         TextLength(v->window->RPort,(STRPTR)alternate,strlen(alternate))+14:0;
    LONG cancel_width=TextLength(v->window->RPort,(STRPTR)"Cancel",6)+20;
    if(v->gadgets) {
        RemoveGList(v->window,v->gadgets,-1);
        FreeGadgets(v->gadgets);
        v->gadgets=NULL;
    }
    /* GadTools must release the old list before nodes are appended. */
    while(v->linked_warnings<warnings->count) {
        AddTail(&v->labels,&warnings->items[v->linked_warnings]->node);
        ++v->linked_warnings;
    }
    for(i=0;i<warnings->count;++i)
        warnings->items[i]->row[0]=warnings->items[i]->kept?'-':warnings->items[i]->confirmed?'+':' ';
    last=CreateContext(&v->gadgets);
    if(!last || action_width+more_width+alternate_width+20>v->width)return 0;
    ng.ng_LeftEdge=(WORD)left;
    ng.ng_TopEdge=(WORD)v->y;
    ng.ng_Width=(WORD)(v->x-left-6);
    ng.ng_Height=(WORD)v->height;
    ng.ng_GadgetID=WARN_LIST;
    ng.ng_VisualInfo=v->visual;
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    v->listview=last=CreateGadget(LISTVIEW_KIND,last,&ng,GTLV_Labels,(ULONG)&v->labels,
                                GTLV_ShowSelected,0UL,GTLV_Selected,(ULONG)v->selected,
                                GTLV_MakeVisible,(ULONG)v->selected,TAG_DONE);
    if(!last)return 0;
    v->action=last=warning_button(v,last,WARN_ACTION,v->x+4,v->button_y,action_width,button);
    if(!last)return 0;
    v->settings=NULL;
    if(alternate_width) {
        v->settings=last=warning_button(v,last,warning->can_keep?WARN_KEEP:WARN_SETTINGS,
                                       v->x+10+action_width,v->button_y,alternate_width,alternate);
        if(!last)return 0;
    }
    v->next=last=warning_button(v,last,WARN_NEXT,v->x+v->width-more_width-4,
                               v->button_y,more_width,"More");
    if(!last)return 0;
    last=warning_button(v,last,WARN_CANCEL,v->x+v->width-cancel_width,
                         v->y+v->height+5,cancel_width,"Cancel");
    if(!last)return 0;
    if(warnings->mount_destination && warning==warnings->mount_summary) {
        ng.ng_LeftEdge=(WORD)left;
        ng.ng_TopEdge=(WORD)(v->y+v->height+5);
        ng.ng_Width=26;
        ng.ng_Height=(WORD)v->row;
        ng.ng_GadgetID=WARN_MOUNT;
        ng.ng_GadgetText=(STRPTR)"Mount destination volumes";
        ng.ng_Flags=PLACETEXT_RIGHT;
        last=CreateGadget(CHECKBOX_KIND,last,&ng,GTCB_Checked,(ULONG)v->mount_checked,TAG_DONE);
        if(!last)return 0;
    }
    AddGList(v->window,v->gadgets,(ULONG)-1,-1,NULL);
    return 1;
}

static void paint_warning(WarningView *v,
                          const char lines[][97],unsigned count,int can_act,int keep) {
    struct RastPort *rp=v->window->RPort;
    struct TagItem tags[]={
        {GT_VisualInfo,(ULONG)v->visual},{GTBB_Recessed,TRUE},{TAG_DONE,0}
    };
    unsigned i,first=v->page*v->lines_per_page;
    SetDrMd(rp,JAM1);
    SetAPen(rp,v->info->dri_Pens[BACKGROUNDPEN]);
    RectFill(rp,v->window->BorderLeft,v->window->BorderTop,
             v->window->Width-v->window->BorderRight-1,
             v->window->Height-v->window->BorderBottom-1);
    DrawBevelBoxA(rp,v->x,v->y,v->width,v->height,tags);
    SetAPen(rp,v->info->dri_Pens[TEXTPEN]);
    for(i=0;i<v->lines_per_page && first+i<count;++i) {
        Move(rp,v->x+6,v->y+5+rp->TxBaseline+(LONG)i*v->row);
        role_text(rp,lines[first+i],strlen(lines[first+i]),1);
    }
    RefreshGList(v->gadgets,v->window,NULL,-1);
    GT_RefreshWindow(v->window,NULL);
    GT_SetGadgetAttrs(v->action,v->window,NULL,GA_Disabled,(ULONG)!can_act,TAG_DONE);
    if(keep && v->settings)
        GT_SetGadgetAttrs(v->settings,v->window,NULL,GA_Disabled,(ULONG)!can_act,TAG_DONE);
    GT_SetGadgetAttrs(v->next,v->window,NULL,GA_Disabled,(ULONG)(count<=v->lines_per_page),TAG_DONE);
}

static LONG warning_label_width(struct RastPort *rp,const char *text) {
    return TextLength(rp,(STRPTR)text,strlen(text));
}

static void measure_warning_view(WarningView *v,struct RastPort *rp,
                                 const DollyWarnings *warnings) {
    unsigned i;
    v->row=rp->TxHeight+5;
    v->list_min=warning_label_width(rp,"+ Destination")+24;
    v->detail_min=warning_label_width(rp,"Proceed")+14;
    for(i=0;i<warnings->count;++i) {
        const DollyWarning *warning=warnings->items[i];
        LONG width=warning_label_width(rp,warning->button)+14;
        if(warning->delay_seconds)width+=warning_label_width(rp," (10)");
        if(warning->can_keep)width+=warning_label_width(rp,"Keep")+14;
        else if(warning->settings)width+=warning_label_width(rp,"Settings")+14;
        if(width>v->detail_min)v->detail_min=width;
    }
    v->detail_min+=warning_label_width(rp,"More")+34;
    if(v->detail_min<230)v->detail_min=230;
}

static int layout_warning_view(WarningView *v) {
    struct Window *w=v->window;
    LONG width=w->Width-w->BorderLeft-w->BorderRight-18;
    LONG list_width=width/3;
    if(list_width<v->list_min)list_width=v->list_min;
    if(list_width>width-v->detail_min)list_width=width-v->detail_min;
    if(list_width<v->list_min)return 0;
    v->x=w->BorderLeft+12+list_width;
    v->width=width-list_width;
    v->y=w->BorderTop+5;
    v->height=w->Height-w->BorderBottom-v->y-v->row-10;
    v->button_y=v->y+v->height-v->row-4;
    if(v->button_y-v->y<3*v->row+9)return 0;
    v->lines_per_page=(v->button_y-v->y-9)/v->row;
    return 1;
}

int dolly_warnings_review(struct Window *parent,DollyWarnings *warnings) {
    WarningView v={0};
    struct Screen *screen=parent->WScreen;
    const DollyEndpoint *settings=NULL;
    struct RastPort measure=screen->RastPort;
    LONG inner_width,inner_height,min_width,min_height,max_width,max_height;
    struct MsgPort *timer_port=NULL;
    struct timerequest *timer=NULL;
    unsigned count=0,pending=0,line_capacity=0;
    int timer_open=0,ticking=0;
    int result=0,done=0,rebuild=1;
    char (*lines)[97]=NULL;
    if(!warnings->count)return 1;
    for(unsigned i=0;i<warnings->count;++i)
        if(warnings->items[i]->delay_seconds) {
            timer_port=CreateMsgPort();
            if(!timer_port)goto out;
            timer=(struct timerequest *)CreateIORequest(timer_port,sizeof(*timer));
            if(!timer || OpenDevice(TIMERNAME,UNIT_VBLANK,&timer->tr_node,0))goto out;
            timer_open=1;
            break;
        }
    unsigned timed_item=DOLLY_WARNING_MAX;
    if(warnings->mount_destination)v.mount_checked=*warnings->mount_destination;
    SetFont(&measure,dolly_amiga_font(0));
    measure_warning_view(&v,&measure,warnings);
    min_width=v.list_min+v.detail_min+18;
    if(warnings->mount_destination) {
        LONG footer=warning_label_width(&measure,"Mount destination volumes")+
                    warning_label_width(&measure,"Cancel")+60;
        if(footer>min_width)min_width=footer;
    }
    min_height=5*v.row+28;
    max_width=screen->Width;
    max_height=screen->Height-screen->BarHeight-1;
    inner_width=max_width-22;
    if(inner_width>540)inner_width=540;
    if(inner_width<min_width)inner_width=min_width;
    inner_height=max_height-screen->BarHeight-15;
    if(inner_height>10*v.row+28)inner_height=10*v.row+28;
    if(inner_height<min_height)inner_height=min_height;
    v.window=OpenWindowTags(NULL,WA_PubScreen,(ULONG)screen,
        WA_Title,(ULONG)"Dolly - Review warnings",WA_InnerWidth,(ULONG)inner_width,
        WA_InnerHeight,(ULONG)inner_height,WA_Top,(ULONG)screen->BarHeight+1,
        WA_MinWidth,120UL,WA_MinHeight,80UL,WA_MaxWidth,(ULONG)max_width,
        WA_MaxHeight,(ULONG)max_height,WA_SizeGadget,TRUE,WA_SizeBBottom,TRUE,
        WA_Activate,TRUE,WA_DragBar,TRUE,WA_CloseGadget,TRUE,WA_SmartRefresh,TRUE,
        WA_IDCMP,LISTVIEWIDCMP|BUTTONIDCMP|IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW|
                 IDCMP_RAWKEY|IDCMP_NEWSIZE,TAG_DONE);
    if(!v.window)goto out;
    dolly_amiga_window_font(v.window,0);
    if(!layout_warning_view(&v) || !WindowLimits(v.window,
           min_width+v.window->BorderLeft+v.window->BorderRight,
           min_height+v.window->BorderTop+v.window->BorderBottom,
           max_width,max_height))goto out;
    v.visual=GetVisualInfo(screen,TAG_DONE);
    v.info=GetScreenDrawInfo(screen);
    if(!v.visual || !v.info)goto out;
    v.labels.lh_Head=(struct Node *)&v.labels.lh_Tail;
    v.labels.lh_TailPred=(struct Node *)&v.labels.lh_Head;
    while(!done) {
        struct IntuiMessage *msg;
        DollyWarning *warning=warnings->items[v.selected];
        const int timed=v.selected==pending && warning->delay_seconds!=0;
        if(timed && timed_item!=pending) {
            timed_item=pending;
            v.remaining=warning->delay_seconds;
            start_second(timer);
            ticking=1;
        }
        if(!timed && !ticking)v.remaining=0;
        unsigned remaining=timed?v.remaining:0;
        if(rebuild) {
            unsigned needed=0;
            if(!wrap_lines(v.window->RPort,warning->text,v.width-12,NULL,&needed,768))break;
            if(needed>line_capacity) {
                char (*larger)[97]=realloc(lines,needed*sizeof(*lines));
                if(!larger)break;
                lines=larger;
                line_capacity=needed;
            }
            count=0;
            if(!wrap_lines(v.window->RPort,warning->text,v.width-12,lines,&count,line_capacity))break;
            if(!warning_controls(&v,warnings,warning,remaining))break;
            paint_warning(&v,(const char (*)[97])lines,count,v.selected==pending && !remaining,warning->can_keep);
            rebuild=0;
        }
        Wait((1UL<<v.window->UserPort->mp_SigBit) |
             (ticking?(1UL<<timer_port->mp_SigBit):0UL));
        while((msg=GT_GetIMsg(v.window->UserPort))!=NULL) {
            ULONG cls=msg->Class;
            UWORD code=msg->Code,id=0;
            if(cls==IDCMP_GADGETUP && msg->IAddress)id=((struct Gadget *)msg->IAddress)->GadgetID;
            if(cls==IDCMP_REFRESHWINDOW) {
                GT_BeginRefresh(v.window);
                GT_EndRefresh(v.window,TRUE);
            }
            GT_ReplyIMsg(msg);
            if(cls==IDCMP_CLOSEWINDOW || (cls==IDCMP_RAWKEY && code==0x45) || id==WARN_CANCEL) {
                done=1;
                break;
            }
            if(cls==IDCMP_NEWSIZE) {
                if(!layout_warning_view(&v))done=1;
                v.page=0;
                rebuild=1;
                continue;
            }
            if(id==WARN_MOUNT)v.mount_checked=code!=0;
            if(done || rebuild)continue;
            if(cls==IDCMP_REFRESHWINDOW)
                paint_warning(&v,(const char (*)[97])lines,count,v.selected==pending && !remaining,warning->can_keep);
            else if(id==WARN_LIST && code<warnings->count) {
                v.selected=code;
                v.page=0;
                rebuild=1;
            } else if(id==WARN_NEXT) {
                if(++v.page*v.lines_per_page>=count)v.page=0;
                paint_warning(&v,(const char (*)[97])lines,count,v.selected==pending && !remaining,warning->can_keep);
            } else if(id==WARN_SETTINGS && warning->settings) {
                settings=warning->settings;
                done=1;
            } else if((id==WARN_ACTION || (id==WARN_KEEP && warning->can_keep)) &&
                      v.selected==pending && !remaining) {
                int action_result=id==WARN_ACTION && warning->action?warning->action(warning):1;
                if(action_result<0){done=1;continue;}
                if(action_result>0) {
                    warning->confirmed=1;
                    warning->kept=id==WARN_KEEP;
                    if(++pending==warnings->count){done=1;result=1;continue;}
                    v.selected=pending;
                }
                v.page=0;
                rebuild=1;
            }
        }
        if(!done && ticking && CheckIO(&timer->tr_node)) {
            WaitIO(&timer->tr_node);
            ticking=0;
            if(timer->tr_node.io_Error)break;
            --v.remaining;
            if(v.selected==timed_item)rebuild=1;
            if(v.remaining) {
                start_second(timer);
                ticking=1;
            }
        }
    }
out:
    if(ticking) {
        if(!CheckIO(&timer->tr_node))AbortIO(&timer->tr_node);
        WaitIO(&timer->tr_node);
    }
    if(timer_open)CloseDevice(&timer->tr_node);
    if(timer)DeleteIORequest(&timer->tr_node);
    if(timer_port)DeleteMsgPort(timer_port);
    if(v.window)CloseWindow(v.window);
    if(v.gadgets)FreeGadgets(v.gadgets);
    if(v.visual)FreeVisualInfo(v.visual);
    if(v.info)FreeScreenDrawInfo(screen,v.info);
    free(lines);
    if(settings)dolly_amiga_io_settings(screen,settings);
    if(result && warnings->mount_destination)*warnings->mount_destination=v.mount_checked;
    if(!result)ActivateWindow(parent);
    return result;
}
