#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#include "dolly_dly_reader.h"
#include "dolly_dly_writer.h"
#include "dolly_ffs_analyze.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#include <winioctl.h>
#include <bcrypt.h>
#else
#include <unistd.h>
#endif

enum { SECTOR = 512, BATCH = 16384, PATH_CAP = 4096, DEFAULT_PART = 524288 };

static const char *name_start(const char *path) {
    const char *name=path;
    while(*path) {
        if(*path=='/'
#ifdef _WIN32
           || *path=='\\' || *path==':'
#endif
          )name=path+1;
        ++path;
    }
    return name;
}

static int member_name(char path[PATH_CAP],const char *prefix,size_t length,uint32_t index) {
    int count;
    if(length>=PATH_CAP)return 0;
    memcpy(path,prefix,length);
    count=index?snprintf(path+length,PATH_CAP-length,".%03" PRIu32,index):
                snprintf(path+length,PATH_CAP-length,".dly");
    return count>0 && (size_t)count<PATH_CAP-length;
}

static FILE *new_file(const char *path) {
    int fd;
    FILE *file;
#ifdef _WIN32
    fd=_open(path,_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY,_S_IREAD|_S_IWRITE);
    if(fd<0)return NULL;
    file=_fdopen(fd,"wb");
    if(!file)_close(fd);
#else
    fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0666);
    if(fd<0)return NULL;
    file=fdopen(fd,"wb");
    if(!file)close(fd);
#endif
    if(!file)remove(path);
    return file;
}

static int enable_sparse(FILE *file) {
#ifdef _WIN32
    HANDLE handle=(HANDLE)_get_osfhandle(_fileno(file));
    DWORD bytes;
    return handle!=INVALID_HANDLE_VALUE &&
           DeviceIoControl(handle,FSCTL_SET_SPARSE,NULL,0,NULL,0,&bytes,NULL)!=0;
#else
    /* Unwritten extents read as zero on POSIX. Physical allocation
     * still depends on the destination filesystem. */
    (void)file;
    return 1;
#endif
}

static int seek_sparse(FILE *file,uint64_t position) {
    if(position>INT64_MAX)return 0;
#ifdef _WIN32
    return _fseeki64(file,(int64_t)position,SEEK_SET)==0;
#else
    return (uint64_t)(off_t)position==position &&
           fseeko(file,(off_t)position,SEEK_SET)==0;
#endif
}

static int make_directory(const char *path) {
#ifdef _WIN32
    return _mkdir(path)==0;
#else
    return mkdir(path,0777)==0;
#endif
}

static void remove_directory(const char *path) {
#ifdef _WIN32
    (void)_rmdir(path);
#else
    (void)rmdir(path);
#endif
}

static int new_set_id(uint64_t *id) {
#ifdef _WIN32
    return BCryptGenRandom(NULL,(PUCHAR)id,sizeof(*id),BCRYPT_USE_SYSTEM_PREFERRED_RNG)==0 && *id;
#else
    FILE *random=fopen("/dev/urandom","rb");
    int ok;
    if(!random)return 0;
    ok=fread(id,1,sizeof(*id),random)==sizeof(*id) && *id;
    fclose(random);
    return ok;
#endif
}

static int write_bytes(void *context,const void *data,size_t bytes) {
    return fwrite(data,1,bytes,(FILE *)context)==bytes;
}

static int seek_output(void *context,uint32_t offset) {
    /* The DOLY writer bounds every part to the signed 32-bit range. */
    return fseek((FILE *)context,(long)offset,SEEK_SET)==0;
}

