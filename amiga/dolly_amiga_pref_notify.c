/* Experimental AmigaOS V36+ preferences notify adapter.
 * NOT yet cross-compiled against a real NDK. No writes or screen access.
 * Caller owns all Intuition/Window operations and must handle events only
 * on the main GUI task. */
#include "dolly_amiga_pref_notify.h"
#include <dos/notify.h>
#include <exec/ports.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdlib.h>
#include <string.h>

#define DOLLY_PREF_SCREENMODE 1u
#define DOLLY_PREF_SCREENFONT 2u
struct DollyAmigaPrefNotify {
    struct MsgPort *port;
    struct NotifyRequest req[2];
    uint32_t active;
};
static int start_one(DollyAmigaPrefNotify *w,unsigned index,
                     const char *name,uint32_t tag){
    struct NotifyRequest *req=&w->req[index];
    memset(req,0,sizeof(*req));
    req->nr_Name=(UBYTE *)name;
    req->nr_Flags=NRF_SEND_MESSAGE | NRF_WAIT_REPLY;
    req->nr_stuff.nr_Msg.nr_Port=w->port;
    req->nr_UserData=(ULONG)tag;
    if(!StartNotify(req))return 0;
    w->active|=tag;
    return 1;
}
DollyAmigaPrefNotify *dolly_amiga_pref_notify_open(void){
    DollyAmigaPrefNotify *w=(DollyAmigaPrefNotify *)calloc(1,sizeof(*w));
    if(!w)return NULL;
    w->port=CreateMsgPort();
    if(!w->port){free(w);return NULL;}
    /* On OS 2.x/3.x ENV: is normally RAM-backed, and StartNotify needs
     * the target file to exist. Missing files simply disable that watch. */
    (void)start_one(w,0,"ENV:sys/screenmode.prefs",DOLLY_PREF_SCREENMODE);
    (void)start_one(w,1,"ENV:sys/screenfont.prefs",DOLLY_PREF_SCREENFONT);
    if(w->active==0){
        DeleteMsgPort(w->port);
        free(w);
        return NULL;
    }
    return w;
}
uint32_t dolly_amiga_pref_notify_active(const DollyAmigaPrefNotify *w){
    return w?w->active:0;
}
uint32_t dolly_amiga_pref_notify_signal(const DollyAmigaPrefNotify *w){
    return w?(uint32_t)(1UL<<w->port->mp_SigBit):0;
}
void dolly_amiga_pref_notify_drain(DollyAmigaPrefNotify *w,
                                  DollyScreenWatch *state){
    struct NotifyMessage *msg;
    if(!w || !state)return;
    while((msg=(struct NotifyMessage *)GetMsg(w->port))!=NULL){
        if(msg->nm_Class==NOTIFY_CLASS){
            if(msg->nm_NReq==&w->req[0])
                dolly_screen_watch_event(state,DOLLY_SCREEN_PREFS_CHANGED);
            if(msg->nm_NReq==&w->req[1])
                dolly_screen_watch_event(state,DOLLY_SCREEN_FONT_CHANGED);
        }
        ReplyMsg((struct Message *)msg);
    }
}
void dolly_amiga_pref_notify_close(DollyAmigaPrefNotify *w){
    struct Message *msg;
    if(!w)return;
    /* EndNotify must only be used for requests which StartNotify accepted. */
    if(w->active&DOLLY_PREF_SCREENMODE)EndNotify(&w->req[0]);
    if(w->active&DOLLY_PREF_SCREENFONT)EndNotify(&w->req[1]);
    /* Reply to any remaining messages before deleting the private port. */
    while((msg=GetMsg(w->port))!=NULL)ReplyMsg(msg);
    DeleteMsgPort(w->port);
    free(w);
}
