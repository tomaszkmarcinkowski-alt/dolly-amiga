#include "dolly_resources.h"
#include <stdint.h>
#include <string.h>
static const char *const disabled[DOLLY_RESOURCE_KIND_COUNT]={
    "Optional GUI backend unavailable; built-in GadTools is still available.",
    "Optional filesystem extension unavailable; basic disk inspection remains available.",
    "Controller database advice unavailable; raw DMA safety checks remain enforced.",
    "Translation unavailable; built-in fallback text remains available.",
    "Extended help unavailable; core commands remain available."
};
static const char *const actions[DOLLY_RESOURCE_KIND_COUNT]={
    "Restore the correct Dolly-release GUI plugin in DOLLY:Plugins/GUI/.",
    "Restore this release's filesystem extension in DOLLY:Plugins/Filesystems/.",
    "Install a matching trusted controller database in DOLLY:Plugins/Devices/. Do not bypass DMA.",
    "Restore the matching catalog in DOLLY:Catalogs/.",
    "Restore the matching help file in DOLLY:Docs/."
};
static const char *const dirs[DOLLY_RESOURCE_KIND_COUNT]={
    "Plugins/GUI/","Plugins/Filesystems/","Plugins/Devices/","Catalogs/","Docs/"
};
const char *dolly_resource_kind_directory(DollyResourceKind kind){
    return (unsigned)kind<DOLLY_RESOURCE_KIND_COUNT?dirs[kind]:0;
}
DollyResourceScanResult dolly_resources_scan_optional(
    const DollyResourceSpec *items,size_t count,
    const char *required_release,uint32_t required_format,
    DollyResourceInspect inspect,void *inspect_context,
    DollyResourceReport report,void *report_context){
    DollyResourceScanResult result={0};
    size_t i;
    char path[256];
    if((count&&!items)||!inspect||!required_release||!required_release[0]||
       strlen(required_release)>80||required_format==0||count>1024u){
        result.status=DOLLY_RESOURCE_SCAN_INVALID;return result;
    }
    for(i=0;i<count;i++){
        DollyPathStatus st;
        DollyResourceProbeResult observed;
        DollyResourceNotice notice;
        const DollyResourceSpec *item=&items[i];
        const char *directory;
        if((unsigned)item->kind>=DOLLY_RESOURCE_KIND_COUNT){
            result.status=DOLLY_RESOURCE_SCAN_INVALID;return result;
        }
        st=dolly_path_resource(item->relative_path,path,sizeof(path));
        if(st!=DOLLY_PATH_OK){
            result.status=st==DOLLY_PATH_TOO_LONG?
                DOLLY_RESOURCE_SCAN_PATH_TOO_LONG:DOLLY_RESOURCE_SCAN_INVALID;
            return result;
        }
        directory=dolly_resource_kind_directory(item->kind);
        if(!directory||strncmp(item->relative_path,directory,strlen(directory))!=0){
            result.status=DOLLY_RESOURCE_SCAN_INVALID;return result;
        }
        observed=inspect(path,inspect_context);
        if(observed.state==DOLLY_RESOURCE_READABLE &&
           observed.integrity_ok==1 && observed.release_id &&
           strcmp(observed.release_id,required_release)==0 &&
           observed.format_version==required_format)continue;
        result.disabled_kinds|=UINT32_C(1)<<(unsigned)item->kind;
        notice.required_release=required_release;
        notice.found_release=observed.release_id;
        notice.missing_path=path;
        notice.disabled_feature=disabled[item->kind];
        notice.suggested_action=actions[item->kind];
        if(observed.state==DOLLY_RESOURCE_ABSENT){
            result.missing_files++;
            notice.code="DOLLY_OPTIONAL_RESOURCE_MISSING";
            notice.headline="Wilki wykradly pliki - te owce odpoczywaja!";
        }else if(observed.state==DOLLY_RESOURCE_UNREADABLE){
            result.unreadable_files++;
            notice.code="DOLLY_OPTIONAL_RESOURCE_UNREADABLE";
            notice.headline="Owce widza plik, lecz nie moga go odczytac!";
        }else if(observed.state==DOLLY_RESOURCE_UNVERIFIED ||
                 observed.state!=DOLLY_RESOURCE_READABLE ||
                 observed.integrity_ok!=1 || !observed.release_id){
            result.unverified_files++;
            notice.code="DOLLY_OPTIONAL_RESOURCE_UNVERIFIED";
            notice.headline="Nieznany paszport pliku - Dolly go nie uruchomi!";
        }else{
            result.incompatible_files++;
            notice.code="DOLLY_OPTIONAL_RESOURCE_VERSION_MISMATCH";
            notice.headline="Plik z innego stada - wersja nie pasuje do Dolly!";
        }
        if(report)report(&notice,report_context);
    }
    return result;
}
