#include "dolly_amiga_physical_catalog.h"
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <devices/trackdisk.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <string.h>
#include <strings.h>
extern struct DosLibrary *DOSBase;

static int region(const void *pointer,ULONG bytes) {
    const UBYTE *p=(const UBYTE *)pointer;
    return p && bytes && TypeOfMem((APTR)p)!=0 &&
           TypeOfMem((APTR)(p+bytes-1u))!=0;
}
static int bptr_valid(BPTR b,ULONG bytes) {
    return b && !((ULONG)b&0xc0000000ul) &&
           region(BADDR(b),bytes);
}
static int driver_name(char *out,BPTR b) {
    const UBYTE *data;
    ULONG n,i;
    const char *suffix=".device";
    if(!bptr_valid(b,1u))return 0;
    data=(const UBYTE *)BADDR(b);
    n=data[0];
    if(!n || n>DOLLY_PHYS_DRIVER_LEN ||
       !bptr_valid(b,n+1u))return 0;
    if(data[n]==0u)--n; /* optional counted terminator */
    if(n<7u || n>=DOLLY_PHYS_DRIVER_LEN)return 0;
    for(i=0;i<n;++i) {
        UBYTE ch=data[i+1u];
        if(ch<33u || ch>126u)return 0;
        out[i]=(char)ch;
    }
    out[n]='\0';
    for(i=0;i<7u;++i) {
        char ch=out[n-7u+i];
        if(ch>='A' && ch<='Z')ch=(char)(ch-'A'+'a');
        if(ch!=suffix[i])return 0;
    }
    return strcmp(out,"trackdisk.device")!=0;
}
static int storage_driver_name(const char *name) {
    static const char *const hints[]={"scsi","ide","ata","disk","uaehf"};
    size_t n;
    unsigned i;
    if(!name || !strcasecmp(name,"trackdisk.device"))return 0;
    n=strlen(name);
    if(n<8 || n>=DOLLY_PHYS_DRIVER_LEN || strcasecmp(name+n-7,".device"))
        return 0;
    /* Exec device nodes carry no generic block-device classification.
     * DOS-registered disk drivers are collected separately. */
    for(i=0;i<sizeof(hints)/sizeof(hints[0]);++i) {
        const char *p;
        size_t length=strlen(hints[i]);
        for(p=name;*p;++p)
            if(!strncasecmp(p,hints[i],length))return 1;
    }
    return 0;
}
static void add_driver(DollyPhysicalCatalog *out,const char *name) {
    unsigned i;
    for(i=0;i<out->count;++i)
        if(!strcasecmp(out->entries[i].driver,name))return;
    if(out->count==DOLLY_PHYS_MAX) {
        out->truncated=1;
        return;
    }
    strcpy(out->entries[out->count].driver,name);
    ++out->count;
    out->available=1;
}
static void add_exec_candidates(DollyPhysicalCatalog *out) {
    const struct Resident *resident;
    const struct Node *node;
    resident=FindResident((STRPTR)"scsi.device");
    if(resident && resident->rt_Type==NT_DEVICE)add_driver(out,"scsi.device");
    resident=FindResident((STRPTR)"uaehf.device");
    if(resident && resident->rt_Type==NT_DEVICE)add_driver(out,"uaehf.device");
    Forbid();
    for(node=SysBase->DeviceList.lh_Head;node->ln_Succ;node=node->ln_Succ)
        if(storage_driver_name(node->ln_Name))add_driver(out,node->ln_Name);
    Permit();
}

