#ifndef DOLLY_RESTORE_SOURCE_H
#define DOLLY_RESTORE_SOURCE_H
#include "dolly_block_source.h"

/* Raw images describe exactly one selected destination extent. */
DollyBlockStatus dolly_restore_check_raw(uint64_t image_bytes,
                       uint64_t target_bytes,uint32_t sector_bytes);
#endif
