#include "dolly_screen_watch.h"
#include <stddef.h>
void dolly_screen_watch_init(DollyScreenWatch *w,int screen_available,
                             int notify_available){
    if(!w)return;
    w->pending_actions=0;
    w->revision=0;
    w->screen_valid=(uint8_t)(screen_available!=0);
    w->windows_open=0;
    w->notification_available=(uint8_t)(notify_available!=0);
}
void dolly_screen_watch_set_windows(DollyScreenWatch *w,int open){
    if(w)w->windows_open=(uint8_t)(open!=0);
}
void dolly_screen_watch_event(DollyScreenWatch *w,DollyScreenEvent e){
    if(!w || (unsigned)e>=DOLLY_SCREEN_EVENT_COUNT)return;
    w->revision++;
    if(e==DOLLY_SCREEN_WORKBENCH_CLOSING){
        /* Invalidate screen immediately. Owner must close all windows and
         * release any locks BEFORE dereferencing the screen again. */
        w->screen_valid=0;
        w->pending_actions=DOLLY_SCREEN_ACTION_CLOSE_WINDOWS |
                           DOLLY_SCREEN_ACTION_ACQUIRE_SCREEN;
        return;
    }
    if(e==DOLLY_SCREEN_AVAILABLE){
        w->screen_valid=1;
        w->pending_actions|=DOLLY_SCREEN_ACTION_ACQUIRE_SCREEN |
                             DOLLY_SCREEN_ACTION_REMEASURE |
                             DOLLY_SCREEN_ACTION_REBUILD_WINDOWS;
        return;
    }
    /* Changes while the screen is absent must not trigger unsafe redraws;
     * carry them forward until an AVAILABLE event/validated screen lock. */
    if(!w->screen_valid){
        w->pending_actions|=DOLLY_SCREEN_ACTION_ACQUIRE_SCREEN;
        return;
    }
    if(e==DOLLY_SCREEN_PREFS_CHANGED){
        /* Preferences changed; Workbench may be reopened with a new Screen.
         * Never assume the old pointer persists; reacquire under LockPubScreen. */
        w->pending_actions|=DOLLY_SCREEN_ACTION_ACQUIRE_SCREEN;
    }
    w->pending_actions|=DOLLY_SCREEN_ACTION_REMEASURE |
                         DOLLY_SCREEN_ACTION_REBUILD_WINDOWS;
}
uint32_t dolly_screen_watch_take_actions(DollyScreenWatch *w){
    uint32_t result;
    if(!w)return 0;
    result=w->pending_actions;
    w->pending_actions=0;
    return result;
}
const char *dolly_screen_event_name(DollyScreenEvent e){
    switch(e){
    case DOLLY_SCREEN_PREFS_CHANGED:return "screen preferences";
    case DOLLY_SCREEN_FONT_CHANGED:return "screen font";
    case DOLLY_SCREEN_WORKBENCH_CLOSING:return "screen closing";
    case DOLLY_SCREEN_AVAILABLE:return "screen available";
    case DOLLY_SCREEN_WINDOW_RESIZED:return "window resized";
    case DOLLY_SCREEN_USER_FONT_CHANGED:return "Dolly font changed";
    default:return "unknown screen event";
    }
}
