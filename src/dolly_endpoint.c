#include "dolly_endpoint.h"
#include <string.h>
void dolly_endpoint_clear(DollyEndpoint *e) {
    if(e) memset(e,0,sizeof(*e));
}
int dolly_endpoint_set_unit(DollyEndpoint *e,unsigned d,unsigned u) {
    if(!e || d>1u || u>1u)return 0;
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
int dolly_demo_needs_target(DollyDemoOperation op) {
    return op==DOLLY_DEMO_COPY || op==DOLLY_DEMO_RESTORE;
}
int dolly_demo_can_start(DollyDemoOperation op,
                         const DollyEndpoint *source,
                         const DollyEndpoint *target) {
    if((unsigned)op>3u || !source ||
       source->kind==DOLLY_ENDPOINT_NONE)return 0;
    if(dolly_demo_needs_target(op) &&
       (!target || target->kind==DOLLY_ENDPOINT_NONE))return 0;
    if(dolly_demo_needs_target(op) && target &&
       source->kind==DOLLY_ENDPOINT_FILE &&
       target->kind==DOLLY_ENDPOINT_FILE &&
       strcmp(source->path,target->path)==0)return 0;
    if(dolly_demo_needs_target(op) && target &&
       source->kind!=DOLLY_ENDPOINT_FILE &&
       target->kind!=DOLLY_ENDPOINT_FILE &&
       source->device==target->device &&
       source->unit==target->unit &&
       (source->kind==DOLLY_ENDPOINT_UNIT ||
        target->kind==DOLLY_ENDPOINT_UNIT ||
        source->partition==target->partition))
        return 0;
    return 1;
}
