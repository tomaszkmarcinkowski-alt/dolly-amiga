#include "dolly_amiga_destructive.h"
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
        int bold=*text==1 || *text==2;
        if(bold) {
            span=*text==1?"** SOURCE **":"** DESTINATION **";
            length=strlen(span);
        } else {
            while(used<n && text[used]!=1 && text[used]!=2)++used;
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
                      char lines[][97],unsigned *count) {
    char marked[768];
    const char *text=marked;
    if(!mark_roles(body,marked))return 0;
    while(*text) {
        size_t limit=strcspn(text,"\n"),n=limit<96?limit:96,word;
        if(*count>=18)return 0;
        while(n && role_text(rp,text,n,0)>width)--n;
        if(!n && limit)return 0;
        if(n<limit) {
            word=n;
            while(word && text[word]!=' ')--word;
            if(word)n=word;
        }
        memcpy(lines[*count],text,n);
        lines[(*count)++][n]=0;
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
    char lines[18][97],label[40];
    if(!parent || !body || !*body || !count || count>3)return 0;
    screen=parent->WScreen;
    width=(LONG)screen->Width-24;
    if(width>580)width=580;
    if(width<220)return 0;
    measure=screen->RastPort;
    row=measure.Font->tf_YSize+4;
    for(i=0;i<count;++i) {
        LONG needed=TextLength(&measure,(STRPTR)buttons[i],strlen(buttons[i]))+20;
        if(needed>button_width)button_width=needed;
    }
    if((LONG)count*button_width+((LONG)count-1)*6>width)return 0;
    if(!wrap_lines(&measure,body,width,lines,&line_count))return 0;
    if(destructive && !wrap_lines(&measure,
        "You are continuing at your own risk.\n"
        "Incorrect use can cause data loss. Back up the destination first.",
        width,lines,&line_count))return 0;
    rows=line_count+(timed?2u:1u);
    if((LONG)rows*row+30>screen->Height-screen->BarHeight)return 0;
    if(timed) {
        port=CreateMsgPort();
        if(!port)return 0;
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
    ng.ng_VisualInfo=visual;ng.ng_TextAttr=screen->Font;
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
