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
    if(p>15u || !dolly_endpoint_set_unit(e,d,u))return 0;
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
/* Physical and partition endpoint paths share a stable
 * "driver.device / Unit N" prefix; the optional third component
 * identifies a partition. Do not confuse unit 1 with unit 10.
 */
static int same_physical_unit(const DollyEndpoint *a,
                              const DollyEndpoint *b) {
    const char *ua,*ub,*enda,*endb;
    size_t len_a,len_b;
    if(a->path[0] && b->path[0]) {
        ua=strstr(a->path," / Unit ");
        ub=strstr(b->path," / Unit ");
        if(!ua || !ub)return 0;
        ua+=8;ub+=8;
        if(*ua<'0' || *ua>'9' || *ub<'0' || *ub>'9')
            return 0;
        enda=ua;
        while(*enda>='0' && *enda<='9')++enda;
        endb=ub;
        while(*endb>='0' && *endb<='9')++endb;
        if(*enda && strncmp(enda," / ",3)!=0)return 0;
        if(*endb && strncmp(endb," / ",3)!=0)return 0;
        len_a=(size_t)(enda-a->path);
        len_b=(size_t)(endb-b->path);
        return len_a==len_b && strncmp(a->path,b->path,len_a)==0;
    }
    return a->device==b->device && a->unit==b->unit;
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
       same_physical_unit(source,target) &&
       (source->kind==DOLLY_ENDPOINT_UNIT ||
        target->kind==DOLLY_ENDPOINT_UNIT ||
        source->partition==target->partition))
        return 0;
    /* Logical-vs-physical aliasing is unknown before real discovery:
     * this is a demo validation only, never authorization to write.
     */
    return 1;
}
