#include "dolly_ffs_analyze.h"
#include <stdlib.h>
#include <string.h>

enum { ROOT_PAGES=25, BOOT_SECTOR_BYTES=512 };

static uint32_t word(const unsigned char *block,uint32_t index) {
    const unsigned char *p=block+index*4u;
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}

static void store_word(unsigned char *block,uint32_t index,uint32_t value) {
    unsigned char *p=block+index*4u;
    p[0]=(unsigned char)(value>>24);
    p[1]=(unsigned char)(value>>16);
    p[2]=(unsigned char)(value>>8);
    p[3]=(unsigned char)value;
}

static int checksum_ok(const unsigned char *block,uint32_t words) {
    uint32_t sum=0,i;
    for(i=0;i<words;++i)sum+=word(block,i);
    return sum==0;
}

static int read_block(DollyBlockSource *source,uint64_t start,uint32_t bytes,
                      uint32_t block,unsigned char *buffer) {
    return dolly_block_read(source,start+(uint64_t)bytes*block,buffer,bytes).status==DOLLY_BLOCK_OK;
}

static int compare_blocks(const void *a,const void *b) {
    uint32_t x=*(const uint32_t *)a,y=*(const uint32_t *)b;
    return (x>y)-(x<y);
}

static uint32_t bitmap_page_count(uint64_t blocks,uint32_t reserved,uint32_t coverage) {
    return (uint32_t)((blocks-reserved-1u)/coverage+1u);
}

static uint32_t bitmap_extension_count(uint32_t pages,uint32_t words) {
    return pages<=ROOT_PAGES?0u:(pages-ROOT_PAGES-1u)/(words-1u)+1u;
}

static uint32_t low_mask(uint32_t bits) {
    return bits>=32u?UINT32_MAX:(UINT32_C(1)<<bits)-1u;
}

static unsigned count_bits(uint32_t bits) {
    bits-=((bits>>1)&UINT32_C(0x55555555));
    bits=(bits&UINT32_C(0x33333333))+((bits>>2)&UINT32_C(0x33333333));
    bits=(bits+(bits>>4))&UINT32_C(0x0f0f0f0f);
    return (unsigned)((bits*UINT32_C(0x01010101))>>24);
}

typedef struct ResizeOutput {
    DollyFfsPatch patch;
    void *context;
    int allow_bitmap_relocation;
} ResizeOutput;

static int emit_block(const ResizeOutput *output,uint32_t block,uint32_t bytes,
                      const unsigned char *buffer) {
    return !output || output->patch(output->context,(uint64_t)block*bytes,buffer,bytes);
}

static void mark_root_valid(unsigned char *root,uint32_t words) {
    uint32_t sum=0;
    store_word(root,words-50u,UINT32_MAX);
    store_word(root,5,0);
    for(uint32_t i=0;i<words;++i)sum+=word(root,i);
    store_word(root,5,0u-sum);
}

static const char *preview_root_children(DollyBlockSource *source,uint64_t start,
                                         DollyFfsReport *report,const unsigned char *root,
                                         const uint32_t *pages,const uint32_t *metadata,
                                         uint32_t metadata_count,uint32_t new_root,
                                         uint32_t *children,const ResizeOutput *output) {
    uint32_t bytes=report->block_bytes,words=bytes/4u,coverage=(words-1u)*32u;
    uint32_t cached_page=UINT32_MAX,count=0,capacity=0;
    uint32_t *visited=NULL;
    unsigned char *entry=malloc(2u*bytes);
    const char *error="Not enough memory for FFS root entries.";
    if(!entry)return error;
    unsigned char *bitmap=entry+bytes;
    for(uint32_t bucket=6u;bucket<words-50u;++bucket) {
        uint32_t block=word(root,bucket),checkpoint=block;
        uint64_t distance=0,interval=1;
        while(block) {
            error="Invalid or cyclic FFS root directory reference.";
            if(block<report->reserved_blocks || block>=report->blocks ||
               count>=report->used-report->reserved_blocks ||
               bsearch(&block,metadata,metadata_count,sizeof(*metadata),compare_blocks))goto done;
            uint32_t relative=block-report->reserved_blocks;
            uint32_t page=relative/coverage,bit=relative%coverage;
            if(page!=cached_page) {
                error="Cannot validate allocation for an FFS root directory entry.";
                if(!read_block(source,start,bytes,pages[page],bitmap) ||
                   !checksum_ok(bitmap,words))goto done;
                cached_page=page;
            }
            error="FFS root directory points to a free block.";
            if(word(bitmap,1u+bit/32u)&(UINT32_C(1)<<(bit%32u)))goto done;
            error="Cannot read an FFS root directory entry.";
            if(!read_block(source,start,bytes,block,entry))goto done;
            error="Invalid FFS root entry header, parent or checksum.";
            if(word(entry,0)!=2u || word(entry,1)!=block ||
               word(entry,words-3u)!=report->root_block || !checksum_ok(entry,words))goto done;
            if(count==capacity) {
                uint64_t next_capacity=capacity ? (uint64_t)capacity*2u : 32u;
                error="Not enough memory for FFS root entries.";
                if(next_capacity>SIZE_MAX/sizeof(*visited) || next_capacity>UINT32_MAX)goto done;
                uint32_t *larger=realloc(visited,(size_t)next_capacity*sizeof(*visited));
                if(!larger)goto done;
                visited=larger;
                capacity=(uint32_t)next_capacity;
            }
            visited[count++]=block;
            block=word(entry,words-4u);
            error="Cyclic FFS root directory hash chain.";
            if(block && block==checkpoint)goto done;
            if(++distance==interval) {
                checkpoint=block;
                interval*=2u;
                distance=0;
            }
            /* Replacing the parent changes the additive checksum by the
             * difference between the old and new block numbers. */
            store_word(entry,words-3u,new_root);
            store_word(entry,5,word(entry,5)+(uint32_t)report->root_block-
                       new_root);
            error="Invalid checksum in the FFS parent update preview.";
            if(!checksum_ok(entry,words))goto done;
            error="Cannot write an updated FFS parent reference.";
            if(!emit_block(output,visited[count-1u],bytes,entry))goto done;
        }
    }
    if(count>1u) {
        qsort(visited,count,sizeof(*visited),compare_blocks);
        error="Duplicate FFS root directory reference.";
        for(uint32_t i=1u;i<count;++i)if(visited[i]==visited[i-1u])goto done;
    }
    *children=count;
    error=NULL;
done:
    free(visited);
    free(entry);
    return error;
}

static int grow_bitmap_page(const DollyFfsReport *report,uint32_t index,
                            const unsigned char *original,unsigned char *output,
                            int64_t *free_delta,int release_root);

/* Compare serialized references against the source order and growth plan.
 * Slots past the target bitmap must stay empty. */
static int preview_page_refs(const DollyFfsReport *report,const uint32_t *pages,
                             uint32_t old_pages,uint32_t first,
                             const unsigned char *block,uint32_t slot,uint32_t count) {
    for(uint32_t i=0;i<count;++i) {
        uint32_t page=first+i;
        uint64_t expected=0;
        if(page<old_pages)expected=pages[page];
        else if(page-old_pages<report->grow_bitmap_pages &&
                !dolly_ffs_grow_metadata_block(report,0,page-old_pages,&expected))return 0;
        if(word(block,slot+i)!=expected)return 0;
    }
    return 1;
}

