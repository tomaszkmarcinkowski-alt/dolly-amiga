#ifndef DOLLY_DLY_WRITER_H
#define DOLLY_DLY_WRITER_H
#include "dolly_dly_codec.h"
#include <stddef.h>
#include <stdint.h>

/* A sink returns nonzero only if it wrote all bytes. A failed write
 * invalidates the stream. Callers publish only a closed, complete file. */
typedef int (*DollyDlySink)(void *context,const void *data,size_t size);
typedef int (*DollyDlySeek)(void *context,uint32_t offset);
typedef struct DollyDlyWriter {
    uint64_t first_lba,sectors,written;
    uint32_t sector_bytes,batch_bytes,file_bytes,emitted;
    DollyDlyCodec compression;
    unsigned char *packed;
    uint16_t *hash;
    DollyDlySeek seek;
    DollyDlySink sink;
    void *context;
    int failed,finished;
} DollyDlyWriter;

/* Plan a single, uncompressed version-1 FORM DOLY within signed DOS sizes. */
int dolly_dly_init(DollyDlyWriter *writer,uint64_t first_lba,uint64_t sectors,
                   uint32_t sector_bytes,uint32_t batch_bytes);
/* Enable version 2 with a batch-sized buffer and optional LZSS hash table.
 * The supplied seek function must reposition the sink to an absolute offset. */
int dolly_dly_compress(DollyDlyWriter *writer,DollyDlyCodec codec,
                       unsigned char *packed,uint16_t *hash,DollyDlySeek seek);
int dolly_dly_begin(DollyDlyWriter *writer,DollyDlySink sink,void *context);
/* Supply one complete batch, or the exact remaining bytes for the final batch. */
int dolly_dly_append(DollyDlyWriter *writer,const void *data,size_t size);
int dolly_dly_finish(DollyDlyWriter *writer);
int dolly_dly_complete(const DollyDlyWriter *writer);
#endif