static int split(const char *input,const char *directory,uint32_t per_part,int fast) {
    char prefix[PATH_CAP],path[PATH_CAP];
    const char *name=name_start(input),*dot=strrchr(name,'.');
    const size_t stem=dot && dot!=name?(size_t)(dot-name):strlen(name);
    DollyBlockSource *source=NULL;
    DollyDlySetInfo set={0};
    unsigned char buffer[BATCH],packed[BATCH];
    uint64_t bytes,parts,offset=0;
    uint32_t made=0;
    int length,ok=0;
    length=snprintf(prefix,sizeof(prefix),"%s/%.*s",directory,
                    (int)(stem<PATH_CAP?stem:0),name);
    if(!stem || stem>=PATH_CAP || length<0 || (size_t)length>=sizeof(prefix)) {
        fprintf(stderr,"Output path is too long or missing a filename.\n");
        return 0;
    }
    if(dolly_block_open_file(input,&source)!=DOLLY_BLOCK_OK) {
        fprintf(stderr,"Cannot open input: %s\n",input);
        return 0;
    }
    bytes=dolly_block_size(source);
    if(!bytes || bytes%SECTOR) {
        fprintf(stderr,"Input must be a nonempty RAW/HDF with whole 512-byte sectors.\n");
        goto done;
    }
    if(bytes>=12u && dolly_block_read(source,0,buffer,12).status==DOLLY_BLOCK_OK &&
       !memcmp(buffer,"FORM",4) && !memcmp(buffer+8,"DOLY",4)) {
        fprintf(stderr,"Input is IFF DOLY. Decode it with join before splitting.\n");
        goto done;
    }
    set.sectors=bytes/SECTOR;
    parts=(set.sectors-1u)/per_part+1u;
    if(parts>UINT32_MAX || !member_name(path,prefix,(size_t)length,(uint32_t)(parts-1u))) {
        fprintf(stderr,"Too many parts or output path too long.\n");
        goto done;
    }
    set.parts=(uint32_t)parts;
    set.sectors_per_part=per_part;
    set.sector_bytes=SECTOR;
    if((parts>1 && !new_set_id(&set.set_id)) || !make_directory(directory)) {
        fprintf(stderr,"Cannot create a new output directory or set identifier: %s\n",directory);
        goto done;
    }
    while(made<set.parts) {
        DollyDlyWriter writer;
        uint64_t sectors=(bytes-offset)/SECTOR,remaining;
        FILE *file;
        int part_ok;
        if(sectors>per_part)sectors=per_part;
        remaining=sectors*SECTOR;
        if(!member_name(path,prefix,(size_t)length,made) || !(file=new_file(path)))break;
        ++made;
        set.part=made;
        part_ok=dolly_dly_init(&writer,offset/SECTOR,sectors,SECTOR,BATCH) &&
            (!fast || dolly_dly_compress(&writer,DOLLY_DLY_PACKBITS,packed,NULL,seek_output)) &&
            (set.parts>1?dolly_dly_begin_set(&writer,write_bytes,file,&set):
                         dolly_dly_begin(&writer,write_bytes,file));
        while(part_ok && remaining) {
            size_t chunk=remaining>BATCH?BATCH:(size_t)remaining;
            part_ok=dolly_block_read(source,offset,buffer,chunk).status==DOLLY_BLOCK_OK &&
                    dolly_dly_append(&writer,buffer,chunk);
            offset+=chunk;
            remaining-=chunk;
        }
        part_ok=part_ok && dolly_dly_finish(&writer);
        if(fclose(file)!=0 || !part_ok)break;
        if(made==set.parts)ok=1;
    }
    if(!ok) {
        fprintf(stderr,"Split failed near %s. Removing incomplete output.\n",path);
        while(made) {
            if(member_name(path,prefix,(size_t)length,--made) && remove(path)!=0)
                fprintf(stderr,"Could not remove %s\n",path);
        }
        remove_directory(directory);
    } else printf("Created %" PRIu32 " IFF DOLY file(s), %" PRIu64 " sectors.\n",set.parts,set.sectors);
done:
    dolly_block_close(source);
    return ok;
}

static int equal_set(const DollyDlySetInfo *first,const DollyDlySetInfo *next,uint32_t index) {
    return first->set_id==next->set_id && next->part==index+1u &&
           first->first_lba==next->first_lba && first->sectors==next->sectors &&
           first->parts==next->parts && first->sector_bytes==next->sector_bytes &&
           first->sectors_per_part==next->sectors_per_part;
}

static int legacy_member_name(char path[PATH_CAP],const char *first,uint32_t index) {
    size_t directory=(size_t)(name_start(first)-first);
    int count;
    if(directory>=PATH_CAP)return 0;
    memcpy(path,first,directory);
    count=snprintf(path+directory,PATH_CAP-directory,"part%08" PRIX32 ".dly",index+1u);
    return count>0 && (size_t)count<PATH_CAP-directory;
}

