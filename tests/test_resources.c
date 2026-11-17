#include "dolly_resources.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct Probe {
    unsigned calls,notices;
    unsigned saw_gui,saw_devices,saw_catalog;
} Probe;
static int exists(const char *path,void *ctx){
    Probe *p=(Probe *)ctx;p->calls++;
    assert(!strncmp(path,"DOLLY:",6));
    return strstr(path,"Plugins/Filesystems/")!=NULL;
}
static void notice(const DollyResourceNotice *n,void *ctx){
    Probe *p=(Probe *)ctx;p->notices++;
    assert(!strcmp(n->code,"DOLLY_OPTIONAL_RESOURCE_MISSING"));
    assert(strstr(n->missing_path,"DOLLY:")==n->missing_path);
    assert(strstr(n->headline,"Wilki")!=NULL);
    assert(strlen(n->disabled_feature)>20);
    assert(strlen(n->suggested_action)>20);
    if(strstr(n->missing_path,"Plugins/GUI/"))p->saw_gui++;
    if(strstr(n->missing_path,"Plugins/Devices/"))p->saw_devices++;
    if(strstr(n->missing_path,"Catalogs/"))p->saw_catalog++;
}
int main(void){
    const DollyResourceSpec list[]={
        {"Plugins/GUI/ReAction.module",DOLLY_RESOURCE_GUI},
        {"Plugins/Filesystems/OFS.extension",DOLLY_RESOURCE_FILESYSTEM},
        {"Plugins/Devices/ClassicIDE.profile",DOLLY_RESOURCE_DEVICE_DATABASE},
        {"Catalogs/polish/dolly.catalog",DOLLY_RESOURCE_CATALOG}
    };
    const DollyResourceSpec invalid[]={
        {"../SYS:Startup-Sequence",DOLLY_RESOURCE_GUI}
    };
    Probe p={0};
    DollyResourceScanResult r=dolly_resources_scan_optional(
        list,sizeof(list)/sizeof(list[0]),exists,&p,notice,&p);
    assert(r.status==DOLLY_RESOURCE_SCAN_OK);
    assert(p.calls==4&&p.notices==3);
    assert(p.saw_gui==1&&p.saw_devices==1&&p.saw_catalog==1);
    assert((r.missing_kinds&(1u<<DOLLY_RESOURCE_GUI))!=0);
    assert((r.missing_kinds&(1u<<DOLLY_RESOURCE_FILESYSTEM))==0);
    assert(r.missing_files==3);
    assert(!strcmp(dolly_resource_kind_directory(DOLLY_RESOURCE_DEVICE_DATABASE),
                   "Plugins/Devices/"));
    r=dolly_resources_scan_optional(invalid,1,exists,&p,notice,&p);
    assert(r.status==DOLLY_RESOURCE_SCAN_INVALID&&p.calls==4);
    r=dolly_resources_scan_optional(NULL,0,exists,&p,notice,&p);
    assert(r.status==DOLLY_RESOURCE_SCAN_OK);
    puts("Optional resource degradation and diagnostics tests passed");
    return 0;
}