typedef struct ShrinkBitmapMove {
    uint32_t index,destination;
} ShrinkBitmapMove;

static uint32_t shrink_bitmap_location(const uint32_t *pages,uint32_t index,
                                        const ShrinkBitmapMove *moves,uint32_t count) {
    uint32_t first=0;
    while(first<count) {
        uint32_t mid=first+(count-first)/2u;
        if(moves[mid].index<index)first=mid+1u;
        else if(moves[mid].index>index)count=mid;
        else return moves[mid].destination;
    }
    return pages[index];
}

static int shrink_reference_preview(DollyBlockSource *source,uint64_t start,
                                     const DollyFfsReport *report,const uint32_t *pages,
                                     uint32_t old_pages,uint32_t old_extensions,
                                     const ShrinkBitmapMove *moves,uint32_t count,
                                     unsigned char *buffer,const ResizeOutput *output) {
    uint32_t bytes=report->block_bytes,words=bytes/4u;
    for(uint32_t row=0;row<=report->shrink_bitmap_extensions;++row) {
        uint32_t first,slot,slots,next,old_next;
        if(!row) {
            if(!read_block(source,start,bytes,(uint32_t)report->root_block,buffer) ||
               !checksum_ok(buffer,words))return 0;
            first=0;
            slot=words-49u;
            slots=ROOT_PAGES;
            old_next=old_extensions?pages[old_pages]:0;
        } else {
            if(!read_block(source,start,bytes,pages[old_pages+row-1u],buffer))return 0;
            first=ROOT_PAGES+(row-1u)*(words-1u);
            slot=0;
            slots=words-1u;
            old_next=row<old_extensions?pages[old_pages+row]:0;
        }
        next=row<report->shrink_bitmap_extensions ?
             shrink_bitmap_location(pages,old_pages+row,moves,count) : 0;
        uint32_t link=row?words-1u:words-24u;
        if(word(buffer,link)!=old_next ||
           (next && (next<report->reserved_blocks || next>=report->target_blocks)))return 0;
        int changed=next!=old_next;
        store_word(buffer,link,next);
        for(uint32_t i=0;i<slots;++i) {
            uint32_t index=first+i;
            uint32_t original=index<old_pages?pages[index]:0;
            if(word(buffer,slot+i)!=original)return 0;
            uint32_t ref=0;
            if(index<report->shrink_bitmap_pages) {
                ref=shrink_bitmap_location(pages,index,moves,count);
                if(ref<report->reserved_blocks || ref>=report->target_blocks)return 0;
            }
            if(ref!=original)changed=1;
            store_word(buffer,slot+i,ref);
        }
        if(!row) {
            if(output)mark_root_valid(buffer,words);
            else {
                uint32_t sum=0;
                store_word(buffer,words-50u,0);
                store_word(buffer,5,0);
                for(uint32_t i=0;i<words;++i)sum+=word(buffer,i);
                store_word(buffer,5,0u-sum);
            }
            if(!checksum_ok(buffer,words) ||
               !emit_block(output,report->shrink_root_block,bytes,buffer))return 0;
        } else {
            uint32_t index=old_pages+row-1u;
            uint32_t location=shrink_bitmap_location(pages,index,moves,count);
            if((changed || location!=pages[index]) &&
               !emit_block(output,location,bytes,buffer))return 0;
        }
    }
    return 1;
}

static uint32_t word_range(uint64_t base,uint64_t first,uint64_t end);

static int shrink_bitmap_preview(DollyBlockSource *source,uint64_t start,
                                 DollyFfsReport *report,const uint32_t *pages,
                                 uint32_t root,uint32_t first_extension,
                                 uint32_t last_retained_extension,unsigned char *buffer,
                                 const uint32_t *obsolete,uint32_t obsolete_count,
                                 const ShrinkBitmapMove *moves,uint32_t move_count,
                                 const ResizeOutput *output) {
    uint32_t bytes=report->block_bytes,words=bytes/4u,coverage=(words-1u)*32u;
    uint32_t last_page=report->shrink_bitmap_pages-1u;
    uint32_t root_page=(report->shrink_root_block-report->reserved_blocks)/coverage;
    uint32_t next_obsolete=0,next_slot=0,next_page=0;
    uint64_t free_before=0,free_after=0;
    int root_pending=report->shrink_root_block!=root;
    int boundary_pending=1;
    uint32_t moved_pages=move_count?report->shrink_required_pages_outside:0u;
    while(boundary_pending || root_pending || next_obsolete<obsolete_count ||
          next_slot<move_count || next_page<moved_pages) {
        uint32_t page=last_page;
        if(root_pending && root_page<page)page=root_page;
        if(next_obsolete<obsolete_count) {
            if(obsolete[next_obsolete]>=report->target_blocks)return 0;
            uint32_t obsolete_page=(obsolete[next_obsolete]-report->reserved_blocks)/coverage;
            if(obsolete_page<page)page=obsolete_page;
        }
        if(next_slot<move_count) {
            uint32_t reserved_page=(moves[next_slot].destination-report->reserved_blocks)/coverage;
            if(reserved_page<page)page=reserved_page;
        }
        if(next_page<moved_pages && moves[next_page].index<page)page=moves[next_page].index;
        if(!read_block(source,start,bytes,pages[page],buffer) ||
           !checksum_ok(buffer,words))return 0;
        uint64_t base=report->reserved_blocks+(uint64_t)page*coverage;
        uint32_t sum=0;
        for(uint32_t slot=1;slot<words;++slot,base+=32u) {
            uint32_t bits=word(buffer,slot)&word_range(base,report->reserved_blocks,
                                                       report->target_blocks);
            free_before+=count_bits(bits);
            bits&=~word_range(base,report->shrink_root_block,report->shrink_root_block+1u);
            while(next_obsolete<obsolete_count && obsolete[next_obsolete]<base+32u) {
                if(obsolete[next_obsolete]<base)return 0;
                uint32_t mask=UINT32_C(1)<<(obsolete[next_obsolete]-base);
                if(bits&mask)return 0;
                bits|=mask;
                ++next_obsolete;
            }
            while(next_slot<move_count && moves[next_slot].destination<base+32u) {
                uint32_t location=moves[next_slot].destination;
                if(location<base || location>=report->target_blocks)return 0;
                uint32_t mask=UINT32_C(1)<<(location-base);
                if(!(bits&mask))return 0;
                bits&=~mask;
                ++next_slot;
            }
            free_after+=count_bits(bits);
            store_word(buffer,slot,bits);
            sum+=bits;
        }
        store_word(buffer,0,0u-sum);
        uint32_t location=shrink_bitmap_location(pages,page,moves,move_count);
        if(!checksum_ok(buffer,words) || !emit_block(output,location,bytes,buffer))return 0;
        if(next_page<moved_pages && moves[next_page].index==page)++next_page;
        if(page==root_page)root_pending=0;
        if(page==last_page)boundary_pending=0;
    }

    /* With relocations, shrink_reference_preview has already rebuilt
     * the root and the complete retained extension chain. */
    if(!move_count) {
        if(!read_block(source,start,bytes,root,buffer) ||
           !checksum_ok(buffer,words) || word(buffer,words-24u)!=first_extension ||
           (report->shrink_bitmap_extensions &&
            (first_extension<report->reserved_blocks || first_extension>=report->target_blocks ||
             last_retained_extension<report->reserved_blocks ||
             last_retained_extension>=report->target_blocks)))return 0;
        for(uint32_t i=0;i<ROOT_PAGES;++i) {
            uint32_t ref=i<=last_page ? pages[i] : 0;
            if(ref>=report->target_blocks || (ref && word(buffer,words-49u+i)!=ref))return 0;
            store_word(buffer,words-49u+i,ref);
        }
        store_word(buffer,words-24u,report->shrink_bitmap_extensions ? first_extension : 0u);
        store_word(buffer,words-50u,0);
        store_word(buffer,5,0);
        uint32_t sum=0;
        for(uint32_t i=0;i<words;++i)sum+=word(buffer,i);
        store_word(buffer,5,0u-sum);
        if(output) {
            mark_root_valid(buffer,words);
            if(!emit_block(output,report->shrink_root_block,bytes,buffer))return 0;
        }

        if(report->shrink_bitmap_extensions) {
            uint32_t first=ROOT_PAGES+(report->shrink_bitmap_extensions-1u)*(words-1u);
            if(!read_block(source,start,bytes,last_retained_extension,buffer))return 0;
            for(uint32_t i=0;i<words-1u;++i) {
                uint32_t ref=first+i<=last_page ? pages[first+i] : 0;
                if(ref>=report->target_blocks || (ref && word(buffer,i)!=ref))return 0;
                store_word(buffer,i,ref);
            }
            store_word(buffer,words-1u,0);
            if(!emit_block(output,last_retained_extension,bytes,buffer))return 0;
        }
    }
    /* Detached metadata reused for the new root remains allocated. */
    uint64_t extra_used=(uint64_t)move_count+
                        (report->shrink_root_block!=root && !report->shrink_root_reuses_bitmap);
    uint64_t removed=(uint64_t)obsolete_count+report->shrink_discarded_outside+
                     (unsigned)report->shrink_root_outside+move_count;
    if(free_before+obsolete_count!=free_after+extra_used ||
       report->used+extra_used<removed ||
       report->used+extra_used-removed>report->target_blocks)return 0;
    report->shrink_preview_free=report->target_blocks-(report->used+extra_used-removed);
    report->shrink_reclaimed_blocks=obsolete_count;
    report->shrink_relocation_bitmap_ready=move_count!=0;
    report->shrink_bitmap_preview_ready=move_count==0;
    return 1;
}

