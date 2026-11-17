#include "dolly_endpoint.h"
#include <string.h>
void dolly_endpoint_clear(DollyEndpoint *e) {
    if(e)memset(e,0,sizeof(*e));
}
int dolly_endpoint_set_unit(DollyEndpoint *e,unsigned d,unsigned u) {
    if(!e || d>15u || u>15u)return 0;
    dolly_endpoint_clear(e);
    e->kind=DOLLY_ENDPOINT_UNIT;
    e->device=(uint8_t)d;
    e->unit=(uint8_t)u;
    return 1;
}
int dolly_endpoint_set_partition(DollyEndpoint *e,unsigned d,
                                 unsigned u,unsigned p) {
    if(p>1u || !dolly_endpoint_set_unit(e,d,u))return 0;
    e->kind=DOLLY_ENDPOINT_PARTITION;
    e->partition=(uint8_t)p;
    return 1;
}
int dolly_endpoint_set_logical(DollyEndpoint *e,unsigned logical) {
    if(!e || logical>23u)return 0;
    dolly_endpoint_clear(e);
    e->kind=DOLLY_ENDPOINT_LOGICAL;
    e->logical=(uint8_t)logical;
    return 1;
}
int dolly_endpoint_set_file(DollyEndpoint *e,const char *path) {
    size_t n;
    if(!e || !path)return 0;
    n=strlen(path);
    if(!n || n>=sizeof(e->path))return 0;
    dolly_endpoint_clear(e);
    e->kind=DOLLY_ENDPOINT_FILE;
    memcpy(e->path,path,n+1u);
    return 1;
}
DollyEndpointMode dolly_endpoint_mode(const DollyEndpoint *e) {
    if(!e)return DOLLY_MODE_PHYSICAL;
    switch(e->kind) {
    case DOLLY_ENDPOINT_LOGICAL:return DOLLY_MODE_LOGICAL;
    case DOLLY_ENDPOINT_PARTITION:return DOLLY_MODE_PARTITION;
    case DOLLY_ENDPOINT_FILE:return DOLLY_MODE_FILE;
    default:return DOLLY_MODE_PHYSICAL;
    }
}
int dolly_demo_needs_target(DollyDemoOperation op) {
    return op==DOLLY_DEMO_COPY || op==DOLLY_DEMO_RESTORE ||
           op==DOLLY_DEMO_CREATE_IMAGE;
}
int dolly_demo_can_start(DollyDemoOperation op,
                         const DollyEndpoint *source,
                         const DollyEndpoint *target) {
    if((unsigned)op>4u || !source ||
       source->kind==DOLLY_ENDPOINT_NONE)return 0;
    if(op==DOLLY_DEMO_RESTORE && source->kind!=DOLLY_ENDPOINT_FILE)
        return 0;
    if(dolly_demo_needs_target(op) &&
       (!target || target->kind==DOLLY_ENDPOINT_NONE))return 0;
    if(op==DOLLY_DEMO_CREATE_IMAGE &&
       target->kind!=DOLLY_ENDPOINT_FILE)return 0;
    if(dolly_demo_needs_target(op) &&
       source->kind==DOLLY_ENDPOINT_FILE &&
       target->kind==DOLLY_ENDPOINT_FILE &&
       strcmp(source->path,target->path)==0)return 0;
    if(dolly_demo_needs_target(op) &&
       source->kind==DOLLY_ENDPOINT_LOGICAL &&
       target->kind==DOLLY_ENDPOINT_LOGICAL &&
       ((source->path[0] && target->path[0] &&
          strcmp(source->path,target->path)==0) ||
        ((!source->path[0] || !target->path[0]) &&
         source->logical==target->logical)))return 0;
    if(dolly_demo_needs_target(op) &&
       (source->kind==DOLLY_ENDPOINT_UNIT ||
        source->kind==DOLLY_ENDPOINT_PARTITION) &&
       (target->kind==DOLLY_ENDPOINT_UNIT ||
        target->kind==DOLLY_ENDPOINT_PARTITION) &&
       ((source->path[0] && target->path[0] &&
         strcmp(source->path,target->path)==0) ||
        ((!source->path[0] || !target->path[0]) &&
         source->device==target->device &&
         source->unit==target->unit)) &&
       (source->kind==DOLLY_ENDPOINT_UNIT ||
        target->kind==DOLLY_ENDPOINT_UNIT ||
        source->partition==target->partition))
        return 0;
    /* Logical-vs-physical aliasing is unknown before real discovery:
     * this is a demo validation only, never authorization to write.
     */
    return 1;
}
