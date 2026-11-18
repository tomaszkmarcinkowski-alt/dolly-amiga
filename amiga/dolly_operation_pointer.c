#include "dolly_operation_pointer.h"
#include "dolly_dlrp.h"
#include "dolly_checksum.h"
#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <string.h>

extern struct Library *AslBase;

static int sink(void *ctx,const void *data,size_t size) {
    return Write(*(BPTR *)ctx,(APTR)data,(LONG)size)==(LONG)size;
}
typedef struct PointerSink {
    BPTR file;
    DollyChecksum checksum;
} PointerSink;

static int checksum_sink(void *ctx,const void *data,size_t size) {
    PointerSink *out=ctx;
    return sink(&out->file,data,size) && dolly_checksum_update(&out->checksum,data,size);
}
static void notice(struct Window *parent,const char *message) {
    struct EasyStruct request={sizeof(struct EasyStruct),0,
        (STRPTR)"Dolly - Operation Restore File",(STRPTR)"%s",(STRPTR)"OK"};
    ULONG args[]={(ULONG)message};
    EasyRequestArgs(parent,&request,NULL,args);
}
static int choose_path(struct Window *parent,char path[256]) {
    struct FileRequester *request;
    int result=0;
    if(!AslBase)return -1;
    request=(struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
        ASL_FuncFlags,(ULONG)FILF_SAVE,ASLFR_Window,(ULONG)parent,
        ASLFR_TitleText,(ULONG)"Save Operation Restore File",
        ASLFR_InitialFile,(ULONG)"operation.dlrp",TAG_DONE);
    if(!request)return -1;
    if(AslRequest(request,NULL)) {
        const char *drawer=request->fr_Drawer?(const char *)request->fr_Drawer:"";
        const char *file=(const char *)request->fr_File;
        result=-1;
        if(strlen(drawer)<256 && file && *file && !strpbrk(file,":/") &&
           strcmp(file,".") && strcmp(file,"..")) {
            strcpy(path,drawer);
            if(AddPart((STRPTR)path,(STRPTR)file,256))result=1;
        }
    }
    FreeAslRequest(request);
    return result;
}
static int unused(const char *path) {
    BPTR lock=Lock((STRPTR)path,ACCESS_READ);
    if(lock){UnLock(lock);return 0;}
    return IoErr()==ERROR_OBJECT_NOT_FOUND;
}
static uint32_t string_chunk_size(const char *s) {
    uint32_t n=(uint32_t)strlen(s);
    return 8u+n+(n&1u);
}
static uint32_t anchor(DollySurfaceScan *scan,uint64_t lba,unsigned *flags,unsigned bit) {
    DollyChecksum checksum;
    DollyBlockReadResult read=dolly_block_read(scan->source,lba*scan->sector_bytes,
                                               scan->buffer,scan->sector_bytes);
    if(read.status!=DOLLY_BLOCK_OK || read.bytes_read!=scan->sector_bytes)return 0;
    dolly_checksum_init(&checksum);
    dolly_checksum_update(&checksum,scan->buffer,scan->sector_bytes);
    *flags|=bit;
    return dolly_checksum_final(&checksum);
}