static void build_boot_preview(uint32_t root,const unsigned char *original,
                               unsigned char *output);

static int read_boot_preview(DollyBlockSource *source,uint64_t start,uint32_t dos_type,
                             uint32_t root,unsigned char *buffer,DollyFfsBootPayload *payload) {
    if(!read_block(source,start,BOOT_SECTOR_BYTES,0,buffer) ||
       !read_block(source,start,BOOT_SECTOR_BYTES,1,buffer+BOOT_SECTOR_BYTES) ||
       word(buffer,0)!=dos_type)return 0;
    *payload=DOLLY_FFS_BOOT_EMPTY;
    for(uint32_t i=12u;i<2u*BOOT_SECTOR_BYTES;++i) {
        if(buffer[i]) {
            *payload=DOLLY_FFS_BOOT_NONZERO;
            break;
        }
    }
    build_boot_preview(root,buffer,buffer);
    return 1;
}

static const char *analyze(DollyBlockSource *source,uint64_t start,uint64_t length,
                              uint32_t block_bytes,uint32_t reserved,uint32_t expected_dos_type,
                              uint64_t target_bytes,DollyFfsReport *out,DollyFfsProgress progress,
                              DollyFfsRange range,void *context,const ResizeOutput *output) {
    DollyFfsReport report={0};
    unsigned char *buffer;
    uint32_t *pages=NULL,*metadata=NULL;
    ShrinkBitmapMove *moves=NULL;
    uint32_t blocks,words,coverage,needed,extension,ext_needed,root,i,j,n=0,m=0,checked=0;
    uint32_t first_extension,last_extension=0,last_retained_extension=0;
    uint32_t previous_used=1u; /* The reserved prefix is included in the copy. */
    uint64_t size=dolly_block_size(source),run_start=0;
    int in_run=1;
    const char *error="Not enough memory for FFS analysis.";
    if(!out || !source || block_bytes<512u || block_bytes>32768u ||
       (block_bytes&(block_bytes-1u)) || start>size || length>size-start ||
       start%512u || length%block_bytes || target_bytes%block_bytes ||
       length/block_bytes>UINT32_MAX || target_bytes/block_bytes>UINT32_MAX || reserved<2u || length/block_bytes<=reserved)
        return "Invalid FFS block size, reserved area or partition extent.";
    blocks=(uint32_t)(length/block_bytes);
    words=block_bytes/4u;
    coverage=(words-1u)*32u;
    needed=bitmap_page_count(blocks,reserved,coverage);
    ext_needed=bitmap_extension_count(needed,words);
    uint64_t target_blocks=target_bytes/block_bytes;
    uint32_t retained_extensions=0;
    if(target_blocks>reserved && target_blocks<blocks)
        retained_extensions=bitmap_extension_count(bitmap_page_count(target_blocks,reserved,coverage),
                                                  words);
    if((uint64_t)(needed+ext_needed+1u)*sizeof(*metadata)>SIZE_MAX)
        return error;
    buffer=malloc(block_bytes==BOOT_SECTOR_BYTES && target_bytes && target_bytes!=length ?
                  2u*BOOT_SECTOR_BYTES : block_bytes);
    int shrinking=target_blocks>reserved && target_blocks<blocks;
    pages=malloc((size_t)(needed+(shrinking?ext_needed:0u))*sizeof(*pages));
    metadata=malloc((size_t)(needed+ext_needed+1u)*sizeof(*metadata));
    if(!buffer || !pages || !metadata)goto done;
    error="Cannot read the FFS boot block.";
    if(!read_block(source,start,block_bytes,0,buffer))goto done;
    report.dos_type=word(buffer,0);
    error=range?"Expected a supported OFS/FFS DosType matching the RDB.":
                "Expected a supported FastFileSystem DosType matching the RDB.";
    if((report.dos_type&UINT32_C(0xffffff00))!=UINT32_C(0x444f5300) ||
       (!range && !(report.dos_type&1u)) || (report.dos_type&255u)>7u ||
       (expected_dos_type && expected_dos_type!=report.dos_type))goto done;
    if(output && (report.dos_type!=UINT32_C(0x444f5301) && report.dos_type!=UINT32_C(0x444f5303))) {
        error="Resize output supports DOS/1 and DOS/3 only.";
        goto done;
    }
    root=word(buffer,2);
    if(root<reserved || root>=blocks)
        root=(uint32_t)(((uint64_t)blocks+reserved-1u)/2u);
    error="Cannot read the FFS root block.";
    if(!read_block(source,start,block_bytes,root,buffer))goto done;
    error="Invalid FFS root block type or checksum.";
    if(word(buffer,0)!=2u || word(buffer,words-1u)!=1u || !checksum_ok(buffer,words))goto done;
    error="FFS reports that its allocation bitmap is invalid.";
    if(word(buffer,words-50u)!=UINT32_MAX)goto done;

    error="Invalid, duplicate or incomplete FFS bitmap references.";
    metadata[m++]=root;
    for(i=0;i<ROOT_PAGES;++i) {
        uint32_t ref=word(buffer,words-49u+i);
        if(n==needed ? ref!=0 : ref<reserved || ref>=blocks)goto done;
        if(ref)metadata[m++]=pages[n++]=ref;
    }
    first_extension=extension=word(buffer,words-24u);
    for(i=0;i<ext_needed;++i) {
        if(extension<reserved || extension>=blocks)goto done;
        last_extension=extension;
        if(shrinking)pages[needed+i]=extension;
        if(i+1u==retained_extensions)last_retained_extension=extension;
        metadata[m++]=extension;
        if(!read_block(source,start,block_bytes,extension,buffer)) {
            error="Cannot read an FFS bitmap extension block.";
            goto done;
        }
        for(j=0;j<words-1u;++j) {
            uint32_t ref=word(buffer,j);
            if(n==needed ? ref!=0 : ref<reserved || ref>=blocks)goto done;
            if(ref)metadata[m++]=pages[n++]=ref;
        }
        extension=word(buffer,words-1u);
    }
    if(extension || n!=needed)goto done;
    qsort(metadata,m,sizeof(*metadata),compare_blocks);
    for(i=1;i<m;++i)if(metadata[i]==metadata[i-1u])goto done;

    report.blocks=blocks;
    report.block_bytes=block_bytes;
    report.reserved_blocks=reserved;
    report.root_block=root;
    report.used=reserved;
    report.copy_runs=1u;
    report.highest_used=reserved-1u;
    report.target_blocks=target_bytes/block_bytes;
    report.first_outside_target=UINT64_MAX;
    uint32_t target_root=0;
    DollyFfsRootPlacement root_placement=DOLLY_FFS_ROOT_UNCHANGED;
    if(report.target_blocks>reserved && report.target_blocks!=blocks) {
        uint32_t target_pages=bitmap_page_count(report.target_blocks,reserved,coverage);
        uint32_t target_extensions=bitmap_extension_count(target_pages,words);
        target_root=(uint32_t)((report.target_blocks+reserved-1u)/2u);
        if(target_root==root)root_placement=DOLLY_FFS_ROOT_UNCHANGED;
        else if(target_root>=blocks)root_placement=DOLLY_FFS_ROOT_NEW_SPACE;
        else if(bsearch(&target_root,metadata,m,sizeof(*metadata),compare_blocks))
            root_placement=DOLLY_FFS_ROOT_METADATA;
        else root_placement=DOLLY_FFS_ROOT_FREE;
        if(report.target_blocks>blocks) {
            report.grow_added_blocks=report.target_blocks-blocks;
            report.grow_root_block=target_root;
            report.grow_tail_available=report.grow_added_blocks-(target_root>=blocks?1u:0u);
            report.grow_bitmap_pages=target_pages-needed;
            report.grow_bitmap_extensions=target_extensions-ext_needed;
        } else {
            report.shrink_root_block=target_root;
            report.shrink_bitmap_pages=target_pages;
            report.shrink_bitmap_extensions=target_extensions;
        }
    }
    if(target_bytes && report.target_blocks<reserved) {
        report.first_outside_target=report.target_blocks;
        report.used_outside_target=reserved-report.target_blocks;
    }
    if(report.shrink_bitmap_pages) {
        report.shrink_root_outside=block_bytes==BOOT_SECTOR_BYTES && root>=report.target_blocks;
        for(i=0;i<needed+ext_needed;++i) {
            uint32_t block=pages[i];
            int is_extension=i>=needed;
            int retained=is_extension ? i-needed<retained_extensions : i<report.shrink_bitmap_pages;
            if(!retained) {
                if(block>=report.target_blocks)++report.shrink_discarded_outside;
                if(block==report.shrink_root_block)report.shrink_root_reuses_bitmap=1;
            } else if(block>=report.target_blocks) {
                if(is_extension)++report.shrink_required_extensions_outside;
                else ++report.shrink_required_pages_outside;
                if(!report.shrink_first_required_bitmap_outside ||
                   block<report.shrink_first_required_bitmap_outside)
                    report.shrink_first_required_bitmap_outside=block;
            }
        }
    }
    uint32_t relocation_needed=report.shrink_required_pages_outside+
                               report.shrink_required_extensions_outside;
    if(relocation_needed && (!output || output->allow_bitmap_relocation)) {
        error="Not enough memory for the FFS bitmap relocation plan.";
        if((uint64_t)relocation_needed*sizeof(*moves)>SIZE_MAX)goto done;
        moves=malloc((size_t)relocation_needed*sizeof(*moves));
        if(!moves)goto done;
    }
    if(progress)progress(0);
    for(i=0;i<needed;++i) {
        uint64_t base=(uint64_t)reserved+(uint64_t)i*coverage;
        if(!read_block(source,start,block_bytes,pages[i],buffer)) {
            error="Cannot read an FFS allocation bitmap block.";
            goto done;
        }
        error="Invalid FFS allocation bitmap checksum.";
        if(!checksum_ok(buffer,words))goto done;
        for(j=1;j<words && base<blocks;++j,base+=32u) {
            uint32_t used=~word(buffer,j)&low_mask((uint32_t)(blocks-base));
            uint64_t end=base+32u;
            while(checked<m && metadata[checked]<end) {
                if(!(used&(UINT32_C(1)<<(metadata[checked]-base)))) {
                    error="FFS bitmap marks a metadata block as free.";
                    goto done;
                }
                ++checked;
            }
            if(root_placement==DOLLY_FFS_ROOT_FREE &&
               target_root>=base && target_root<end &&
               (used&(UINT32_C(1)<<(target_root-base))))
                root_placement=DOLLY_FFS_ROOT_OCCUPIED;
            if(report.shrink_relocation_slots<relocation_needed && base<report.target_blocks) {
                uint32_t free_bits=~used&low_mask((uint32_t)(report.target_blocks-base));
                /* Keep the proposed root available for the resize. Select
                 * candidate blocks in ascending partition-relative order. */
                if(target_root>=base && target_root<end)
                    free_bits&=~(UINT32_C(1)<<(target_root-base));
                for(unsigned bit=0;free_bits && report.shrink_relocation_slots<relocation_needed;
                    ++bit,free_bits>>=1) {
                    if(!(free_bits&1u))continue;
                    uint32_t block=(uint32_t)(base+bit);
                    if(!report.shrink_relocation_slots)report.shrink_first_relocation_slot=block;
                    report.shrink_last_relocation_slot=block;
                    if(moves)moves[report.shrink_relocation_slots].destination=block;
                    ++report.shrink_relocation_slots;
                }
            }
            if(used) {
                unsigned highest=31;
                while(!(used&(UINT32_C(1)<<highest)))--highest;
                report.used+=count_bits(used);
                report.copy_runs+=count_bits(used&~((used<<1)|previous_used));
                report.highest_used=base+highest;
            }
            if(range && (used!=UINT32_MAX || !in_run)) {
                unsigned bit;
                for(bit=0;bit<32u && base+bit<blocks;++bit) {
                    int occupied=(used&(UINT32_C(1)<<bit))!=0;
                    if(occupied && !in_run)run_start=base+bit;
                    if(!occupied && in_run &&
                       !range(context,run_start*block_bytes,(base+bit-run_start)*block_bytes)) {
                        error="Cannot stage FFS copy ranges.";
                        goto done;
                    }
                    in_run=occupied;
                    if((used>>bit)==0)break;
                }
            }
            previous_used=used>>31;
            if(target_bytes && report.target_blocks<end) {
                uint32_t inside=report.target_blocks>base?(uint32_t)(report.target_blocks-base):0;
                uint32_t outside=used&~low_mask(inside);
                if(outside && report.first_outside_target==UINT64_MAX) {
                    unsigned bit=inside;
                    while(!(outside&(UINT32_C(1)<<bit)))++bit;
                    report.first_outside_target=base+bit;
                }
                report.used_outside_target+=count_bits(outside);
            }
        }
        if(progress) {
            unsigned percent=(i+1u)*100u/needed;
            if(percent!=i*100u/needed)progress(percent);
        }
    }
    if(output) {
        DollyFfsBootPayload payload=DOLLY_FFS_BOOT_UNCHECKED;
        error="Resize output requires two readable boot sectors with an empty startup payload.";
        if(!read_boot_preview(source,start,report.dos_type,target_root,buffer,&payload) ||
           payload!=DOLLY_FFS_BOOT_EMPTY)goto done;
    }
    if(report.grow_added_blocks)report.grow_root_placement=root_placement;
    if(report.shrink_bitmap_pages)report.shrink_root_placement=root_placement;
    if(report.grow_added_blocks && report.grow_root_placement<=DOLLY_FFS_ROOT_NEW_SPACE &&
       (uint64_t)report.grow_bitmap_pages+report.grow_bitmap_extensions<=report.grow_tail_available) {
        int64_t free_delta=0;
        uint32_t root_page=(uint32_t)((report.grow_root_block-reserved)/coverage);
        error="Could not build the planned FFS allocation bitmap.";
        int release_root=output && report.grow_root_block!=root;
        uint32_t old_root_page=(root-reserved)/coverage;
        uint32_t existing_pages[]={needed-1u,root_page,old_root_page};
        unsigned candidates=release_root ? 3u : 2u;
        for(i=0;i<candidates;++i) {
            uint32_t index=existing_pages[i];
            if(index>=needed || (i && index==existing_pages[0]) ||
               (i==2u && index==root_page))continue;
            if(!read_block(source,start,block_bytes,pages[index],buffer) ||
               !grow_bitmap_page(&report,index,buffer,buffer,&free_delta,release_root) ||
               !emit_block(output,pages[index],block_bytes,buffer))goto done;
            ++report.grow_preview_bitmap_pages;
        }
        for(i=0;i<report.grow_bitmap_pages;++i) {
            uint64_t location;
            if(!dolly_ffs_grow_metadata_block(&report,0,i,&location) ||
               !grow_bitmap_page(&report,needed+i,NULL,buffer,&free_delta,release_root) ||
               !emit_block(output,(uint32_t)location,block_bytes,buffer))goto done;
            ++report.grow_preview_bitmap_pages;
        }
        uint64_t added_used=(uint64_t)report.grow_bitmap_pages+report.grow_bitmap_extensions+
                            (report.grow_root_block!=report.root_block && !release_root);
        error="FFS growth bitmap does not match the planned free block count.";
        if(free_delta!=(int64_t)report.grow_added_blocks-(int64_t)added_used)goto done;
        report.grow_preview_used=report.used+added_used;
        report.grow_preview_free=report.target_blocks-report.grow_preview_used;
        error="Could not build the planned FFS root block.";
        if(!read_block(source,start,block_bytes,root,buffer) ||
           !dolly_ffs_grow_root_preview(&report,buffer,buffer))goto done;
        error="The planned FFS root bitmap references do not match the allocation plan.";
        uint64_t root_extension=first_extension;
        if(!ext_needed && report.grow_bitmap_extensions &&
           !dolly_ffs_grow_metadata_block(&report,1,0,&root_extension))goto done;
        if(word(buffer,words-24u)!=root_extension ||
           !preview_page_refs(&report,pages,needed,0,buffer,words-49u,ROOT_PAGES))goto done;
        if(output) {
            error="Cannot write the new FFS root.";
            mark_root_valid(buffer,words);
            if(!emit_block(output,(uint32_t)report.grow_root_block,block_bytes,buffer))goto done;
        }
        if(report.grow_root_block!=report.root_block) {
            error=preview_root_children(source,start,&report,buffer,pages,metadata,m,
                                        (uint32_t)report.grow_root_block,&report.grow_root_children,output);
            if(error)goto done;
        }
        if(block_bytes==BOOT_SECTOR_BYTES &&
           !read_boot_preview(source,start,report.dos_type,(uint32_t)report.grow_root_block,
                              buffer,&report.grow_boot_payload)) {
            error="Could not build the planned FFS boot block.";
            goto done;
        }
        if(output && !emit_block(output,0,2u*BOOT_SECTOR_BYTES,buffer)) {
            error="Cannot write the new FFS boot sectors.";
            goto done;
        }
        error="Could not build or verify the planned FFS bitmap extensions.";
        /* Earlier existing extensions are already validated and retain their
         * links. Only the existing tail and the new extension blocks change. */
        uint32_t extension_end=ext_needed+report.grow_bitmap_extensions;
        uint32_t extension_start=ext_needed && report.grow_bitmap_pages ?
                                 ext_needed-1u : ext_needed;
        for(i=extension_start;i<extension_end;++i) {
            const unsigned char *original=NULL;
            uint64_t next=0;
            if(i<ext_needed) {
                if(!read_block(source,start,block_bytes,last_extension,buffer))goto done;
                original=buffer;
            }
            if(i+1u<extension_end &&
               !dolly_ffs_grow_metadata_block(&report,1,i+1u-ext_needed,&next))goto done;
            if(!dolly_ffs_grow_extension_preview(&report,i,original,buffer) ||
               word(buffer,words-1u)!=next ||
               !preview_page_refs(&report,pages,needed,ROOT_PAGES+i*(words-1u),
                                  buffer,0,words-1u))goto done;
            uint64_t location=last_extension;
            if(i>=ext_needed &&
               !dolly_ffs_grow_metadata_block(&report,1,i-ext_needed,&location))goto done;
            if(!emit_block(output,(uint32_t)location,block_bytes,buffer))goto done;
        }
    }
    if(moves && report.shrink_relocation_slots==relocation_needed &&
       report.used_outside_target==(uint64_t)report.shrink_discarded_outside+
                                   (unsigned)report.shrink_root_outside+relocation_needed &&
       (report.shrink_root_placement<=DOLLY_FFS_ROOT_FREE ||
        report.shrink_root_reuses_bitmap)) {
        uint32_t assigned=0;
        for(i=0;i<report.shrink_bitmap_pages;++i)
            if(pages[i]>=report.target_blocks)moves[assigned++].index=i;
        for(i=0;i<retained_extensions;++i)
            if(pages[needed+i]>=report.target_blocks)moves[assigned++].index=needed+i;
        error="Could not build the relocated FFS bitmap reference chain.";
        if(assigned!=relocation_needed ||
           !shrink_reference_preview(source,start,&report,pages,needed,ext_needed,moves,
                                     relocation_needed,buffer,output))goto done;
        /* Identify the exact source-to-target assignments for preflight
         * revalidation, independently of the displayed first/last slots. */
        uint32_t fingerprint=UINT32_C(2166136261);
        for(i=0;i<assigned;++i) {
            fingerprint=(fingerprint^moves[i].index)*UINT32_C(16777619);
            fingerprint=(fingerprint^pages[moves[i].index])*UINT32_C(16777619);
            fingerprint=(fingerprint^moves[i].destination)*UINT32_C(16777619);
        }
        report.shrink_relocation_fingerprint=fingerprint;
        report.shrink_relocation_refs_ready=1;
    }
    uint32_t preview_moves=report.shrink_relocation_refs_ready?relocation_needed:0u;
    if(report.shrink_bitmap_pages &&
       report.used_outside_target==(uint64_t)report.shrink_discarded_outside+
                                   (unsigned)report.shrink_root_outside+preview_moves &&
       (report.shrink_root_placement<=DOLLY_FFS_ROOT_FREE ||
        report.shrink_root_reuses_bitmap)) {
        if(report.shrink_root_block!=root) {
            error="Cannot reread the FFS root block for shrinking.";
            if(!read_block(source,start,block_bytes,root,buffer) ||
               !checksum_ok(buffer,words))goto done;
            error=preview_root_children(source,start,&report,buffer,pages,metadata,m,
                                        report.shrink_root_block,&report.shrink_root_children,output);
            if(error)goto done;
        }
        /* The root-entry check has finished using the metadata index.
         * Reuse it for the detached blocks, ordered by physical address. */
        uint32_t obsolete_count=0;
        for(i=report.shrink_bitmap_pages;i<needed;++i)
            if(pages[i]<report.target_blocks && pages[i]!=report.shrink_root_block)
                metadata[obsolete_count++]=pages[i];
        for(i=retained_extensions;i<ext_needed;++i)
            if(pages[needed+i]<report.target_blocks && pages[needed+i]!=report.shrink_root_block)
                metadata[obsolete_count++]=pages[needed+i];
        if(output && root!=report.shrink_root_block && root<report.target_blocks)
            metadata[obsolete_count++]=root;
        qsort(metadata,obsolete_count,sizeof(*metadata),compare_blocks);
        error="Could not build the shortened FFS allocation bitmap.";
        if(!shrink_bitmap_preview(source,start,&report,pages,root,first_extension,
                                  last_retained_extension,buffer,metadata,obsolete_count,
                                  moves,preview_moves,output))goto done;
        if(block_bytes==BOOT_SECTOR_BYTES &&
           !read_boot_preview(source,start,report.dos_type,report.shrink_root_block,
                              buffer,&report.shrink_boot_payload)) {
            error="Could not build the shortened FFS boot block.";
            goto done;
        }
    }
    if(output) {
        error="The selected FFS resize needs block relocation beyond this prototype.";
        int shrink_ready=report.shrink_bitmap_preview_ready || report.shrink_relocation_bitmap_ready;
        if(!report.grow_preview_bitmap_pages && !shrink_ready)goto done;
        if(shrink_ready &&
           !emit_block(output,0,2u*BOOT_SECTOR_BYTES,buffer)) {
            error="Cannot write the new FFS boot sectors.";
            goto done;
        }
    }
    if(range && in_run &&
       !range(context,run_start*block_bytes,(blocks-run_start)*block_bytes)) {
        error="Cannot stage FFS copy ranges.";
        goto done;
    }
    report.minimum_blocks=report.highest_used+1u;
    *out=report;
    error=NULL;
done:
    free(moves);
    free(metadata);
    free(pages);
    free(buffer);
    return error;
}


