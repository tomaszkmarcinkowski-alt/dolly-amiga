#include "dolly_selfcheck.h"
#include "dolly_checksum.h"
#include <string.h>
DollySelfCheckReport dolly_selfcheck_stream(
    DollySelfRead reader,void *ctx,DollySelfCheckExpected expected){
    DollySelfCheckReport report={0};
    DollyChecksum checksum;
    unsigned char chunk[1024];
    if(!reader || !expected.running_release || !expected.manifest_release ||
       !expected.running_release[0] || !expected.manifest_release[0] ||
       expected.size==0){
        report.result=DOLLY_SELF_INVALID;return report;
    }
    if(strcmp(expected.running_release,expected.manifest_release)!=0){
        report.result=DOLLY_SELF_VERSION_MISMATCH;return report;
    }
    dolly_checksum_init(&checksum);
    for(;;){
        long count=reader(ctx,chunk,sizeof(chunk));
        if(count<0 || (unsigned long)count>sizeof(chunk)){
            report.result=DOLLY_SELF_READ_FAILED;return report;
        }
        if(count==0)break;
        if(dolly_checksum_update(&checksum,chunk,(size_t)count)!=0){
            report.result=DOLLY_SELF_READ_FAILED;return report;
        }
        if(checksum.bytes>expected.size){
            report.actual_size=checksum.bytes;
            report.result=DOLLY_SELF_SIZE_MISMATCH;return report;
        }
    }
    report.actual_size=checksum.bytes;
    report.actual_crc32=dolly_checksum_final(&checksum);
    if(report.actual_size!=expected.size)
        report.result=DOLLY_SELF_SIZE_MISMATCH;
    else if(report.actual_crc32!=expected.crc32)
        report.result=DOLLY_SELF_CRC_MISMATCH;
    else report.result=DOLLY_SELF_OK;
    return report;
}
const char *dolly_selfcheck_message(DollySelfCheckResult result){
    switch(result){
    case DOLLY_SELF_OK:
        return "Dolly executable CRC32 and version match.";
    case DOLLY_SELF_INVALID:
        return "Missing or invalid Dolly checksum metadata. Disk functions disabled.";
    case DOLLY_SELF_OPEN_FAILED:
        return "Cannot open the running Dolly executable for checking. Disk functions disabled.";
    case DOLLY_SELF_READ_FAILED:
        return "Cannot read the Dolly executable completely. Disk functions disabled.";
    case DOLLY_SELF_VERSION_MISMATCH:
        return "Dolly executable and release manifest are from different versions. Disk functions disabled.";
    case DOLLY_SELF_SIZE_MISMATCH:
        return "Dolly executable file size differs from this release. Disk functions disabled.";
    case DOLLY_SELF_CRC_MISMATCH:
        return "Wilki pogryzly Dolly! Executable checksum differs. Reinstall this release.";
    default:
        return "Unknown self-check result. Disk functions disabled.";
    }
}
