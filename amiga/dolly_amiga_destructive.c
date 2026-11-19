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
#include <string.h>

static void start_second(struct timerequest *timer) {
    timer->tr_node.io_Command=TR_ADDREQUEST;
    timer->tr_time.tv_secs=1;
    timer->tr_time.tv_micro=0;
    SendIO(&timer->tr_node);
}

static int wrap_lines(struct RastPort *rp,const char *text,LONG width,
                      char lines[][97],unsigned *count) {
    while(*text) {
        size_t limit=strcspn(text,"\n"),n=limit<96?limit:96,word;
        if(*count>=10)return 0;
        while(n && TextLength(rp,(STRPTR)text,n)>width)--n;
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

static int confirm_timed(struct Window *parent,const char *reason,int range) {
    static const char *const warnings[]={
        "You are continuing at your own risk.",
        "Incorrect use can cause data loss. Back up the destination first."
    };
    struct MsgPort *port;
    struct timerequest *timer=NULL;
    struct Window *window=NULL;
    struct Gadget *gadgets=NULL,*last,*proceed,*as_is=NULL,*clock;
    struct NewGadget ng={0};
    struct Screen *screen;
    APTR visual=NULL;
    int opened=0,pending=0,done=0,confirmed=0;
    unsigned seconds=10,i,line_count=0;
    LONG row,width,button_width,left;
    char lines[10][97],label[40];
    if(!parent || !reason || !*reason)return 0;
    screen=parent->WScreen;
    width=(LONG)screen->Width-24;
    if(width>580)width=580;
    if(width<220)return 0;
    row=screen->RastPort.Font->tf_YSize+4;
    button_width=TextLength(&screen->RastPort,(STRPTR)(range?"Fill gap with 0s":"Proceed"),range?16:7)+20;
    if(range && 3*button_width+12>width)return 0;
    if(!wrap_lines(&screen->RastPort,reason,width,&lines[0],&line_count))return 0;
    for(i=0;!range && i<2;++i)
        if(!wrap_lines(&screen->RastPort,warnings[i],width,&lines[0],&line_count))return 0;
    if((LONG)(line_count+2u)*row+30>screen->Height-screen->BarHeight)return 0;
    port=CreateMsgPort();
    if(!port)return 0;
    timer=(struct timerequest *)CreateIORequest(port,sizeof(*timer));
    if(!timer || OpenDevice(TIMERNAME,UNIT_VBLANK,&timer->tr_node,0))goto out;
    opened=1;
    visual=GetVisualInfo(screen,TAG_DONE);
    if(!visual)goto out;
    window=OpenWindowTags(NULL,WA_PubScreen,(ULONG)screen,
        WA_Title,(ULONG)(range?"Partition outside driver range":"Confirm destructive operation"),WA_InnerWidth,(ULONG)(width+12),
        WA_InnerHeight,(ULONG)((line_count+2u)*row+16),
        WA_Left,4UL,
        WA_Top,(ULONG)(screen->BarHeight+3),
        WA_Activate,TRUE,WA_CloseGadget,TRUE,WA_DragBar,TRUE,WA_SmartRefresh,TRUE,
        WA_IDCMP,IDCMP_GADGETUP|IDCMP_CLOSEWINDOW|IDCMP_RAWKEY|IDCMP_REFRESHWINDOW,
        TAG_DONE);
    if(!window)goto out;
    last=CreateContext(&gadgets);
    if(!last)goto out;
    ng.ng_VisualInfo=visual;ng.ng_TextAttr=screen->Font;
    left=window->BorderLeft+6;
    ng.ng_LeftEdge=(WORD)left;ng.ng_Width=(WORD)width;ng.ng_Height=(WORD)row;
    ng.ng_TopEdge=window->BorderTop+4;
    for(i=0;i<line_count;++i) {
        last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)lines[i],TAG_DONE);
        if(!last)goto out;
        ng.ng_TopEdge+=(WORD)row;
    }
    ng.ng_TopEdge+=3;
    strcpy(label,range?"Choices available in 10 s":"Proceed available in 10 s");
    clock=last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)label,TAG_DONE);
    if(!last)goto out;
    ng.ng_TopEdge+=(WORD)(row+2);ng.ng_Width=(WORD)button_width;
    ng.ng_GadgetID=1;ng.ng_GadgetText=(STRPTR)(range?"Fill gap with 0s":"Proceed");
    proceed=last=CreateGadget(BUTTON_KIND,last,&ng,GA_Disabled,TRUE,TAG_DONE);
    if(!last)goto out;
    if(range) {
        ng.ng_LeftEdge=(WORD)(left+(width-button_width)/2);
        ng.ng_GadgetID=3;ng.ng_GadgetText=(STRPTR)"Use as-is";
        as_is=last=CreateGadget(BUTTON_KIND,last,&ng,GA_Disabled,TRUE,TAG_DONE);
        if(!last)goto out;
    }
    ng.ng_LeftEdge=(WORD)(left+width-button_width);ng.ng_GadgetID=2;ng.ng_GadgetText=(STRPTR)"Cancel";
    last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);
    if(!last)goto out;
    AddGList(window,gadgets,(ULONG)-1,-1,NULL);
    RefreshGList(gadgets,window,NULL,-1);
    GT_RefreshWindow(window,NULL);
    start_second(timer);pending=1;
    while(!done) {
        struct IntuiMessage *msg;
        Wait((1UL<<window->UserPort->mp_SigBit)|(1UL<<port->mp_SigBit));
        while((msg=GT_GetIMsg(window->UserPort))!=NULL) {
            ULONG cls=msg->Class;
            UWORD id=cls==IDCMP_GADGETUP && msg->IAddress?
                     ((struct Gadget *)msg->IAddress)->GadgetID:0;
            UWORD code=msg->Code;
            if(cls==IDCMP_REFRESHWINDOW) {
                GT_BeginRefresh(window);
                GT_EndRefresh(window,TRUE);
            }
            GT_ReplyIMsg(msg);
            if(cls==IDCMP_CLOSEWINDOW || (cls==IDCMP_RAWKEY && code==0x45) || id==2)
                done=1;
            else if((id==1 || (range && id==3)) && !seconds && !done) {
                confirmed=id==1?1:2;
                done=1;
            }
        }
        if(!done && pending && CheckIO(&timer->tr_node)) {
            WaitIO(&timer->tr_node);pending=0;
            if(timer->tr_node.io_Error)break;
            --seconds;
            snprintf(label,sizeof(label),range?"Choices available in %u s":"Proceed available in %u s",seconds);
            GT_SetGadgetAttrs(clock,window,NULL,GTTX_Text,(ULONG)label,TAG_DONE);
            if(seconds) {
                start_second(timer);pending=1;
            } else {
                GT_SetGadgetAttrs(proceed,window,NULL,GA_Disabled,FALSE,TAG_DONE);
                if(as_is)GT_SetGadgetAttrs(as_is,window,NULL,GA_Disabled,FALSE,TAG_DONE);
            }
        }
    }
out:
    if(pending) {
        if(!CheckIO(&timer->tr_node))AbortIO(&timer->tr_node);
        WaitIO(&timer->tr_node);
    }
    if(opened)CloseDevice(&timer->tr_node);
    if(timer)DeleteIORequest(&timer->tr_node);
    DeleteMsgPort(port);
    if(window)CloseWindow(window);
    if(gadgets)FreeGadgets(gadgets);
    if(visual)FreeVisualInfo(visual);
    return confirmed;
}


int dolly_amiga_confirm_destructive(struct Window *parent,const char *reason) {
    return confirm_timed(parent,reason,0);
}
int dolly_amiga_confirm_outside_range(struct Window *parent,const char *reason) {
    return confirm_timed(parent,reason,1);
}
