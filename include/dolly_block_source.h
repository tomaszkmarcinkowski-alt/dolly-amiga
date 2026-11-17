#ifndef DOLLY_BLOCK_SOURCE_H
#define DOLLY_BLOCK_SOURCE_H
/* Portable, byte-addressed, READ-ONLY BlockSource. No write interface. */
#include <stddef.h>
#include <stdint.h>

typedef struct DollyBlockSource DollyBlockSource;
typedef enum DollyBlockStatus {
    DOLLY_BLOCK_OK = 0,
    DOLLY_BLOCK_EINVAL,
    DOLLY_BLOCK_ERANGE,
    DOLLY_BLOCK_ESHORT,
    DOLLY_BLOCK_EIO,
    DOLLY_BLOCK_ENOMEM,
    DOLLY_BLOCK_EUNSUPPORTED
} DollyBlockStatus;

typedef struct DollyBlockReadResult {
    DollyBlockStatus status;
    size_t bytes_read;
} DollyBlockReadResult;

/* A backend is responsible for its own I/O, while the core guarantees
 * source bounds, validates returned byte counts and owns its context.
 * Both callbacks must be non-NULL; open_backend takes ownership ONLY
 * on success. The backend must NEVER write to its source. */
typedef DollyBlockReadResult (*DollyBlockReadFn)(void *context,
                                  uint64_t byte_offset, void *buffer, size_t bytes);
typedef void (*DollyBlockCloseFn)(void *context);
DollyBlockStatus dolly_block_open_backend(uint64_t byte_capacity,
                                          DollyBlockReadFn read_fn,
                                          DollyBlockCloseFn close_fn,
                                          void *context, DollyBlockSource **out);

/* A partial successful read returns ESHORT and bytes_read is the actual
 * number transferred; ERANGE is reserved for a starting offset beyond
 * the source. No allocations proportional to byte_capacity. */
DollyBlockReadResult dolly_block_read(DollyBlockSource *source,
                                     uint64_t byte_offset,
                                     void *buffer, size_t bytes);
uint64_t dolly_block_size(const DollyBlockSource *source);
void dolly_block_close(DollyBlockSource *source);
const char *dolly_block_status_name(DollyBlockStatus status);

/* POSIX host-file adapter; not linked into the AmigaOS build. */
DollyBlockStatus dolly_block_open_file(const char *path,
                                      DollyBlockSource **out);
#endif
