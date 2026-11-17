#include "dolly_probe.h"
#include <assert.h>
#include <stdio.h>
static DollyProbeResult probe_bytes(const unsigned char *data, size_t n, DollyProbeError *err) {
    FILE *f = tmpfile(); DollyProbeResult p;
    assert(f); assert(fwrite(data, 1, n, f) == n); fflush(f);
    *err = dolly_probe_stream(f, &p); fclose(f); return p;
}
int main(void) {
    DollyProbeError err; DollyProbeResult p;
    const unsigned char good[] = {'F','O','R','M',0,0,0,4,'D','O','L','Y'};
    const unsigned char bad[] = {'F','O','R','M',0,0,0,9,'D','O','L','Y'};
    const unsigned char partial[] = {'F','O','R','M',0,0,0,4};
    const unsigned char raw[] = {1,2,3};
    p=probe_bytes(good,sizeof(good),&err);assert(err==DOLLY_PROBE_OK && p.kind==DOLLY_IMAGE_IFF_DOLY);
    p=probe_bytes(bad,sizeof(bad),&err);assert(err==DOLLY_PROBE_INVALID_IFF);
    p=probe_bytes(partial,sizeof(partial),&err);assert(err==DOLLY_PROBE_INVALID_IFF);
    p=probe_bytes(raw,sizeof(raw),&err);assert(err==DOLLY_PROBE_OK && p.kind==DOLLY_IMAGE_RAW);
    puts("4 probe tests passed");return 0;
}
