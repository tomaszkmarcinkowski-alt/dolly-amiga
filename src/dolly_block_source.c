#include "dolly_block_source.h"
#include <stdint.h>
#include <stdlib.h>

struct DollyBlockSource {
    uint64_t length;
    DollyBlockReadFn read_fn;
    DollyBlockCloseFn close_fn;
    void *context;
};
const char *dolly_block_status_name(DollyBlockStatus s) {
    switch (s) {
    case DOLLY_BLOCK_OK: return "OK";
    case DOLLY_BLOCK_EINVAL: return "Invalid argument";
    case DOLLY_BLOCK_ERANGE: return "Range exceeds source";
    case DOLLY_BLOCK_ESHORT: return "Short read";
    case DOLLY_BLOCK_EIO: return "I/O error";
    case DOLLY_BLOCK_ENOMEM: return "Out of memory";
    case DOLLY_BLOCK_EUNSUPPORTED: return "Unsupported operation";
    case DOLLY_BLOCK_EDEVICE: return "Device or media unavailable";
    default: return "Unknown error";
    }
}
DollyBlockStatus dolly_block_open_backend(uint64_t capacity,
                                          DollyBlockReadFn read_fn,
                                          DollyBlockCloseFn close_fn,
                                          void *context, DollyBlockSource **out) {
    DollyBlockSource *s;
    if (!out || !read_fn || !close_fn) return DOLLY_BLOCK_EINVAL;
    *out = NULL;
    s = (DollyBlockSource *)malloc(sizeof(*s));
    if (!s) return DOLLY_BLOCK_ENOMEM;
    s->length=capacity;
    s->read_fn=read_fn;
    s->close_fn=close_fn;
    s->context=context;
    *out=s;
    return DOLLY_BLOCK_OK;
}
uint64_t dolly_block_size(const DollyBlockSource *src) {
    return src ? src->length : 0;
}
DollyBlockReadResult dolly_block_read(DollyBlockSource *src,
                                     uint64_t offset, void *buffer, size_t bytes) {
    DollyBlockReadResult r={DOLLY_BLOCK_EINVAL,0};
    uint64_t available;
    size_t want;
    if(!src || (!buffer && bytes)) return r;
    if(offset > src->length) {r.status=DOLLY_BLOCK_ERANGE;return r;}
    if(!bytes) {r.status=DOLLY_BLOCK_OK;return r;}
    available=src->length-offset;
    want=bytes;
    if(available<(uint64_t)want)want=(size_t)available;
    if(!want){r.status=DOLLY_BLOCK_ESHORT;return r;}
    r=src->read_fn(src->context,offset,buffer,want);
    if(r.bytes_read>want || (r.status==DOLLY_BLOCK_OK && r.bytes_read!=want)){
        r.status=DOLLY_BLOCK_EIO;
        r.bytes_read=0;
        return r;
    }
    if(r.status==DOLLY_BLOCK_OK && want!=bytes)r.status=DOLLY_BLOCK_ESHORT;
    if(r.status==DOLLY_BLOCK_OK && r.bytes_read!=bytes)r.status=DOLLY_BLOCK_ESHORT;
    return r;
}
void dolly_block_close(DollyBlockSource *src){
    if(src){src->close_fn(src->context);free(src);}
}
