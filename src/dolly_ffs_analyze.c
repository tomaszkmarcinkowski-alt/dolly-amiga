#include "dolly_ffs_analyze.h"
#include <stdlib.h>

enum { ROOT_PAGES=25 };

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

static const char *analyze(DollyBlockSource *source,uint64_t start,uint64_t length,
                              uint32_t block_bytes,uint32_t reserved,uint32_t expected_dos_type,
                              uint64_t target_bytes,DollyFfsReport *out,DollyFfsProgress progress,
                              DollyFfsRange range,void *context) {
    DollyFfsReport report={0};
    unsigned char *buffer;
    uint32_t *pages=NULL,*metadata=NULL;
    uint32_t blocks,words,coverage,needed,extension,ext_needed,root,i,j,n=0,m=0,checked=0;
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
    if((uint64_t)(needed+ext_needed+1u)*sizeof(*metadata)>SIZE_MAX)
        return error;
    buffer=malloc(block_bytes);
    pages=malloc((size_t)needed*sizeof(*pages));
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
    extension=word(buffer,words-24u);
    for(i=0;i<ext_needed;++i) {
        if(extension<reserved || extension>=blocks)goto done;
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
    if(report.target_blocks>blocks) {
        uint32_t new_pages=bitmap_page_count(report.target_blocks,reserved,coverage);
        uint32_t new_extensions=bitmap_extension_count(new_pages,words);
        uint32_t target_root=(uint32_t)((report.target_blocks+reserved-1u)/2u);
        report.grow_added_blocks=report.target_blocks-blocks;
        report.grow_root_block=target_root;
        report.grow_tail_available=report.grow_added_blocks-(target_root>=blocks?1u:0u);
        report.grow_bitmap_pages=new_pages-needed;
        report.grow_bitmap_extensions=new_extensions-ext_needed;
        if(target_root==root)report.grow_root_placement=DOLLY_FFS_ROOT_UNCHANGED;
        else if(target_root>=blocks)report.grow_root_placement=DOLLY_FFS_ROOT_NEW_SPACE;
        else if(bsearch(&target_root,metadata,m,sizeof(*metadata),compare_blocks))
            report.grow_root_placement=DOLLY_FFS_ROOT_METADATA;
        else report.grow_root_placement=DOLLY_FFS_ROOT_FREE;
    }
    if(target_bytes && report.target_blocks<reserved) {
        report.first_outside_target=report.target_blocks;
        report.used_outside_target=reserved-report.target_blocks;
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
            if(report.grow_added_blocks && report.grow_root_placement==DOLLY_FFS_ROOT_FREE &&
               report.grow_root_block>=base && report.grow_root_block<end &&
               (used&(UINT32_C(1)<<(report.grow_root_block-base))))
                report.grow_root_placement=DOLLY_FFS_ROOT_OCCUPIED;
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
    if(report.grow_added_blocks && report.grow_root_placement<=DOLLY_FFS_ROOT_NEW_SPACE &&
       (uint64_t)report.grow_bitmap_pages+report.grow_bitmap_extensions<=report.grow_tail_available) {
        uint32_t root_page=(uint32_t)((report.grow_root_block-reserved)/coverage);
        error="Could not build the planned FFS allocation bitmap.";
        /* The old boundary page and the future root page are the only
         * existing pages whose in-volume bits may need changing. */
        if(!read_block(source,start,block_bytes,pages[needed-1u],buffer) ||
           !dolly_ffs_grow_bitmap_page(&report,needed-1u,buffer,buffer))
            goto done;
        if(root_page<needed-1u &&
           (!read_block(source,start,block_bytes,pages[root_page],buffer) ||
            !dolly_ffs_grow_bitmap_page(&report,root_page,buffer,buffer)))goto done;
        if(report.grow_bitmap_pages &&
           (!dolly_ffs_grow_bitmap_page(&report,needed,NULL,buffer) ||
            !dolly_ffs_grow_bitmap_page(&report,needed+report.grow_bitmap_pages-1u,NULL,buffer)))
            goto done;
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
                   out,progress,NULL,NULL);
}

const char *dolly_ffs_copy_plan(DollyBlockSource *source,uint64_t start,uint64_t length,
                              uint32_t block_bytes,uint32_t reserved,uint32_t dos_type,
                              DollyFfsReport *out,DollyFfsProgress progress,
                              DollyFfsRange range,void *context) {
    if(!range)return "Missing FFS copy range visitor.";
    return analyze(source,start,length,block_bytes,reserved,dos_type,length,out,
                   progress,range,context);
}

const char *dolly_ffs_analyze(DollyBlockSource *source,uint64_t start,uint64_t length,
                              uint32_t block_bytes,uint32_t reserved,
                              uint32_t expected_dos_type,uint64_t target_bytes,
                              DollyFfsReport *out) {
    return dolly_ffs_analyze_with_progress(source,start,length,block_bytes,reserved,
                                          expected_dos_type,target_bytes,out,NULL);
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

int dolly_ffs_grow_bitmap_page(const DollyFfsReport *report,uint32_t index,
                              const unsigned char *original,unsigned char *output) {
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
        bits|=word_range(base,report->blocks,report->target_blocks);
        bits&=~word_range(base,report->blocks,metadata_end);
        bits&=~word_range(base,report->grow_root_block,report->grow_root_block+1u);
        store_word(output,j,bits);
        sum+=bits;
    }
    store_word(output,0,0u-sum);
    return 1;
}
