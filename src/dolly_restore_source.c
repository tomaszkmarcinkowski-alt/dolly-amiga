#include "dolly_restore_source.h"

DollyBlockStatus dolly_restore_check_raw(uint64_t image_bytes,
                       uint64_t target_bytes,uint32_t sector_bytes) {
    if(sector_bytes<512u || sector_bytes>32768u ||
       (sector_bytes&(sector_bytes-1u)) || !image_bytes || !target_bytes ||
       image_bytes%sector_bytes || target_bytes%sector_bytes)
        return DOLLY_BLOCK_EINVAL;
    return image_bytes==target_bytes?DOLLY_BLOCK_OK:DOLLY_BLOCK_ERANGE;
}
