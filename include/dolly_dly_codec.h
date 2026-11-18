#ifndef DOLLY_DLY_CODEC_H
#define DOLLY_DLY_CODEC_H
#include <stddef.h>
#include <stdint.h>

typedef enum DollyDlyCodec {
    DOLLY_DLY_RAW=0, DOLLY_DLY_PACKBITS=1, DOLLY_DLY_LZSS=2
} DollyDlyCodec;

#define DOLLY_DLY_HASH_SLOTS 4096u
/* Return zero when the output does not fit. NULL output measures PackBits. */
size_t dolly_dly_packbits(const unsigned char *in,size_t size,
                          unsigned char *out,size_t limit);
size_t dolly_dly_lzss(const unsigned char *in,size_t size,
                     unsigned char *out,size_t limit,uint16_t *hash);
int dolly_dly_decode(DollyDlyCodec codec,const unsigned char *in,size_t size,
                     unsigned char *out,size_t raw_size);
#endif
