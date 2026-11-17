#include "dolly_resources.h"
#include <stdint.h>
static const char *const disabled[DOLLY_RESOURCE_KIND_COUNT]={
    "Optional GUI backend is unavailable; built-in GadTools remains available.",
    "Optional filesystem extension is unavailable; basic disk inspection remains available.",
    "Automatic controller-profile suggestions are unavailable; unverified DMA reads remain blocked.",
    "Translated messages are unavailable; built-in fallback language remains available.",
    "Extended help is unavailable; core commands remain available."
};
static const char *const actions[DOLLY_RESOURCE_KIND_COUNT]={
    "Install this optional GUI addon under DOLLY:Plugins/GUI/ or select GadTools.",
    "Reinstall the filesystem addon under DOLLY:Plugins/Filesystems/.",
    "Install a compatible device profile under DOLLY:Plugins/Devices/; do not bypass DMA checks.",
    "Restore the missing catalog under DOLLY:Catalogs/.",
    "Restore the help file under DOLLY:Docs/."
};
static const char *const directories[DOLLY_RESOURCE_KIND_COUNT]={
    "Plugins/GUI/","Plugins/Filesystems/","Plugins/Devices/","Catalogs/","Docs/"
};
const char *dolly_resource_kind_directory(DollyResourceKind kind){
    return (unsigned)kind<DOLLY_RESOURCE_KIND_COUNT?directories[kind]:0;
}
DollyResourceScanResult dolly_resources_scan_optional(
    const DollyResourceSpec *items,size_t count,
    DollyResourceExists exists,void *exists_context,
    DollyResourceReport report,void *report_context){
    DollyResourceScanResult result={0};
    size_t i;
    char path[256];
    if((count&&!items)||!exists||count>1024u){
        result.status=DOLLY_RESOURCE_SCAN_INVALID;return result;
    }
    for(i=0;i<count;i++){
        DollyPathStatus st;
        const DollyResourceSpec *item=&items[i];
        DollyResourceNotice notice;
        if((unsigned)item->kind>=DOLLY_RESOURCE_KIND_COUNT){
            result.status=DOLLY_RESOURCE_SCAN_INVALID;return result;
        }
        st=dolly_path_resource(item->relative_path,path,sizeof(path));
        if(st!=DOLLY_PATH_OK){
            result.status=st==DOLLY_PATH_TOO_LONG?
                 DOLLY_RESOURCE_SCAN_PATH_TOO_LONG:DOLLY_RESOURCE_SCAN_INVALID;
            return result;
        }
        if(exists(path,exists_context))continue;
        result.missing_files++;
        result.missing_kinds|=UINT32_C(1)<<(unsigned)item->kind;
        notice.code="DOLLY_OPTIONAL_RESOURCE_MISSING";
        notice.headline="Wilki wykradly pliki - niektore owce nie moga pracowac!";
        notice.missing_path=path;
        notice.disabled_feature=disabled[item->kind];
        notice.suggested_action=actions[item->kind];
        if(report)report(&notice,report_context);
    }
    return result;
}