unsigned dolly_physical_probe_units(const char *driver,ULONG units[DOLLY_UNIT_PROBE_COUNT]) {
    struct MsgPort *port=CreateMsgPort();
    struct IORequest *io;
    ULONG unit;
    unsigned count=0;
    if(!port)return 0;
    for(unit=0;unit<DOLLY_UNIT_PROBE_COUNT;++unit) {
        io=CreateIORequest(port,sizeof(struct IOExtTD));
        if(!io)break;
        if(OpenDevice((STRPTR)driver,unit,io,0)==0) {
            CloseDevice(io);
            units[count++]=unit;
        }
        DeleteIORequest(io);
    }
    DeleteMsgPort(port);
    return count;
}
void dolly_physical_catalog_snapshot(DollyPhysicalCatalog *out) {
    const ULONG flags=LDF_DEVICES|LDF_READ;
    struct DosList *entry;
    if(!out)return;
    memset(out,0,sizeof(*out));
    if(!DOSBase)return;
    entry=LockDosList(flags);
    if(!entry) {
        add_exec_candidates(out);
        return;
    }
    out->available=1;
    while((entry=NextDosEntry(entry,LDF_DEVICES))!=NULL) {
        BPTR start=(BPTR)entry->dol_misc.dol_handler.dol_Startup;
        struct FileSysStartupMsg *f;
        struct DosEnvec *env;
        DollyPhysicalEntry found;
        unsigned i;
        memset(&found,0,sizeof(found));
        if(!bptr_valid(start,sizeof(*f)))continue;
        f=(struct FileSysStartupMsg *)BADDR(start);
        if(!bptr_valid(f->fssm_Environ,sizeof(ULONG)*11u))
            continue;
        env=(struct DosEnvec *)BADDR(f->fssm_Environ);
        if(env->de_TableSize<10ul || env->de_TableSize>256ul ||
           env->de_SizeBlock<64ul || env->de_SizeBlock>4096ul)
            continue;
        if(!driver_name(found.driver,f->fssm_Device))continue;
        found.unit=f->fssm_Unit;
        found.unit_registered=1;
        for(i=0;i<out->count;++i)
            if(out->entries[i].unit==found.unit &&
               strcmp(out->entries[i].driver,found.driver)==0)break;
        if(i<out->count)continue;
        if(out->count<DOLLY_PHYS_MAX)
            out->entries[out->count++]=found;
        else out->truncated=1;
    }
    UnLockDosList(flags);
    add_exec_candidates(out);
}
static int startup_extent(const struct DosList *entry,DollyPhysicalEntry *device,
                          uint64_t *start,uint64_t *length,ULONG *sector_bytes,
                          DollyIoHints *hints) {
    int ok=0;
    if(entry && bptr_valid(entry->dol_misc.dol_handler.dol_Startup,
                            sizeof(struct FileSysStartupMsg))) {
        struct FileSysStartupMsg *f=BADDR(entry->dol_misc.dol_handler.dol_Startup);
        if(bptr_valid(f->fssm_Environ,11u*sizeof(ULONG)) &&
           driver_name(device->driver,f->fssm_Device)) {
            const struct DosEnvec *e=BADDR(f->fssm_Environ);
            if(e->de_TableSize>=10u && !e->de_SecOrg && e->de_SectorPerBlock==1 &&
               e->de_SizeBlock>=128 && e->de_SizeBlock<=8192 &&
               e->de_Surfaces && e->de_BlocksPerTrack &&
               e->de_HighCyl>=e->de_LowCyl) {
                uint64_t sectors=(uint64_t)e->de_Surfaces*e->de_BlocksPerTrack;
                ULONG block=e->de_SizeBlock*4u;
                uint64_t limit=UINT64_MAX;
                if(sectors<=limit/block) {
                    uint64_t cylinder=sectors*block;
                    if((uint64_t)e->de_HighCyl+1u<=limit/cylinder) {
                        device->unit=f->fssm_Unit;
                        *sector_bytes=block;
                        *start=(uint64_t)e->de_LowCyl*cylinder;
                        *length=((uint64_t)e->de_HighCyl-e->de_LowCyl+1u)*cylinder;
                        if(hints && e->de_TableSize>=12u &&
                           bptr_valid(f->fssm_Environ,13u*sizeof(ULONG))) {
                            hints->present|=DOLLY_IO_BUF_MEM;
                            hints->buffer_memory=e->de_BufMemType;
                        }
                        if(hints && e->de_TableSize>=13u &&
                           bptr_valid(f->fssm_Environ,14u*sizeof(ULONG))) {
                            hints->present|=DOLLY_IO_MAX_TRANSFER;
                            hints->max_transfer=e->de_MaxTransfer;
                        }
                        if(hints && e->de_TableSize>=14u &&
                           bptr_valid(f->fssm_Environ,15u*sizeof(ULONG))) {
                            hints->present|=DOLLY_IO_MASK;
                            hints->mask=e->de_Mask;
                        }
                        ok=1;
                    }
                }
            }
        }
    }
    return ok;
}

