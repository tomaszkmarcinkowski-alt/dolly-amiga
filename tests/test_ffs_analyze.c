#include "dolly_ffs_analyze.h"
#include "dolly_clone_names.h"
#include <assert.h>
#include <string.h>

enum { BLOCKS=103000, BYTES=512, COVERAGE=4064, PAGES=26 };
static const uint64_t start=UINT64_C(0x100010000);
static int damage;
static void put(unsigned char *b,unsigned i,uint32_t x) {
    b+=i*4u;
    b[0]=(unsigned char)(x>>24); b[1]=(unsigned char)(x>>16);
    b[2]=(unsigned char)(x>>8); b[3]=(unsigned char)x;
}
static void mark(unsigned char *b,uint32_t page,uint32_t key) {
    uint32_t relative=key-2u;
    if(relative/COVERAGE==page) {
        uint32_t bit=relative%COVERAGE;
        b[4u+(bit/32u)*4u+3u-(bit%32u)/8u]&=
            (unsigned char)~(1u<<(bit%8u));
    }
}
static void seal(unsigned char *b,unsigned checksum) {
    uint32_t sum=0;
    unsigned i;
    for(i=0;i<128u;++i) {
        const unsigned char *p=b+i*4u;
        sum+=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
    }
    put(b,checksum,0u-sum);
}
static uint32_t boot_dos_type=0x444f5303;
static DollyBlockReadResult read_fixture(void *context,uint64_t off,void *data,size_t bytes) {
    unsigned char *b=data;
    uint32_t block=(uint32_t)((off-start)/BYTES),i;
    uint32_t extension=damage==6?51501u:500u;
    (void)context;
    assert(bytes==BYTES && off>=start && (off-start)%BYTES==0);
    memset(b,0,bytes);
    if(block==0) {
        put(b,0,boot_dos_type);
        put(b,2,51500);
    } else if(block==51500) {
        put(b,0,2);put(b,127,1);
        put(b,78,damage==1?0:UINT32_MAX);
        for(i=0;i<25;++i)put(b,79+i,100+i);
        if(damage!=7)put(b,104,extension);
        seal(b,5);
    } else if(block==extension) {
        put(b,0,damage==2?100:125);
        if(damage==3)put(b,127,500);
    } else if(block>=100 && block<100+PAGES) {
        uint32_t page=block-100;
        memset(b+4,255,bytes-4);
        if(damage!=4)mark(b,page,51500);
        if(damage!=7)mark(b,page,extension);
        for(i=0;i<(damage==7?25u:PAGES);++i)mark(b,page,100+i);
        mark(b,page,32);mark(b,page,33);
        mark(b,page,102991);mark(b,page,102999);
        seal(b,0);
        if(damage==5 && page==25)b[31]^=1;
    } else return (DollyBlockReadResult){DOLLY_BLOCK_EIO,0};
    return (DollyBlockReadResult){DOLLY_BLOCK_OK,bytes};
}
static int is_free(const unsigned char *page,uint32_t index,uint32_t block) {
    uint32_t bit=block-2u-index*COVERAGE;
    assert(bit<COVERAGE);
    return (page[4u+(bit/32u)*4u+3u-(bit%32u)/8u]&(1u<<(bit%8u)))!=0;
}

static void test_bitmap_growth(DollyBlockSource *source,uint64_t length) {
    DollyFfsReport r;
    unsigned char original[BYTES],page[BYTES],checksum[BYTES];
    uint64_t last_metadata;
    uint32_t page_index;
    assert(!dolly_ffs_analyze(source,start,length,BYTES,2,0x444f5303,length*2u,&r));
    assert(dolly_block_read(source,start+125u*BYTES,original,BYTES).status==DOLLY_BLOCK_OK);
    assert(dolly_ffs_grow_bitmap_page(&r,25,original,page));
    assert(!is_free(page,25,102999) && !is_free(page,25,103000) &&
           !is_free(page,25,103001));
    assert(dolly_ffs_grow_metadata_block(&r,0,r.grow_bitmap_pages-1u,&last_metadata));
    assert(is_free(page,25,(uint32_t)last_metadata+1u));
    memcpy(checksum,page,sizeof(checksum));
    put(checksum,0,0);
    seal(checksum,0);
    assert(!memcmp(checksum,page,sizeof(page)));
    assert(dolly_ffs_grow_bitmap_page(&r,25,original,original));
    assert(!memcmp(original,page,sizeof(page)));

    page_index=(uint32_t)((r.target_blocks-3u)/COVERAGE);
    assert(dolly_ffs_grow_bitmap_page(&r,page_index,NULL,page));
    assert(is_free(page,page_index,(uint32_t)r.target_blocks-1u));
    assert(!is_free(page,page_index,(uint32_t)r.target_blocks));
    assert(!dolly_ffs_grow_bitmap_page(&r,page_index+1u,NULL,page));

    assert(!dolly_ffs_analyze(source,start,length,BYTES,2,0x444f5303,length+BYTES,&r));
    page_index=(uint32_t)((r.grow_root_block-2u)/COVERAGE);
    assert(dolly_block_read(source,start+(100u+page_index)*BYTES,original,BYTES).status==DOLLY_BLOCK_OK);
    assert(is_free(original,page_index,(uint32_t)r.grow_root_block));
    assert(dolly_ffs_grow_bitmap_page(&r,page_index,original,page));
    assert(!is_free(page,page_index,(uint32_t)r.grow_root_block));
    assert(!is_free(page,page_index,(uint32_t)r.root_block));
    original[0]^=1;
    assert(!dolly_ffs_grow_bitmap_page(&r,page_index,original,page));
}

