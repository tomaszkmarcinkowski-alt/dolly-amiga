#ifndef DOLLY_AMIGA_PREF_NOTIFY_H
#define DOLLY_AMIGA_PREF_NOTIFY_H
/* AmigaDOS V36+ file notifications for ScreenMode and Screen Font.
 * Native AmigaOS only; never included in the host-only core.
 * NOTE: Preference changes are not equivalent to public-screen close events. */
#include "dolly_screen_watch.h"
#include <stdint.h>
typedef struct DollyAmigaPrefNotify DollyAmigaPrefNotify;
/* Returns NULL if port allocation or both subscriptions fail.
 * Missing optional subscription does not prevent core from running. */
DollyAmigaPrefNotify *dolly_amiga_pref_notify_open(void);
/* Bitmask of successful watches: 1 ScreenMode, 2 Screen Font. */
uint32_t dolly_amiga_pref_notify_active(const DollyAmigaPrefNotify *watcher);
/* Bitmask to OR into Exec Wait() mask. Zero if watcher is NULL. */
uint32_t dolly_amiga_pref_notify_signal(const DollyAmigaPrefNotify *watcher);
/* Nonblocking: read and ReplyMsg all notifications on app's UI task.
 * Each event feeds the portable reducer; coalescing happens there. */
void dolly_amiga_pref_notify_drain(DollyAmigaPrefNotify *watcher,
                                  DollyScreenWatch *state);
/* Must be called BEFORE closing event port; EndNotify only on successful
 * requests. No file is written; no system setting is modified. */
void dolly_amiga_pref_notify_close(DollyAmigaPrefNotify *watcher);
#endif