int dolly_logical_extent(const char *name,DollyPhysicalEntry *device,
                         uint64_t *start,uint64_t *length,ULONG *sector_bytes,
                         DollyIoHints *hints) {
    const ULONG flags=LDF_DEVICES|LDF_READ;
    struct DosList *list,*entry;
    char dos_name[40];
    size_t n=strlen(name);
    int ok=0;
    if(n<2 || n>=sizeof(dos_name) || name[n-1]!=':')return 0;
    memcpy(dos_name,name,n-1);
    dos_name[n-1]='\0';
    list=LockDosList(flags);
    if(!list)return 0;
    entry=FindDosEntry(list,(STRPTR)dos_name,LDF_DEVICES);
    ok=startup_extent(entry,device,start,length,sector_bytes,hints);
    UnLockDosList(flags);
    return ok;
}

int dolly_physical_from_lock(BPTR lock,DollyPhysicalEntry *device,
                             uint64_t *start,uint64_t *length) {
    const ULONG flags=LDF_DEVICES|LDF_READ;
    const struct FileLock *file_lock;
    const struct DosList *ram;
    struct DosList *list,*entry;
    int found=0;
    *start=*length=0;
    if(!bptr_valid(lock,sizeof(*file_lock)))return 0;
    file_lock=BADDR(lock);
    if(!file_lock->fl_Task)return 0;
    list=LockDosList(flags);
    if(!list)return 0;
    ram=FindDosEntry(list,(STRPTR)"RAM",LDF_DEVICES);
    entry=list;
    while((entry=NextDosEntry(entry,LDF_DEVICES))!=NULL) {
        const struct FileSysStartupMsg *startup;
        BPTR b;
        ULONG sector_bytes;
        if(entry->dol_Task!=file_lock->fl_Task)continue;
        if(found) {
            found=0;
            break;
        }
        if(entry==ram) {
            found=2;
            continue;
        }
        b=entry->dol_misc.dol_handler.dol_Startup;
        if(!bptr_valid(b,sizeof(*startup)))break;
        startup=BADDR(b);
        if(!driver_name(device->driver,startup->fssm_Device))break;
        device->unit=startup->fssm_Unit;
        (void)startup_extent(entry,device,start,length,&sector_bytes,NULL);
        found=1;
    }
    UnLockDosList(flags);
    return found;
}

int dolly_amiga_unit_registered(const char *driver,ULONG unit) {
    const ULONG flags=LDF_DEVICES|LDF_READ;
    struct DosList *entry=LockDosList(flags);
    int found=0;
    if(!entry)return -1;
    while((entry=NextDosEntry(entry,LDF_DEVICES))!=NULL) {
        const struct FileSysStartupMsg *startup;
        char name[DOLLY_PHYS_DRIVER_LEN];
        BPTR b=entry->dol_misc.dol_handler.dol_Startup;
        if(!bptr_valid(b,sizeof(*startup)))continue;
        startup=BADDR(b);
        if(startup->fssm_Unit==unit &&
           driver_name(name,startup->fssm_Device) && !strcasecmp(name,driver)) {
            found=1;
            break;
        }
    }
    UnLockDosList(flags);
    return found;
}
