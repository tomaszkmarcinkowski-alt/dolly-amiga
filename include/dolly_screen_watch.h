#ifndef DOLLY_SCREEN_WATCH_H
#define DOLLY_SCREEN_WATCH_H
/* Portable reducer for Amiga screen/preferences events.
 * No Intuition or DOS symbols: backends deliver normalized notifications. */
#include <stdint.h>
typedef enum DollyScreenEvent {
    DOLLY_SCREEN_PREFS_CHANGED=0, /* ENV:sys/screenmode.prefs */
    DOLLY_SCREEN_FONT_CHANGED,    /* ENV:sys/screenfont.prefs */
    DOLLY_SCREEN_WORKBENCH_CLOSING, /* optional ScreenNotify integration */
    DOLLY_SCREEN_AVAILABLE,      /* verified new screen, no stale pointer */
    DOLLY_SCREEN_WINDOW_RESIZED, /* IDCMP_CHANGEWINDOW or equivalent */
    DOLLY_SCREEN_USER_FONT_CHANGED,
    DOLLY_SCREEN_EVENT_COUNT
} DollyScreenEvent;
typedef enum DollyScreenAction {
    DOLLY_SCREEN_ACTION_NONE=0,
    DOLLY_SCREEN_ACTION_CLOSE_WINDOWS=1,
    DOLLY_SCREEN_ACTION_ACQUIRE_SCREEN=2,
    DOLLY_SCREEN_ACTION_REMEASURE=4,
    DOLLY_SCREEN_ACTION_REBUILD_WINDOWS=8,
    DOLLY_SCREEN_ACTION_SHOW_FALLBACK_NOTICE=16
} DollyScreenAction;
typedef struct DollyScreenWatch {
    uint32_t pending_actions;
    uint32_t revision;
    uint8_t screen_valid;
    uint8_t windows_open;
    uint8_t notification_available;
} DollyScreenWatch;
/* Call on the app's own UI task; queue and coalesce raw notifications
 * into one plan. NEVER reuse a Screen* after CLOSING. */
void dolly_screen_watch_init(DollyScreenWatch *watch,int screen_available,
                             int screen_notify_available);
void dolly_screen_watch_event(DollyScreenWatch *watch,DollyScreenEvent event);
uint32_t dolly_screen_watch_take_actions(DollyScreenWatch *watch);
void dolly_screen_watch_set_windows(DollyScreenWatch *watch,int open);
const char *dolly_screen_event_name(DollyScreenEvent event);
#endif