static int write_pointer(BPTR file,const DollyEndpoint *source,
                         const DollyEndpoint *destination,DollySurfaceScan *scan,
                         DollyAmigaImage *image,const char *final,const char *partial,
                         uint32_t output_bytes) {
    unsigned char head[108]={0},geometry[36]={0},state[24]={0},data[20];
    const DollySurfaceLogPage *page;
    unsigned flags=0,i;
    PointerSink out;
    uint32_t body=8u+sizeof(head)+8u+sizeof(geometry)+12u;
    uint32_t first_crc=anchor(scan,scan->first_lba,&flags,1);
    uint32_t last_crc=anchor(scan,scan->first_lba+scan->total-1,&flags,2);
    body+=string_chunk_size(scan->source_path)+string_chunk_size(scan->device.driver)+
          string_chunk_size(scan->device_name);
    if(image)body+=8u+sizeof(state)+string_chunk_size(final)+string_chunk_size(partial);
    for(page=scan->log.first;page;page=page->next) {
        if(page->count>(INT32_MAX-12u-body)/28u)return 0;
        body+=28u*page->count;
    }
    dolly_dlrp_u32(head,1);
    dolly_dlrp_u32(head+4,image?2:1);
    dolly_dlrp_u32(head+8,source->kind);
    dolly_dlrp_u32(head+12,scan->device.unit);
    dolly_dlrp_u32(head+16,scan->sector_bytes);
    dolly_dlrp_u32(head+20,scan->transfer_bytes);
    dolly_dlrp_u64(head+24,scan->first_lba);
    dolly_dlrp_u64(head+32,scan->total);
    dolly_dlrp_u64(head+40,scan->processed);
    dolly_dlrp_u64(head+48,scan->geometry.block_count);
    dolly_dlrp_u32(head+56,scan->errors);
    dolly_dlrp_u32(head+60,scan->retries);
    dolly_dlrp_u32(head+64,scan->fallback);
    dolly_dlrp_u32(head+68,scan->retry_pending);
    dolly_dlrp_u64(head+72,scan->log.omitted);
    dolly_dlrp_u32(head+80,flags);
    dolly_dlrp_u32(head+84,first_crc);
    dolly_dlrp_u32(head+88,last_crc);
    dolly_dlrp_u32(head+92,scan->effective_io.mask);
    dolly_dlrp_u32(head+96,scan->effective_io.buffer_memory);
    dolly_dlrp_u32(head+100,scan->effective_io.max_transfer);
    dolly_dlrp_u32(head+104,source->partition);
    dolly_dlrp_u32(geometry,scan->geometry.capacity_source);
    dolly_dlrp_u32(geometry+4,scan->geometry.cylinders);
    dolly_dlrp_u32(geometry+8,scan->geometry.heads);
    dolly_dlrp_u32(geometry+12,scan->geometry.sectors_per_track);
    dolly_dlrp_u32(geometry+16,scan->effective_io.present);
    dolly_dlrp_u64(geometry+20,source->partition_start_bytes);
    dolly_dlrp_u64(geometry+28,source->partition_length_bytes);
    out.file=file;
    dolly_checksum_init(&out.checksum);
    if(!dolly_dlrp_form(checksum_sink,&out,body) ||
       !dolly_dlrp_chunk(checksum_sink,&out,"HEAD",head,sizeof(head)) ||
       !dolly_dlrp_chunk(checksum_sink,&out,"GEOM",geometry,sizeof(geometry)) ||
       !dolly_dlrp_chunk(checksum_sink,&out,"SRCE",scan->source_path,strlen(scan->source_path)) ||
       !dolly_dlrp_chunk(checksum_sink,&out,"DRVR",scan->device.driver,strlen(scan->device.driver)) ||
       !dolly_dlrp_chunk(checksum_sink,&out,"NAME",scan->device_name,strlen(scan->device_name)))
        return 0;
    if(image) {
        dolly_dlrp_u32(state,destination->image_format);
        dolly_dlrp_u32(state+4,output_bytes);
        dolly_dlrp_u32(state+8,image->dly.compression);
        dolly_dlrp_u32(state+12,image->dly.file_bytes);
        dolly_dlrp_u64(state+16,image->dly.written);
        if(!dolly_dlrp_chunk(checksum_sink,&out,"OUTP",state,sizeof(state)) ||
           !dolly_dlrp_chunk(checksum_sink,&out,"DEST",final,strlen(final)) ||
           !dolly_dlrp_chunk(checksum_sink,&out,"PART",partial,strlen(partial)))return 0;
    }
    for(page=scan->log.first;page;page=page->next)
        for(i=0;i<page->count;++i) {
            const DollySurfaceEvent *event=&page->entries[i];
            dolly_dlrp_u64(data,event->lba);
            dolly_dlrp_u32(data+8,event->sectors);
            dolly_dlrp_u32(data+12,event->kind);
            dolly_dlrp_u32(data+16,event->status);
            if(!dolly_dlrp_chunk(checksum_sink,&out,"EVNT",data,sizeof(data)))return 0;
        }
    dolly_dlrp_u32(data,dolly_checksum_final(&out.checksum));
    return dolly_dlrp_chunk(sink,&file,"CR32",data,4);
}

void dolly_operation_pointer_offer(struct Window *parent,const DollyEndpoint *source,
                         const DollyEndpoint *destination,DollySurfaceScan *scan,
                         DollyAmigaImage *image) {
    struct EasyStruct request={sizeof(struct EasyStruct),0,
        (STRPTR)"Dolly - Operation interrupted",
        (STRPTR)"Save an Operation Restore File for later?",
        (STRPTR)"Save...|Cancel"};
    char path[256],temp[267],file_path[273],final[256],partial[256];
    BPTR drawer,file;
    LONG output=0;
    size_t prefix;
    int ok,choice;
    if(!image || !image->failure || !image->file || !scan->source ||
       !scan->total || scan->processed>scan->total ||
       EasyRequestArgs(parent,&request,NULL,NULL)!=1)return;
    choice=choose_path(parent,path);
    if(choice<=0) {
        if(choice<0)notice(parent,"The save requester or filename is unavailable.");
        return;
    }
    if(!unused(path)) {
        notice(parent,"The checkpoint destination is already in use.");
        return;
    }
    if(image) {
        if(!NameFromLock(image->drawer,final,sizeof(final)))goto failed;
        strcpy(partial,final);
        if(!AddPart((STRPTR)final,(STRPTR)image->name,sizeof(final)) ||
           !AddPart((STRPTR)partial,(STRPTR)image->staged_file,sizeof(partial)))goto failed;
        output=Seek(image->file,0,OFFSET_CURRENT);
        uint64_t committed=image->dly.file_bytes?image->dly.emitted:
                           scan->processed*scan->sector_bytes;
        if(output<0 || (uint64_t)output<committed || committed>INT32_MAX)goto failed;
        if((uint64_t)output>committed &&
           SetFileSize(image->file,(LONG)committed,OFFSET_BEGINNING)!=(LONG)committed)
            goto failed;
        output=(LONG)committed;
    }
    prefix=(size_t)((const char *)FilePart((STRPTR)path)-path);
    memcpy(temp,path,prefix);
    strcpy(temp+prefix,".dolly-dlrp");
    strcpy(file_path,temp);strcat(file_path,"/data");
    drawer=CreateDir((STRPTR)temp);
    if(!drawer)goto failed;
    UnLock(drawer);
    file=Open((STRPTR)file_path,MODE_NEWFILE);
    if(!file) {
        DeleteFile((STRPTR)temp);
        goto failed;
    }
    ok=1;
    if(image) {
        ok=Close(image->file)!=0;
        image->file=0;
    }
    if(ok)ok=write_pointer(file,source,destination,scan,image,final,partial,(uint32_t)output);
    if(!Close(file))ok=0;
    if(ok)ok=unused(path) && Rename((STRPTR)file_path,(STRPTR)path);
    if(!ok)DeleteFile((STRPTR)file_path);
    DeleteFile((STRPTR)temp);
    if(!ok)goto failed;
    if(image)image->keep_staging=1;
    notice(parent,"Operation Restore File saved.");
    return;
failed:
    notice(parent,"Could not save the Operation Restore File.");
}
