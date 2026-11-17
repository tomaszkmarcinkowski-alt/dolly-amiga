#ifndef DOLLY_ENDPOINT_H
#define DOLLY_ENDPOINT_H
#include <stdint.h>
typedef enum DollyEndpointKind {
    DOLLY_ENDPOINT_NONE=0,
    DOLLY_ENDPOINT_UNIT=1,       /* physical device/unit */
    DOLLY_ENDPOINT_PARTITION=2,  /* RDB/MBR partition of a physical unit */
    DOLLY_ENDPOINT_FILE=3,
    DOLLY_ENDPOINT_LOGICAL=4     /* DOS mounted logical device */
} DollyEndpointKind;
typedef enum DollyEndpointMode {
    DOLLY_MODE_PHYSICAL=0,
    DOLLY_MODE_LOGICAL=1,
    DOLLY_MODE_PARTITION=2,
    DOLLY_MODE_FILE=3
} DollyEndpointMode;
typedef struct DollyEndpoint {
    DollyEndpointKind kind;
    uint8_t device;
    uint8_t unit;
    uint8_t partition;
    uint8_t logical;
    char path[256];
} DollyEndpoint;
typedef enum DollyDemoOperation {
    DOLLY_DEMO_SCAN=0,
    DOLLY_DEMO_INVENTORY=1,
    DOLLY_DEMO_COPY=2,
    DOLLY_DEMO_RESTORE=3,
    DOLLY_DEMO_CREATE_IMAGE=4
} DollyDemoOperation;
void dolly_endpoint_clear(DollyEndpoint *e);
int dolly_endpoint_set_unit(DollyEndpoint *e,unsigned device,unsigned unit);
int dolly_endpoint_set_partition(DollyEndpoint *e,unsigned device,
                                 unsigned unit,unsigned partition);
int dolly_endpoint_set_logical(DollyEndpoint *e,unsigned logical);
int dolly_endpoint_set_file(DollyEndpoint *e,const char *path);
DollyEndpointMode dolly_endpoint_mode(const DollyEndpoint *e);
int dolly_demo_needs_target(DollyDemoOperation op);
int dolly_demo_can_start(DollyDemoOperation op,
                         const DollyEndpoint *source,
                         const DollyEndpoint *target);
#endif
