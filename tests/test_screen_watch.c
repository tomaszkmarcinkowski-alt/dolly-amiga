#include "dolly_screen_watch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    DollyScreenWatch w;
    uint32_t actions;
    dolly_screen_watch_init(&w,1,1);
    assert(w.screen_valid&&w.notification_available);
    dolly_screen_watch_set_windows(&w,1);
    dolly_screen_watch_event(&w,DOLLY_SCREEN_FONT_CHANGED);
    dolly_screen_watch_event(&w,DOLLY_SCREEN_WINDOW_RESIZED);
    actions=dolly_screen_watch_take_actions(&w);
    assert((actions&DOLLY_SCREEN_ACTION_REMEASURE)!=0);
    assert((actions&DOLLY_SCREEN_ACTION_REBUILD_WINDOWS)!=0);
    assert((actions&DOLLY_SCREEN_ACTION_CLOSE_WINDOWS)==0);
    assert(dolly_screen_watch_take_actions(&w)==0);
    dolly_screen_watch_event(&w,DOLLY_SCREEN_PREFS_CHANGED);
    actions=dolly_screen_watch_take_actions(&w);
    assert(actions&DOLLY_SCREEN_ACTION_ACQUIRE_SCREEN);
    /* During loss, never redraw on a stale Screen pointer. */
    dolly_screen_watch_event(&w,DOLLY_SCREEN_WORKBENCH_CLOSING);
    dolly_screen_watch_event(&w,DOLLY_SCREEN_FONT_CHANGED);
    actions=dolly_screen_watch_take_actions(&w);
    assert(!w.screen_valid);
    assert(actions&DOLLY_SCREEN_ACTION_CLOSE_WINDOWS);
    assert((actions&DOLLY_SCREEN_ACTION_REBUILD_WINDOWS)==0);
    dolly_screen_watch_event(&w,DOLLY_SCREEN_AVAILABLE);
    actions=dolly_screen_watch_take_actions(&w);
    assert(w.screen_valid);
    assert(actions&DOLLY_SCREEN_ACTION_ACQUIRE_SCREEN);
    assert(actions&DOLLY_SCREEN_ACTION_REMEASURE);
    assert(actions&DOLLY_SCREEN_ACTION_REBUILD_WINDOWS);
    dolly_screen_watch_init(&w,1,0); /* OS2.x without optional library */
    dolly_screen_watch_event(&w,DOLLY_SCREEN_USER_FONT_CHANGED);
    actions=dolly_screen_watch_take_actions(&w);
    assert(actions&DOLLY_SCREEN_ACTION_REMEASURE);
    assert(!w.notification_available);
    assert(!strcmp(dolly_screen_event_name(DOLLY_SCREEN_FONT_CHANGED),"screen font"));
    assert(dolly_screen_watch_take_actions(NULL)==0);
    puts("Screen change watcher state tests passed");
    return 0;
}