const char *dolly_ffs_analyze_with_progress(DollyBlockSource *source,uint64_t start,uint64_t length,
                              uint32_t block_bytes,uint32_t reserved,
                              uint32_t expected_dos_type,uint64_t target_bytes,
                              DollyFfsReport *out,DollyFfsProgress progress) {
    return analyze(source,start,length,block_bytes,reserved,expected_dos_type,target_bytes,
                   out,progress,NULL,NULL,NULL);
}

const char *dolly_ffs_copy_plan(DollyBlockSource *source,uint64_t start,uint64_t length,
                              uint32_t block_bytes,uint32_t reserved,uint32_t dos_type,
                              DollyFfsReport *out,DollyFfsProgress progress,
                              DollyFfsRange range,void *context) {
    if(!range)return "Missing FFS copy range visitor.";
    return analyze(source,start,length,block_bytes,reserved,dos_type,length,out,
                   progress,range,context,NULL);
}

const char *dolly_ffs_analyze(DollyBlockSource *source,uint64_t start,uint64_t length,
                              uint32_t block_bytes,uint32_t reserved,
                              uint32_t expected_dos_type,uint64_t target_bytes,
                              DollyFfsReport *out) {
    return dolly_ffs_analyze_with_progress(source,start,length,block_bytes,reserved,
                                          expected_dos_type,target_bytes,out,NULL);
}

