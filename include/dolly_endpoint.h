#ifndef DOLLY_ENDPOINT_H
#define DOLLY_ENDPOINT_H
#include <stdint.h>
typedef enum DollyEndpointKind {
    DOLLY_ENDPOINT_NONE=0,
    DOLLY_ENDPOINT_UNIT=1,
    DOLLY_ENDPOINT_PARTITION=2,
    DOLLY_ENDPOINT_FILE=3
} DollyEndpointKind;
typedef struct DollyEndpoint {
    DollyEndpointKind kind;
    uint8_t device;
    uint8_t unit;
    uint8_t partition;
    char path[256];
} DollyEndpoint;
typedef enum DollyDemoOperation {
    DOLLY_DEMO_SCAN=0,
    DOLLY_DEMO_INVENTORY=1,
    DOLLY_DEMO_COPY=2,
    DOLLY_DEMO_RESTORE=3
} DollyDemoOperation;
void dolly_endpoint_clear(DollyEndpoint *e);
int dolly_endpoint_set_unit(DollyEndpoint *e,unsigned device,unsigned unit);
int dolly_endpoint_set_partition(DollyEndpoint *e,unsigned device,
                                 unsigned unit,unsigned partition);
int dolly_endpoint_set_file(DollyEndpoint *e,const char *path);
int dolly_demo_needs_target(DollyDemoOperation op);
int dolly_demo_can_start(DollyDemoOperation op,
                         const DollyEndpoint *source,
                         const DollyEndpoint *target);
#endif
