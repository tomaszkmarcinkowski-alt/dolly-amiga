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

static int mount_name(char out[DOLLY_MOUNT_NAME],BPTR name) {
    const UBYTE *str;
    unsigned n;
    if(!bptr_valid(name,1))return 0;
    str=BADDR(name);
    n=str[0];
    if(!n || n>DOLLY_MOUNT_NAME-2 || !bptr_valid(name,n+1))return 0;
    if(str[n]==0)--n;
    if(!n || memchr(str+1,0,n))return 0;
    memcpy(out,str+1,n);
    out[n]=':';
    out[n+1]=0;
    return 1;
}
int dolly_amiga_mounted_extents(const DollyPhysicalEntry *device,uint64_t start,
                               uint64_t bytes,int include_inactive,DollyMountList *out) {
    const ULONG flags=LDF_DEVICES|LDF_VOLUMES|LDF_READ;
    struct DosList *list=LockDosList(flags),*entry=list;
    int ok=1;
    out->count=0;
    if(!list)return 0;
    while((entry=NextDosEntry(entry,LDF_DEVICES))!=NULL) {
        const struct FileSysStartupMsg *startup;
        BPTR b=entry->dol_misc.dol_handler.dol_Startup;
        DollyPhysicalEntry found;
        uint64_t first,length;
        ULONG sector;
        int extent_valid;
        struct DosList *volume=list;
        DollyMountedVolume *mount;
        if((!include_inactive && !entry->dol_Task) || !bptr_valid(b,sizeof(*startup)))continue;
        startup=BADDR(b);
        if(startup->fssm_Unit!=device->unit || !driver_name(found.driver,startup->fssm_Device) ||
           strcasecmp(found.driver,device->driver))continue;
        extent_valid=startup_extent(entry,&found,&first,&length,&sector,NULL);
        if(extent_valid && (first>=start+bytes || start>=first+length))
            continue;
        if(out->count==DOLLY_MOUNT_MAX){ok=0;break;}
        mount=&out->entries[out->count];
        memset(mount,0,sizeof(*mount));
        if(!mount_name(mount->device,entry->dol_Name)){ok=0;break;}
        mount->entry=entry;
        mount->handler=entry->dol_Task;
        if(extent_valid) {
            mount->start=first;
            mount->length=length;
            mount->block_bytes=sector;
            if(bptr_valid(startup->fssm_Environ,17u*sizeof(ULONG))) {
                const struct DosEnvec *env=BADDR(startup->fssm_Environ);
                if(env->de_TableSize>=16u)mount->dos_type=env->de_DosType;
            }
        }
        while((volume=NextDosEntry(volume,LDF_VOLUMES))!=NULL) {
            if(!mount->handler || volume->dol_Task!=mount->handler)continue;
            if(!mount_name(mount->volume,volume->dol_Name))ok=0;
            break;
        }
        if(!ok)break;
        ++out->count;
    }
    UnLockDosList(flags);
    return ok;
}

int dolly_amiga_inhibit_mount(const DollyMountedVolume *mount,int suspend) {
    const ULONG flags=LDF_DEVICES|LDF_READ;
    struct DosList *list,*entry;
    char name[DOLLY_MOUNT_NAME];
    size_t length=strlen(mount->device);
    int same;
    if(length<2 || length>=sizeof(name) || mount->device[length-1]!=':')return 0;
    memcpy(name,mount->device,length-1);
    name[length-1]=0;
    list=LockDosList(flags);
    if(!list)return 0;
    entry=FindDosEntry(list,(STRPTR)name,LDF_DEVICES);
    same=entry && entry==mount->entry && entry->dol_Task==mount->handler;
    UnLockDosList(flags);
    /* DOS may send a packet and wait for the handler. Do not hold the list lock.
     * Un-inhibiting validates the disk again. Route failures back to Dolly
     * instead of opening an AmigaDOS requester during cleanup. */
    if(!same)return 0;
    if(suspend)return Inhibit((STRPTR)mount->device,DOSTRUE)!=0;
    {
        struct Process *process=(struct Process *)FindTask(NULL);
        APTR old_window=process->pr_WindowPtr;
        int resumed;
        process->pr_WindowPtr=(APTR)-1;
        resumed=Inhibit((STRPTR)mount->device,DOSFALSE)!=0;
        process->pr_WindowPtr=old_window;
        return resumed;
    }
}

const char *dolly_amiga_unmount_checked(const DollyMountedVolume *mount,LONG *dos_error) {
    const ULONG read_flags=LDF_DEVICES|LDF_READ;
    const ULONG write_flags=LDF_DEVICES|LDF_VOLUMES|LDF_WRITE;
    struct DosList *list,*entry;
    char name[DOLLY_MOUNT_NAME];
    size_t length=strlen(mount->device);
    *dos_error=0;
    if(length<2 || length>=sizeof(name) || mount->device[length-1]!=':')
        return "The DOS device name is invalid.";
    memcpy(name,mount->device,length-1);
    name[length-1]=0;
    list=LockDosList(read_flags);
    if(!list)return "Cannot inspect the DOS device list.";
    entry=FindDosEntry(list,(STRPTR)name,LDF_DEVICES);
    int same=entry && entry==mount->entry && entry->dol_Task==mount->handler;
    UnLockDosList(read_flags);
    if(!same)return "The DOS device changed before shutdown.";

    if(mount->handler && !DoPkt(mount->handler,ACTION_DIE,0,0,0,0,0)) {
        *dos_error=IoErr();
        return "The filesystem handler refused shutdown.";
    }

    /* A handler can acknowledge shutdown before removing its volume.
     * Wait briefly for that state change without reusing its port. */
    const char *problem="The DOS device list is busy.";
    for(unsigned attempt=0;attempt<21u;++attempt) {
        list=AttemptLockDosList(write_flags);
        if(list) {
            struct DosList *volume=list;
            int volume_active=0;
            int removed=0;
            while(mount->handler && (volume=NextDosEntry(volume,LDF_VOLUMES))!=NULL)
                if(volume->dol_Task==mount->handler) {
                    volume_active=1;
                    break;
                }
            entry=FindDosEntry(list,(STRPTR)name,LDF_DEVICES);
            if(entry && (entry!=mount->entry ||
                         (entry->dol_Task && entry->dol_Task!=mount->handler))) {
                UnLockDosList(write_flags);
                return "A different DOS device appeared during shutdown.";
            }
            if(!volume_active)removed=!entry || RemDosEntry(entry)!=0;
            UnLockDosList(write_flags);
            if(removed)return NULL;
            problem=volume_active ? "The DOS volume still exists after shutdown." :
                                    "DOS could not remove the device entry.";
        }
        if(attempt<20u)Delay(5);
    }
    return problem;
}

int dolly_amiga_unmount(const DollyMountedVolume *mount) {
    LONG dos_error;
    return dolly_amiga_unmount_checked(mount,&dos_error)==NULL;
}

int dolly_amiga_lock_uses_mount(BPTR lock,const DollyMountedVolume *mount) {
    const struct FileLock *opened;
    if(!mount->handler || !bptr_valid(lock,sizeof(*opened)))return 0;
    opened=BADDR(lock);
    return opened->fl_Task==mount->handler;
}

int dolly_amiga_file_uses_mount(BPTR file,const DollyMountedVolume *mount) {
    const struct FileHandle *opened;
    if(!mount->handler || !bptr_valid(file,sizeof(*opened)))return 0;
    opened=BADDR(file);
    /* AmigaDOS stores the file's handler port in fh_Type. */
    return opened->fh_Type==mount->handler;
}