static const char *resize_copy(DollyBlockSource *source,uint64_t start,uint64_t length,
                               uint32_t reserved,uint64_t target_bytes,DollyFfsReport *out,
                               const ResizeOutput *output) {
    if(!output->patch || target_bytes==length || target_bytes/BOOT_SECTOR_BYTES<=reserved)
        return "Resize output requires a different target size with room for the filesystem.";
    return analyze(source,start,length,BOOT_SECTOR_BYTES,reserved,0,target_bytes,
                   out,NULL,NULL,NULL,output);
}

const char *dolly_ffs_resize_copy(DollyBlockSource *source,uint64_t start,uint64_t length,
                                uint32_t reserved,uint64_t target_bytes,DollyFfsReport *out,
                                DollyFfsPatch patch,void *context) {
    const ResizeOutput output={patch,context,0};
    return resize_copy(source,start,length,reserved,target_bytes,out,&output);
}

typedef struct FfsResizeSector {
    uint32_t index;
    unsigned char data[BOOT_SECTOR_BYTES];
} FfsResizeSector;

typedef struct FfsResizeSource {
    DollyBlockSource *source;
    FfsResizeSector *sectors;
    uint64_t start,length;
    size_t count,capacity;
} FfsResizeSource;

static int stage_resize_sector(void *context,uint64_t offset,const void *data,size_t bytes) {
    FfsResizeSource *resize=context;
    const unsigned char *buffer=data;
    while(bytes) {
        if(resize->count==resize->capacity) {
            size_t capacity=resize->capacity ? resize->capacity*2u : 16u;
            if(capacity<resize->capacity || capacity>SIZE_MAX/sizeof(*resize->sectors))return 0;
            FfsResizeSector *sectors=realloc(resize->sectors,capacity*sizeof(*sectors));
            if(!sectors)return 0;
            resize->capacity=capacity;
            resize->sectors=sectors;
        }
        FfsResizeSector *sector=&resize->sectors[resize->count++];
        sector->index=(uint32_t)(offset/BOOT_SECTOR_BYTES);
        memcpy(sector->data,buffer,BOOT_SECTOR_BYTES);
        offset+=BOOT_SECTOR_BYTES;
        buffer+=BOOT_SECTOR_BYTES;
        bytes-=BOOT_SECTOR_BYTES;
    }
    return 1;
}