static int decode(const char *first,const char *destination,int sparse) {
    static const unsigned char zero[BATCH];
    unsigned char prefix[DOLLY_DLY_GEOM_END+DOLLY_DLY_NUMBERED_MSET_BYTES];
    unsigned char buffer[BATCH];
    char path[PATH_CAP];
    const char *dot=strrchr(name_start(first),'.');
    size_t stem=dot?(size_t)(dot-first):strlen(first);
    DollyDlySetInfo set={0};
    DollyBlockSource *container=NULL,*decoded=NULL;
    FILE *output=NULL;
    const char *filename=first;
    uint64_t total=0;
    uint32_t index=0;
    int ok=0,unwritten=0;
    do {
        DollyDlyImageInfo info;
        DollyDlySetInfo metadata;
        uint64_t offset=0,bytes;
        size_t header;
        int manifest;
        if(dolly_block_open_file(filename,&container)!=DOLLY_BLOCK_OK)goto done;
        bytes=dolly_block_size(container);
        header=bytes<sizeof(prefix)?(size_t)bytes:sizeof(prefix);
        if(dolly_block_read(container,0,prefix,header).status!=DOLLY_BLOCK_OK)goto done;
        manifest=header>=DOLLY_DLY_GEOM_END+4u && !memcmp(prefix+DOLLY_DLY_GEOM_END,"MSET",4);
        if(manifest && dolly_dly_set_unpack(prefix,header,&metadata)!=DOLLY_BLOCK_OK)goto done;
        if(!index && manifest) {
            if(metadata.part>1u) {
                fprintf(stderr,"Select the first part, not part %" PRIu32 "/%" PRIu32 ".\n",
                        metadata.part,metadata.parts);
                goto done;
            }
            set=metadata;
        } else if(set.part && (!manifest || !equal_set(&set,&metadata,index)))goto done;
        if(dolly_dly_open(container,&decoded,&info)!=DOLLY_BLOCK_OK)goto done;
        container=NULL;
        if(set.parts && !dolly_dly_set_matches(&set,index,&info))goto done;
        if(!index && destination) {
            output=new_file(destination);
            if(!output) {
                fprintf(stderr,"Cannot create new output (existing files are protected): %s\n",destination);
                goto done;
            }
            if(sparse && !enable_sparse(output)) {
                fprintf(stderr,"Cannot enable sparse output on this filesystem: %s\n",destination);
                goto done;
            }
        }
        bytes=dolly_block_size(decoded);
        while(offset<bytes) {
            size_t chunk=bytes-offset>BATCH?BATCH:(size_t)(bytes-offset);
            if(dolly_block_read(decoded,offset,buffer,chunk).status!=DOLLY_BLOCK_OK)goto done;
            if(output) {
                if(sparse && !memcmp(buffer,zero,chunk)) {
                    unwritten=1;
                } else {
                    if((unwritten && !seek_sparse(output,total+offset)) ||
                       !write_bytes(output,buffer,chunk))goto done;
                    unwritten=0;
                }
            }
            offset+=chunk;
        }
        total+=bytes;
        dolly_block_close(decoded);
        decoded=NULL;
        ++index;
        if(index<set.parts) {
            if(!(set.part?member_name(path,first,stem,index):legacy_member_name(path,first,index)))
                goto done;
            filename=path;
        }
    } while(index<set.parts);
    /* Seeking alone does not extend a file ending in an unwritten extent. */
    if(unwritten && (!seek_sparse(output,total-1u) || fputc(0,output)==EOF))goto done;
    ok=1;
done:
    dolly_block_close(decoded);
    dolly_block_close(container);
    if(output) {
        if(fclose(output)!=0)ok=0;
        if(!ok && remove(destination)!=0)fprintf(stderr,"Could not remove %s\n",destination);
    }
    if(ok)printf("Verified %" PRIu64 " bytes in %" PRIu32 " file(s).\n",total,index);
    else fprintf(stderr,"Could not verify or decode: %s\n",filename);
    return ok;
}