static void keep(void *context) {(void)context;}
static unsigned progress_calls,last_percent;
static void record_progress(unsigned percent) {
    assert(progress_calls ? percent>last_percent : percent==0);
    assert(percent<=100);
    last_percent=percent;
    ++progress_calls;
}

static int check_range(void *context,uint64_t offset,uint64_t length) {
    static const uint32_t blocks[][2]={
        {0,2},{32,2},{100,26},{500,1},{51500,1},{102991,1},{102999,1}
    };
    unsigned *index=context;
    assert(*index<sizeof(blocks)/sizeof(blocks[0]));
    assert(offset==(uint64_t)blocks[*index][0]*BYTES);
    assert(length==(uint64_t)blocks[*index][1]*BYTES);
    ++*index;
    return 1;
}

static void test_clone_names(void) {
    unsigned char block[BYTES],before[BYTES];
    char name[32];
    memset(block,0,sizeof(block));
    memcpy(block,"PART",4);
    put(block,1,128);
    put(block,32,16);
    memcpy(block+36,"\3DH0",4);
    seal(block,2);
    memcpy(before,block,sizeof(block));
    assert(dolly_clone_read_name(DOLLY_CLONE_PART_NAME,block,BYTES,name));
    assert(!strcmp(name,"DH0"));
    assert(!dolly_clone_rename(DOLLY_CLONE_PART_NAME,block,BYTES,"DH1","DH0_0"));
    assert(!memcmp(block,before,sizeof(block)));
    assert(dolly_clone_rename(DOLLY_CLONE_PART_NAME,block,BYTES,"DH0","DH0_0"));
    assert(dolly_clone_read_name(DOLLY_CLONE_PART_NAME,block,BYTES,name));
    assert(!strcmp(name,"DH0_0"));
    assert(!memcmp(block+68,before+68,BYTES-68));

    memset(block,0,sizeof(block));
    put(block,0,2);
    put(block,78,UINT32_MAX);
    put(block,127,1);
    memcpy(block+432,"\4Work",5);
    seal(block,5);
    memcpy(before,block,sizeof(block));
    assert(dolly_clone_rename(DOLLY_CLONE_VOLUME_NAME,block,BYTES,"Work","Copy_of_Work"));
    assert(dolly_clone_read_name(DOLLY_CLONE_VOLUME_NAME,block,BYTES,name));
    assert(!strcmp(name,"Copy_of_Work"));
    assert(!memcmp(block+24,before+24,432-24));
    assert(!memcmp(block+463,before+463,BYTES-463));
    block[38]=1;
    assert(!dolly_clone_read_name(DOLLY_CLONE_VOLUME_NAME,block,BYTES,name));
    memset(block,0,sizeof(block));
    assert(!dolly_clone_read_name(DOLLY_CLONE_VOLUME_NAME,block,BYTES,name));
}