static int compare_resize_sector(const void *a,const void *b) {
    return compare_blocks(&((const FfsResizeSector *)a)->index,
                          &((const FfsResizeSector *)b)->index);
}

static DollyBlockReadResult read_resize_source(void *context,uint64_t offset,
                                               void *data,size_t bytes) {
    FfsResizeSource *resize=context;
    unsigned char *buffer=data;
    uint64_t source_size=resize->length;
    size_t original=0;
    if(offset<source_size) {
        original=source_size-offset<bytes ? (size_t)(source_size-offset) : bytes;
        DollyBlockReadResult read=dolly_block_read(resize->source,resize->start+offset,buffer,original);
        if(read.status!=DOLLY_BLOCK_OK)return read;
    }
    if(original<bytes)memset(buffer+original,0,bytes-original);

    size_t low=0,high=resize->count;
    while(low<high) {
        size_t mid=low+(high-low)/2u;
        if(resize->sectors[mid].index<offset/BOOT_SECTOR_BYTES)low=mid+1u;
        else high=mid;
    }
    for(;low<resize->count;++low) {
        const FfsResizeSector *sector=&resize->sectors[low];
        uint64_t start=(uint64_t)sector->index*BOOT_SECTOR_BYTES;
        if(start>=offset+bytes)break;
        uint64_t first=start<offset ? offset : start;
        uint64_t end=start+BOOT_SECTOR_BYTES;
        if(end>offset+bytes)end=offset+bytes;
        memcpy(buffer+(size_t)(first-offset),sector->data+(size_t)(first-start),
               (size_t)(end-first));
    }
    return (DollyBlockReadResult){DOLLY_BLOCK_OK,bytes};
}

