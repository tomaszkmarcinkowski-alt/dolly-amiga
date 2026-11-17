#ifndef DOLLY_GUI_BACKEND_H
#define DOLLY_GUI_BACKEND_H
/* Design contract only: MVP uses builtin GadTools exclusively;
 * ReAction and MUI are strictly post-MVP external options.
 * No dynamic plugin loader has been implemented.
 * No Intuition/GadTools/ReAction/MUI headers are allowed here. */
#include <stdint.h>
#define DOLLY_GUI_BACKEND_ABI 1u
typedef enum DollyGuiBackendId {
    DOLLY_GUI_BACKEND_GADTOOLS=1,
    DOLLY_GUI_BACKEND_REACTION=2,
    DOLLY_GUI_BACKEND_MUI=3
} DollyGuiBackendId;
typedef struct DollyGuiHostV1 {
    uint32_t structure_size,abi_version;
    void *host_context; /* owned by Dolly, never retained after shutdown */
    void (*report_error)(void *host,const char *code,const char *message);
    void (*request_relayout)(void *host); /* screen/font change */
} DollyGuiHostV1;
typedef struct DollyGuiBackendV1 {
    uint32_t structure_size,abi_version,backend_id;
    const char *name;
    uint32_t required_os_version; /* documented minimum of optional backend */
    void *(*open)(const DollyGuiHostV1 *host); /* NULL => cannot use backend */
    void (*close)(void *context);
    int (*show_main)(void *context);
    int (*show_notice)(void *context,const char *code,const char *title,
                       const char *details); /* must not clip */
    void (*screen_change)(void *context); /* screen is reacquired by backend */
} DollyGuiBackendV1;
/* Host must validate structure_size and abi_version before calling slots.
 * GadTools backend is linked into main executable. Future optional modules
 * may expose this table through a verified AmigaOS loader mechanism. */
#endif