int main(void) {
    DollyBlockSource *source=NULL;
    DollyFfsReport r;
    unsigned ranges=0;
    uint64_t length=(uint64_t)BLOCKS*BYTES,placement;
    test_clone_names();
    assert(dolly_block_open_backend(start+length,read_fixture,keep,NULL,&source)==DOLLY_BLOCK_OK);
    assert(!dolly_ffs_analyze_with_progress(source,start,length,BYTES,2,0x444f5303,
                                             102992u*512u,&r,record_progress));
    assert(progress_calls>1 && last_percent==100);
    assert(!dolly_ffs_copy_plan(source,start,length,BYTES,2,0x444f5303,
                                &r,NULL,check_range,&ranges));
    assert(ranges==7 && r.copy_runs==ranges);
    boot_dos_type=0x444f5300;
    ranges=0;
    assert(!dolly_ffs_copy_plan(source,start,length,BYTES,2,boot_dos_type,
                                &r,NULL,check_range,&ranges));
    assert(ranges==7 && r.dos_type==boot_dos_type);
    boot_dos_type=0x444f5303;
    assert(!dolly_ffs_analyze(source,start,length,BYTES,2,0x444f5303,102992u*512u,&r));
    assert(r.blocks==BLOCKS && r.used==34 && r.copy_runs==7 && r.root_block==51500);
    /* Blocks 32 and 33 form one range across a bitmap word boundary. */
    assert(r.highest_used==102999 && r.minimum_blocks==103000);
    assert(r.used_outside_target==1 && r.first_outside_target==102999);
    assert(!dolly_ffs_analyze(source,start,length,BYTES,2,0x444f5303,102991u*512u,&r));
    assert(r.used_outside_target==2 && r.first_outside_target==102991);
    assert(!dolly_ffs_analyze(source,start,length,BYTES,2,0x444f5303,length,&r));
    assert(!r.used_outside_target && !r.grow_added_blocks);
    assert(!dolly_ffs_analyze(source,start,length,BYTES,2,0x444f5303,
                              (uint64_t)(BLOCKS+1u)*BYTES,&r));
    assert(r.grow_added_blocks==1 && r.grow_root_block==51501 &&
           r.grow_root_placement==DOLLY_FFS_ROOT_FREE && !r.grow_bitmap_pages);
    assert(!dolly_ffs_analyze(source,start,length,BYTES,2,0x444f5303,
                              UINT64_C(205981)*BYTES,&r));
    assert(r.grow_root_block==102991 && r.grow_root_placement==DOLLY_FFS_ROOT_OCCUPIED);
    assert(!dolly_ffs_analyze(source,start,length,BYTES,2,0x444f5303,
                              ((uint64_t)152*COVERAGE+3u)*BYTES,&r));
    assert(r.grow_root_placement==DOLLY_FFS_ROOT_NEW_SPACE &&
           r.grow_bitmap_pages==127 && r.grow_bitmap_extensions==1);
    assert(dolly_ffs_grow_metadata_block(&r,0,0,&placement) && placement==BLOCKS);
    assert(dolly_ffs_grow_metadata_block(&r,0,126,&placement) && placement==BLOCKS+126u);
    assert(dolly_ffs_grow_metadata_block(&r,1,0,&placement) && placement==BLOCKS+127u);
    assert(!dolly_ffs_grow_metadata_block(&r,1,1,&placement));
    assert(!dolly_ffs_analyze(source,start,length,BYTES,2,0x444f5303,
                              (uint64_t)BLOCKS*BYTES*2u,&r));
    assert(r.grow_root_block==BLOCKS && r.grow_tail_available==BLOCKS-1u);
    assert(dolly_ffs_grow_metadata_block(&r,0,0,&placement) && placement==BLOCKS+1u);
    assert(dolly_ffs_analyze(source,start,length,BYTES,2,0x444f5303,
                             (UINT64_C(0x100000000))*BYTES,&r));
    assert(dolly_ffs_analyze(source,start,length,BYTES,2,0x444f5301,length,&r));
    test_bitmap_growth(source,length);
    for(damage=1;damage<=5;++damage)
        assert(dolly_ffs_analyze(source,start,length,BYTES,2,0x444f5303,length,&r));
    damage=5;
    progress_calls=0;
    assert(dolly_ffs_analyze_with_progress(source,start,length,BYTES,2,0x444f5303,
                                            length,&r,record_progress));
    assert(progress_calls>1 && last_percent<100);
    damage=6;
    assert(!dolly_ffs_analyze(source,start,length,BYTES,2,0x444f5303,
                              (uint64_t)(BLOCKS+1u)*BYTES,&r));
    assert(r.grow_root_placement==DOLLY_FFS_ROOT_METADATA);
    damage=7;
    {
        uint64_t exact_coverage=(UINT64_C(25)*COVERAGE+2u)*BYTES;
        assert(!dolly_ffs_analyze(source,start,exact_coverage,BYTES,2,0x444f5303,
                                  exact_coverage+BYTES,&r));
        assert(r.grow_bitmap_pages==1 && r.grow_bitmap_extensions==1 &&
               r.grow_tail_available==1);
        assert(!dolly_ffs_grow_metadata_block(&r,0,0,&placement));
        assert(!dolly_ffs_grow_metadata_block(&r,1,0,&placement));
    }
    dolly_block_close(source);
    return 0;
}