static void close_resize_source(void *context) {
    FfsResizeSource *resize=context;
    free(resize->sectors);
    free(resize);
}

const char *dolly_ffs_open_resized_source(DollyBlockSource *source,uint64_t start,uint64_t length,
                                        uint32_t reserved,uint64_t target,
                                        int allow_bitmap_relocation,DollyFfsReport *report,
                                        DollyBlockSource **resized) {
    if(!resized)return "Missing resized block source.";
    *resized=NULL;
    FfsResizeSource *resize=calloc(1,sizeof(*resize));
    if(!resize)return "Cannot allocate FFS resize metadata.";
    resize->source=source;
    resize->start=start;
    resize->length=length;
    const ResizeOutput output={stage_resize_sector,resize,allow_bitmap_relocation};
    const char *error=resize_copy(source,start,length,reserved,target,report,&output);
    if(!error) {
        qsort(resize->sectors,resize->count,sizeof(*resize->sectors),compare_resize_sector);
        for(size_t i=1u;i<resize->count;++i)
            if(resize->sectors[i-1u].index==resize->sectors[i].index) {
                error="The FFS resize generated conflicting metadata locations.";
                break;
            }
    }
    if(!error && dolly_block_open_backend(target,read_resize_source,close_resize_source,
                                          resize,resized)!=DOLLY_BLOCK_OK)
        error="Cannot open the resized partition view.";
    if(error)close_resize_source(resize);
    return error;
}

int dolly_ffs_grow_metadata_block(const DollyFfsReport *report,int extension,
                                  uint32_t index,uint64_t *block) {
    uint64_t count,position;
    if(!report || !block || !report->grow_added_blocks)return 0;
    count=(uint64_t)report->grow_bitmap_pages+report->grow_bitmap_extensions;
    if(count>report->grow_tail_available ||
       index>=(extension?report->grow_bitmap_extensions:report->grow_bitmap_pages))
        return 0;
    position=report->blocks+index+(extension?report->grow_bitmap_pages:0u);
    if(report->grow_root_block>=report->blocks && position>=report->grow_root_block)
        ++position;
    if(position>=report->target_blocks)return 0;
    *block=position;
    return 1;
}

/* Return the bits in the 32-block word that intersect [first,end). */
static uint32_t word_range(uint64_t base,uint64_t first,uint64_t end) {
    uint64_t word_end=base+32u;
    if(first>=word_end || end<=base || first>=end)return 0;
    if(first<base)first=base;
    if(end>word_end)end=word_end;
    return low_mask((uint32_t)(end-base))&~low_mask((uint32_t)(first-base));
}

static int grow_bitmap_page(const DollyFfsReport *report,uint32_t index,
                            const unsigned char *original,unsigned char *output,
                            int64_t *free_delta,int release_root) {
    uint32_t words,coverage,old_pages,target_pages,sum=0,j;
    uint64_t begin,end,base,metadata_end,count;
    int root_in_tail;
    if(!report || !output || report->block_bytes<512u || report->block_bytes>32768u ||
       (report->block_bytes&(report->block_bytes-1u)) ||
       report->reserved_blocks<2u || report->blocks<=report->reserved_blocks ||
       report->target_blocks<=report->blocks || report->target_blocks>UINT32_MAX ||
       report->grow_added_blocks!=report->target_blocks-report->blocks ||
       report->grow_root_block<report->reserved_blocks ||
       report->grow_root_block>=report->target_blocks ||
       report->grow_root_placement>DOLLY_FFS_ROOT_NEW_SPACE)
        return 0;
    words=report->block_bytes/4u;
    coverage=(words-1u)*32u;
    old_pages=bitmap_page_count(report->blocks,report->reserved_blocks,coverage);
    target_pages=bitmap_page_count(report->target_blocks,report->reserved_blocks,coverage);
    root_in_tail=report->grow_root_block>=report->blocks;
    count=(uint64_t)report->grow_bitmap_pages+report->grow_bitmap_extensions;
    if(report->grow_bitmap_pages!=target_pages-old_pages ||
       report->grow_bitmap_extensions!=
           bitmap_extension_count(target_pages,words)-bitmap_extension_count(old_pages,words) ||
       report->grow_root_block!=(report->target_blocks+report->reserved_blocks-1u)/2u ||
       report->grow_tail_available!=report->grow_added_blocks-(root_in_tail?1u:0u) ||
       count>report->grow_tail_available || index>=target_pages ||
       (index<old_pages)!=(original!=NULL) ||
       (original && !checksum_ok(original,words)))return 0;
    metadata_end=report->blocks;
    if(count) {
        uint64_t last;
        uint32_t extensions=report->grow_bitmap_extensions;
        if(!dolly_ffs_grow_metadata_block(report,extensions!=0,
                     (extensions?extensions:report->grow_bitmap_pages)-1u,&last))return 0;
        metadata_end=last+1u;
    }
    begin=(uint64_t)report->reserved_blocks+(uint64_t)index*coverage;
    end=begin+coverage;
    if(report->grow_root_placement==DOLLY_FFS_ROOT_FREE &&
       report->grow_root_block>=begin && report->grow_root_block<end) {
        uint32_t relative=(uint32_t)(report->grow_root_block-begin);
        if(!original || !(word(original,1u+relative/32u)&(UINT32_C(1)<<(relative%32u))))
            return 0;
    }
    for(j=1,base=begin;j<words;++j,base+=32u) {
        uint32_t bits=original?word(original,j):0;
        /* Existing positions keep their allocation state. Only the added
         * extent is initially free, and out-of-volume bits stay cleared. */
        bits&=word_range(base,report->reserved_blocks,report->blocks);
        uint32_t old_free=bits;
        bits|=word_range(base,report->blocks,report->target_blocks);
        bits&=~word_range(base,report->blocks,metadata_end);
        bits&=~word_range(base,report->grow_root_block,report->grow_root_block+1u);
        if(release_root)bits|=word_range(base,report->root_block,report->root_block+1u);
        if(free_delta)*free_delta+=(int32_t)count_bits(bits)-(int32_t)count_bits(old_free);
        store_word(output,j,bits);
        sum+=bits;
    }
    store_word(output,0,0u-sum);
    return 1;
}

