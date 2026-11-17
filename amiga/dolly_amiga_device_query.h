#ifndef DOLLY_AMIGA_DEVICE_QUERY_H
#define DOLLY_AMIGA_DEVICE_QUERY_H
#include <exec/types.h>
/* Called ONLY by user pressing Query. Never by opening a picker,
 * changing its mode, choosing a list entry or resizing a window.
 */
int dolly_amiga_device_query(const char *driver,ULONG unit,
                            char *name,unsigned name_capacity,
                            char *size,unsigned size_capacity);
#endif