static int resize_partition(const char *input,const char *destination,uint64_t start,
                            uint64_t length,uint64_t target,uint32_t reserved) {
    DollyBlockSource *source=NULL,*resized=NULL,*result=NULL;
    FILE *output=NULL;
    unsigned char buffer[BATCH],zero[BATCH]={0};
    DollyFfsReport plan,verified;
    int created=0,ok=0;
    const char *error="Cannot open or analyze the source FFS partition.";
    if(dolly_block_open_file(input,&source)!=DOLLY_BLOCK_OK)goto done;
    if(start>dolly_block_size(source) || length>dolly_block_size(source)-start) {
        error="The source partition extent exceeds the file.";
        goto done;
    }
    error=dolly_ffs_open_resized_source(source,start,length,reserved,target,1,&plan,&resized);
    if(error)goto done;
    error="Cannot create a new destination. Existing files are protected.";
    output=new_file(destination);
    if(!output)goto done;
    created=1;
    error="Cannot allocate or copy the destination image.";
    int sparse=enable_sparse(output);
    if(!seek_sparse(output,target-1u) || fputc(0,output)==EOF)goto done;
    for(uint64_t offset=0;offset<target;) {
        size_t chunk=target-offset>BATCH?BATCH:(size_t)(target-offset);
        if(dolly_block_read(resized,offset,buffer,chunk).status!=DOLLY_BLOCK_OK ||
           ((!sparse || memcmp(buffer,zero,chunk)) &&
            (!seek_sparse(output,offset) || !write_bytes(output,buffer,chunk))))goto done;
        offset+=chunk;
    }
    error="Cannot close the resized image.";
    int close_status=fclose(output);
    output=NULL;
    if(close_status)goto done;

    error="Cannot reopen or validate the resized image.";
    if(dolly_block_open_file(destination,&result)!=DOLLY_BLOCK_OK ||
       dolly_block_size(result)!=target)goto done;
    error=dolly_ffs_analyze(result,0,target,SECTOR,reserved,plan.dos_type,target,&verified);
    if(error)goto done;
    error="The resized image does not match the planned root or allocation.";
    uint64_t expected_root=plan.grow_added_blocks?plan.grow_root_block:plan.shrink_root_block;
    uint64_t expected_free=plan.grow_added_blocks?plan.grow_preview_free:plan.shrink_preview_free;
    if(verified.root_block!=expected_root || verified.blocks-verified.used!=expected_free)goto done;
    printf("Verified resized FFS partition: %" PRIu64 " blocks, root %" PRIu64
           ", %" PRIu64 " free blocks.\n",verified.blocks,verified.root_block,expected_free);
    ok=1;
done:
    dolly_block_close(result);
    dolly_block_close(resized);
    dolly_block_close(source);
    if(output)fclose(output);
    if(!ok) {
        fprintf(stderr,"%s\n",error);
        if(created && remove(destination)!=0)fprintf(stderr,"Could not remove %s\n",destination);
    }
    return ok;
}

static int parse_sectors(const char *text,uint64_t *value) {
    char *end;
    if(*text<'0' || *text>'9')return 0;
    errno=0;
    uintmax_t sectors=strtoumax(text,&end,10);
    if(errno || *end || sectors>UINT64_MAX/SECTOR)return 0;
    *value=(uint64_t)sectors*SECTOR;
    return 1;
}

int main(int argc,char **argv) {
    if((argc==7 || argc==8) && !strcmp(argv[1],"resize-ffs")) {
        uint64_t start,length,target,reserved=2u*SECTOR;
        if(!parse_sectors(argv[4],&start) || !parse_sectors(argv[5],&length) ||
           !parse_sectors(argv[6],&target) ||
           (argc==8 && !parse_sectors(argv[7],&reserved)) ||
           !length || !target || reserved/SECTOR<2u || reserved/SECTOR>UINT32_MAX) {
            fprintf(stderr,"Provide decimal sector counts and at least two reserved blocks.\n");
            return 2;
        }
        return resize_partition(argv[2],argv[3],start,length,target,(uint32_t)(reserved/SECTOR))?0:1;
    }
    if(argc>=4 && argc<=6 && !strcmp(argv[1],"split")) {
        uint32_t per_part=DEFAULT_PART;
        int i,fast=0,part_given=0;
        for(i=4;i<argc;++i) {
            char *end;
            unsigned long n;
            if(!fast && !strcmp(argv[i],"--fast")) {
                fast=1;
                continue;
            }
            errno=0;
            n=strtoul(argv[i],&end,10);
            if(part_given || errno || end==argv[i] || *end || !n || n>DEFAULT_PART) {
                fprintf(stderr,"Use [PART_SECTORS] [--fast]. Part sectors must be between 1 and %u.\n",
                        DEFAULT_PART);
                return 2;
            }
            per_part=(uint32_t)n;
            part_given=1;
        }
        return split(argv[2],argv[3],per_part,fast)?0:1;
    }
    if(argc==3 && !strcmp(argv[1],"verify"))return decode(argv[2],NULL,0)?0:1;
    if((argc==4 || (argc==5 && !strcmp(argv[4],"--sparse"))) && !strcmp(argv[1],"join"))
        return decode(argv[2],argv[3],argc==5)?0:1;
    fprintf(stderr,"Usage:\n"
                   "  dolly-image split INPUT.raw NEW_DIRECTORY [PART_SECTORS] [--fast]\n"
                   "  dolly-image verify FIRST.dly\n"
                   "  dolly-image join FIRST.dly OUTPUT.raw [--sparse]\n"
                   "  dolly-image resize-ffs INPUT.raw NEW_PARTITION.raw START SOURCE_SECTORS TARGET_SECTORS [RESERVED]\n");
    return 2;
}
