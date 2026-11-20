#ifndef DOLLY_CLONE_NAMES_H
#define DOLLY_CLONE_NAMES_H
#include <stddef.h>
#include <stdint.h>

typedef enum DollyCloneNameKind {
    DOLLY_CLONE_PART_NAME,
    DOLLY_CLONE_VOLUME_NAME
} DollyCloneNameKind;

/* Decode only checked PART or OFS/FFS root blocks. A failed rename leaves
 * the supplied buffer untouched. Sector I/O and collision checks are external. */
int dolly_clone_read_name(DollyCloneNameKind kind,const void *block,size_t bytes,char name[32]);
int dolly_clone_rename(DollyCloneNameKind kind,void *block,size_t bytes,
                       const char *old_name,const char *new_name);
#endif
