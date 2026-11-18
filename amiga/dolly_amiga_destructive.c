#include "dolly_amiga_destructive.h"
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/gadtools.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <string.h>

static void start_second(struct timerequest *timer) {
    timer->tr_node.io_Command=TR_ADDREQUEST;
    timer->tr_time.tv_secs=1;
    timer->tr_time.tv_micro=0;
    SendIO(&timer->tr_node);
}

int dolly_amiga_confirm_destructive(struct Window *parent,const char *reason) {
    static const char *const warnings[]={
        "You are continuing at your own risk.",
        "Incorrect use can cause data loss. Back up the destination first."
    };
    struct MsgPort *port;
    struct timerequest *timer=NULL;
    struct Window *window=NULL;
    struct Gadget *gadgets=NULL,*last,*proceed,*clock;
    struct NewGadget ng={0};
    struct Screen *screen;
    APTR visual=NULL;
    int opened=0,pending=0,done=0,confirmed=0;
    unsigned seconds=10,i;
    char lines[3][65]={{0}},label[40];
    if(!parent || !reason || !*reason)return 0;
    screen=parent->WScreen;
    port=CreateMsgPort();
    if(!port)return 0;
    timer=(struct timerequest *)CreateIORequest(port,sizeof(*timer));
    if(!timer || OpenDevice(TIMERNAME,UNIT_VBLANK,&timer->tr_node,0))goto out;
    opened=1;
    visual=GetVisualInfo(screen,TAG_DONE);
    if(!visual)goto out;
    window=OpenWindowTags(NULL,WA_PubScreen,(ULONG)screen,
        WA_Title,(ULONG)"Confirm destructive operation",WA_Width,604UL,WA_Height,135UL,
        WA_Left,(ULONG)(parent->LeftEdge>30?30:parent->LeftEdge),
        WA_Top,(ULONG)(screen->BarHeight+3),
        WA_Activate,TRUE,WA_CloseGadget,TRUE,WA_DragBar,TRUE,WA_SmartRefresh,TRUE,
        WA_IDCMP,IDCMP_GADGETUP|IDCMP_CLOSEWINDOW|IDCMP_RAWKEY|IDCMP_REFRESHWINDOW,
        TAG_DONE);
    if(!window)goto out;
    last=CreateContext(&gadgets);
    if(!last)goto out;
    ng.ng_VisualInfo=visual;ng.ng_TextAttr=screen->Font;
    ng.ng_LeftEdge=12;ng.ng_Width=580;ng.ng_Height=12;
    ng.ng_TopEdge=window->BorderTop+4;
    for(i=0;i<3 && *reason;++i) {
        size_t length=strcspn(reason,"\n");
        if(length>64) {
            length=64;
            while(length>40 && reason[length]!=' ')--length;
            if(length==40)length=64;
        }
        memcpy(lines[i],reason,length);
        reason+=length;
        if(*reason=='\n' || *reason==' ')++reason;
        last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)lines[i],TAG_DONE);
        if(!last)goto out;
        ng.ng_TopEdge+=12;
    }
    for(i=0;i<2;++i) {
        last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)warnings[i],TAG_DONE);
        if(!last)goto out;
        ng.ng_TopEdge+=12;
    }
    ng.ng_TopEdge+=3;
    strcpy(label,"Proceed available in 10 s");
    clock=last=CreateGadget(TEXT_KIND,last,&ng,GTTX_Text,(ULONG)label,TAG_DONE);
    if(!last)goto out;
    ng.ng_TopEdge+=14;ng.ng_Height=15;ng.ng_Width=120;
    ng.ng_GadgetID=1;ng.ng_GadgetText=(STRPTR)"Proceed";
    proceed=last=CreateGadget(BUTTON_KIND,last,&ng,GA_Disabled,TRUE,TAG_DONE);
    if(!last)goto out;
    ng.ng_LeftEdge=460;ng.ng_GadgetID=2;ng.ng_GadgetText=(STRPTR)"Cancel";
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
            else if(id==1 && !seconds && !done) {
                confirmed=1;
                done=1;
            }
        }
        if(!done && pending && CheckIO(&timer->tr_node)) {
            WaitIO(&timer->tr_node);pending=0;
            if(timer->tr_node.io_Error)break;
            --seconds;
            snprintf(label,sizeof(label),"Proceed available in %u s",seconds);
            GT_SetGadgetAttrs(clock,window,NULL,GTTX_Text,(ULONG)label,TAG_DONE);
            if(seconds) {
                start_second(timer);pending=1;
            } else GT_SetGadgetAttrs(proceed,window,NULL,GA_Disabled,FALSE,TAG_DONE);
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

