#include "dolly_resources.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct Fixture {
    unsigned calls,notices,versions,formats,unknown,checksum;
} Fixture;
static DollyResourceProbeResult probe(const char *path,void *context){
    Fixture *f=(Fixture *)context;
    DollyResourceProbeResult r={0};
    f->calls++;
    assert(strncmp(path,"DOLLY:",6)==0);
    if(strstr(path,"Good.profile")){
        r.state=DOLLY_RESOURCE_READABLE;
        r.release_id="Dolly-0.1.7+abc123";
        r.format_version=3;r.integrity_ok=1;
        r.actual_crc32=0xcbf43926u;r.actual_bytes=9;
    }else if(strstr(path,"Old.profile")){
        r.state=DOLLY_RESOURCE_READABLE;
        r.release_id="Dolly-0.1.6+old";
        r.format_version=3;r.integrity_ok=1;
    }else if(strstr(path,"WrongABI.profile")){
        r.state=DOLLY_RESOURCE_READABLE;
        r.release_id="Dolly-0.1.7+abc123";
        r.format_version=2;r.integrity_ok=1;
        r.actual_crc32=0xcbf43926u;r.actual_bytes=9;
    }else if(strstr(path,"BadCRC.profile")){
        r.state=DOLLY_RESOURCE_READABLE;r.release_id="Dolly-0.1.7+abc123";
        r.format_version=3;r.integrity_ok=1;r.actual_crc32=0x12345678u;
        r.actual_bytes=9;
    }else if(strstr(path,"NoMetadata.profile")){
        r.state=DOLLY_RESOURCE_READABLE;
        r.release_id=NULL;r.format_version=3;r.integrity_ok=0;
    }else if(strstr(path,"Unreadable.profile")){
        r.state=DOLLY_RESOURCE_UNREADABLE;
    }else if(strstr(path,"Catalogs/")){
        r.state=DOLLY_RESOURCE_ABSENT;
    }else{
        r.state=DOLLY_RESOURCE_UNVERIFIED;
    }
    return r;
}
static void notice(const DollyResourceNotice *n,void *context){
    Fixture *f=(Fixture *)context;f->notices++;
    assert(n->code&&n->headline&&n->missing_path);
    assert(n->disabled_feature&&n->suggested_action);
    assert(!strcmp(n->required_release,"Dolly-0.1.7+abc123"));
    assert(strncmp(n->missing_path,"DOLLY:",6)==0);
    if(!strcmp(n->code,"DOLLY_OPTIONAL_RESOURCE_VERSION_MISMATCH")){
        f->versions++;
        assert(n->found_release);
    }
    if(!strcmp(n->code,"DOLLY_OPTIONAL_RESOURCE_API_MISMATCH")){
        f->formats++;
        assert(n->expected_format==3 && n->found_format==2);
    }
    if(!strcmp(n->code,"DOLLY_OPTIONAL_RESOURCE_UNVERIFIED"))f->unknown++;
    if(!strcmp(n->code,"DOLLY_OPTIONAL_RESOURCE_CHECKSUM_MISMATCH"))f->checksum++;
}
int main(void){
    const DollyResourceSpec files[]={
      {"Plugins/Devices/Good.profile",DOLLY_RESOURCE_DEVICE_DATABASE,3,0xcbf43926u,9},
      {"Plugins/Devices/Old.profile",DOLLY_RESOURCE_DEVICE_DATABASE,3,0xcbf43926u,9},
      {"Plugins/Devices/WrongABI.profile",DOLLY_RESOURCE_DEVICE_DATABASE,3,0xcbf43926u,9},
      {"Plugins/Devices/BadCRC.profile",DOLLY_RESOURCE_DEVICE_DATABASE,3,0xcbf43926u,9},
      {"Plugins/Devices/NoMetadata.profile",DOLLY_RESOURCE_DEVICE_DATABASE,3,0xcbf43926u,9},
      {"Plugins/Devices/Unreadable.profile",DOLLY_RESOURCE_DEVICE_DATABASE,3,0xcbf43926u,9},
      {"Catalogs/polish/dolly.catalog",DOLLY_RESOURCE_CATALOG,3,0xcbf43926u,9}
    };
    const DollyResourceSpec wrong_directory[]={
      {"Plugins/GUI/Old.profile",DOLLY_RESOURCE_DEVICE_DATABASE,3,0xcbf43926u,9}
    };
    Fixture f={0};
    DollyResourceScanResult r=dolly_resources_scan_optional(
        files,sizeof(files)/sizeof(files[0]),"Dolly-0.1.7+abc123",
        probe,&f,notice,&f);
    assert(r.status==DOLLY_RESOURCE_SCAN_OK);
    assert(f.calls==7&&f.notices==6);
    assert(r.missing_files==1&&r.unreadable_files==1);
    assert(r.incompatible_files==2&&r.unverified_files==2);
    assert(f.versions==1&&f.formats==1&&f.unknown==1&&f.checksum==1);
    assert(r.disabled_kinds==(1u<<DOLLY_RESOURCE_DEVICE_DATABASE |
                               1u<<DOLLY_RESOURCE_CATALOG));
    assert(!strcmp(dolly_resource_kind_directory(DOLLY_RESOURCE_DEVICE_DATABASE),
                   "Plugins/Devices/"));
    r=dolly_resources_scan_optional(wrong_directory,1,"Dolly-0.1.7+abc123",
                                     probe,&f,notice,&f);
    assert(r.status==DOLLY_RESOURCE_SCAN_INVALID&&f.calls==7);
    r=dolly_resources_scan_optional(NULL,0,"Dolly-0.1.7+abc123",
                                     probe,&f,notice,&f);
    assert(r.status==DOLLY_RESOURCE_SCAN_OK);
    r=dolly_resources_scan_optional(files,1,NULL,probe,&f,notice,&f);
    assert(r.status==DOLLY_RESOURCE_SCAN_INVALID);
    r=dolly_resources_scan_optional(files,1,"",
                                      probe,&f,notice,&f);
    assert(r.status==DOLLY_RESOURCE_SCAN_INVALID);
    {
        DollyResourceSpec invalid_schema={
            "Plugins/Devices/Good.profile",DOLLY_RESOURCE_DEVICE_DATABASE,0,0xcbf43926u,9
        };
        r=dolly_resources_scan_optional(&invalid_schema,1,
                   "Dolly-0.1.7+abc123",probe,&f,notice,&f);
        assert(r.status==DOLLY_RESOURCE_SCAN_INVALID);
    }
    puts("Optional assets require exact release, schema and integrity checks");
    return 0;
}
