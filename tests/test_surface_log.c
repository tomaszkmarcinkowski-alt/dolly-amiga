#include "dolly_surface_log.h"
#include "dolly_scan_range.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    {
        DollyMediaGeometry geometry={.cylinders=100,.heads=4,.sectors_per_track=16};
        uint64_t lba;
        assert(dolly_scan_position("4294967297",&geometry,&lba) && lba==UINT64_C(4294967297));
        assert(dolly_scan_position("1/2/3",&geometry,&lba) && lba==98);
        assert(!dolly_scan_position("1/2/0",&geometry,&lba));
        assert(!dolly_scan_position("100/0/1",&geometry,&lba));
        assert(!dolly_scan_position("1/4/1",&geometry,&lba));
        assert(!dolly_scan_position("18446744073709551616",&geometry,&lba));
    }

    DollySurfaceLog log={0};
    DollyMediaGeometry geometry={0};
    DollySurfaceEvent error={0,1,DOLLY_SURFACE_UNREADABLE,DOLLY_BLOCK_EIO};
    const DollySurfaceLogPage *page;
    char line[DOLLY_LOG_LINE_SIZE];
    unsigned i;
    for(i=0;i<DOLLY_LOG_PAGE_SIZE+1u;++i)
        dolly_surface_log_add(&log,DOLLY_SURFACE_RETRY,UINT64_MAX-i,1,DOLLY_BLOCK_EIO);
    assert(log.first && log.first->count==DOLLY_LOG_PAGE_SIZE);
    assert(log.last && log.last!=log.first && log.last->count==1);
    assert(log.last->previous==log.first && log.first->next==log.last);
    assert(log.last->entries[0].lba==UINT64_MAX-DOLLY_LOG_PAGE_SIZE);
    page=log.first;
    dolly_surface_log_line(line,&page->entries[0],&geometry);
    assert(strstr(line,"18446744073709551615") && strstr(line,"Retry"));
    dolly_surface_log_clear(&log);
    assert(!log.first && !log.last && !log.omitted);
    dolly_surface_log_add(&log,DOLLY_SURFACE_BATCH_FAILED,0,32,DOLLY_BLOCK_ESHORT);
    dolly_surface_log_line(line,&log.first->entries[0],&geometry);
    assert(strstr(line,"LBA 0 +32 sectors") && strstr(line,"Batch failed"));
    dolly_surface_log_clear(&log);
    assert(strstr(line,"CHS unavailable"));
    geometry.cylinders=10;
    geometry.heads=2;
    geometry.sectors_per_track=40;
    geometry.block_count=810;
    dolly_surface_log_geometry(line,&geometry);
    assert(strstr(line,"10 cylinders / 2 heads / 40 sectors per track"));
    dolly_surface_log_line(line,&error,&geometry);
    assert(strstr(line,"LBA 0  CHS 0/0/1"));
    error.lba=79;
    dolly_surface_log_line(line,&error,&geometry);
    assert(strstr(line,"LBA 79  CHS 0/1/40"));
    error.lba=80;
    dolly_surface_log_line(line,&error,&geometry);
    assert(strstr(line,"LBA 80  CHS 1/0/1"));
    error.lba=799;
    dolly_surface_log_line(line,&error,&geometry);
    assert(strstr(line,"LBA 799  CHS 9/1/40"));
    error.lba=800;
    dolly_surface_log_line(line,&error,&geometry);
    assert(strstr(line,"CHS unavailable"));
    geometry.sectors_per_track=0;
    error.lba=1;
    dolly_surface_log_line(line,&error,&geometry);
    assert(strstr(line,"CHS unavailable"));
    puts("Surface Scan log: PASS");
    return 0;
}