int dolly_ffs_grow_bitmap_page(const DollyFfsReport *report,uint32_t index,
                              const unsigned char *original,unsigned char *output) {
    return grow_bitmap_page(report,index,original,output,NULL,0);
}

int dolly_ffs_grow_root_preview(const DollyFfsReport *report,
                               const unsigned char *original,unsigned char *output) {
    uint32_t words,coverage,old_pages,new_pages,index,sum=0;
    uint64_t placement=0;
    if(!report || !original || !output || report->block_bytes<512u ||
       report->block_bytes>32768u || (report->block_bytes&(report->block_bytes-1u)) ||
       report->reserved_blocks<2u || report->blocks<=report->reserved_blocks ||
       report->target_blocks<=report->blocks || report->target_blocks>UINT32_MAX ||
       report->grow_root_placement>DOLLY_FFS_ROOT_NEW_SPACE)return 0;
    words=report->block_bytes/4u;
    coverage=(words-1u)*32u;
    old_pages=bitmap_page_count(report->blocks,report->reserved_blocks,coverage);
    new_pages=bitmap_page_count(report->target_blocks,report->reserved_blocks,coverage);
    if(word(original,0)!=2u || word(original,words-1u)!=1u ||
       word(original,words-50u)!=UINT32_MAX || !checksum_ok(original,words) ||
       report->grow_bitmap_pages!=new_pages-old_pages)return 0;
    /* Validate allocation before modifying the caller's buffer. */
    for(index=old_pages;index<new_pages && index<ROOT_PAGES;++index)
        if(word(original,words-49u+index) ||
           !dolly_ffs_grow_metadata_block(report,0,index-old_pages,&placement))return 0;
    if(old_pages<=ROOT_PAGES && new_pages>ROOT_PAGES &&
       (word(original,words-24u) ||
        !dolly_ffs_grow_metadata_block(report,1,0,&placement)))return 0;

    if(output!=original)memcpy(output,original,report->block_bytes);
    for(index=old_pages;index<new_pages && index<ROOT_PAGES;++index) {
        (void)dolly_ffs_grow_metadata_block(report,0,index-old_pages,&placement);
        store_word(output,words-49u+index,(uint32_t)placement);
    }
    if(old_pages<=ROOT_PAGES && new_pages>ROOT_PAGES) {
        (void)dolly_ffs_grow_metadata_block(report,1,0,&placement);
        store_word(output,words-24u,(uint32_t)placement);
    }
    store_word(output,words-50u,0); /* The prospective bitmap needs validation. */
    store_word(output,5,0);
    for(index=0;index<words;++index)sum+=word(output,index);
    store_word(output,5,0u-sum);
    return 1;
}

static uint32_t boot_sum(const unsigned char *block) {
    uint32_t sum=0;
    for(uint32_t i=0;i<2u*BOOT_SECTOR_BYTES/4u;++i) {
        uint32_t previous=sum;
        sum+=word(block,i);
        if(sum<previous)++sum;
    }
    return sum;
}

int dolly_ffs_grow_boot_preview(const DollyFfsReport *report,
                               const unsigned char *original,unsigned char *output) {
    if(!report || !original || !output || report->block_bytes!=BOOT_SECTOR_BYTES ||
       report->reserved_blocks<2u || report->target_blocks<=report->blocks ||
       report->target_blocks>UINT32_MAX ||
       report->grow_root_block<report->reserved_blocks ||
       report->grow_root_block>=report->target_blocks ||
       report->grow_root_placement>DOLLY_FFS_ROOT_NEW_SPACE ||
       word(original,0)!=report->dos_type)return 0;

    build_boot_preview((uint32_t)report->grow_root_block,original,output);
    return 1;
}

static void build_boot_preview(uint32_t root,const unsigned char *original,
                               unsigned char *output) {
    int checksum_valid=boot_sum(original)==UINT32_MAX;
    if(output!=original)memcpy(output,original,2u*BOOT_SECTOR_BYTES);
    store_word(output,2,root);
    store_word(output,1,0);
    uint32_t checksum=~boot_sum(output);
    /* A previously invalid boot checksum must remain invalid. */
    store_word(output,1,checksum_valid ? checksum : (checksum==0u ? 1u : 0u));
}

int dolly_ffs_grow_extension_preview(const DollyFfsReport *report,uint32_t index,
                                    const unsigned char *original,unsigned char *output) {
    uint32_t words,coverage,old_pages,new_pages,old_count,new_count,first,i,next=0;
    uint64_t placement=0;
    if(!report || !output || report->block_bytes<512u || report->block_bytes>32768u ||
       (report->block_bytes&(report->block_bytes-1u)) ||
       report->reserved_blocks<2u || report->blocks<=report->reserved_blocks ||
       report->target_blocks<=report->blocks || report->target_blocks>UINT32_MAX ||
       report->grow_root_placement>DOLLY_FFS_ROOT_NEW_SPACE)return 0;
    words=report->block_bytes/4u;
    coverage=(words-1u)*32u;
    old_pages=bitmap_page_count(report->blocks,report->reserved_blocks,coverage);
    new_pages=bitmap_page_count(report->target_blocks,report->reserved_blocks,coverage);
    old_count=bitmap_extension_count(old_pages,words);
    new_count=bitmap_extension_count(new_pages,words);
    if(index>=new_count || (original!=NULL)!=(index<old_count) ||
       report->grow_bitmap_pages!=new_pages-old_pages ||
       report->grow_bitmap_extensions!=new_count-old_count)return 0;

    first=ROOT_PAGES+index*(words-1u);
    if(original) {
        for(i=0;i<words-1u;++i) {
            uint32_t ref=word(original,i);
            if(first+i<old_pages ? ref<report->reserved_blocks || ref>=report->blocks : ref!=0)
                return 0;
        }
        next=word(original,words-1u);
        if(index+1u<old_count ? next<report->reserved_blocks || next>=report->blocks : next!=0)
            return 0;
    }
    if(index+1u>=old_count && index+1u<new_count) {
        if(!dolly_ffs_grow_metadata_block(report,1,index+1u-old_count,&placement))return 0;
        next=(uint32_t)placement;
    }
    if(report->grow_bitmap_pages &&
       !dolly_ffs_grow_metadata_block(report,0,report->grow_bitmap_pages-1u,&placement))
        return 0;
    for(i=0;i<words-1u;++i) {
        uint32_t page=first+i,ref=0;
        if(page<old_pages)ref=word(original,i);
        else if(page<new_pages) {
            (void)dolly_ffs_grow_metadata_block(report,0,page-old_pages,&placement);
            ref=(uint32_t)placement;
        }
        store_word(output,i,ref);
    }
    store_word(output,words-1u,next);
    return 1;
}
