/* Dolly M1.23: diagnostic-only bridge from registered AmigaDOS
 * filesystem startup records to physical driver/unit pairs.
 *
 * Run without arguments to enumerate. Use "--probe" explicitly for
 * TD_GETGEOMETRY and SCSI INQUIRY (both read-only queries).
 *
 * Not part of the GUI yet. No disk sectors read or written.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <devices/trackdisk.h>
#include <devices/scsidisk.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

extern struct DosLibrary *DOSBase;
#define MAX_CANDIDATES 16u
#define DRIVER_LEN 60u
typedef struct ProbeCandidate {
    char driver[DRIVER_LEN];
    ULONG unit;
    char volume[40];
} ProbeCandidate;
static ProbeCandidate candidates[MAX_CANDIDATES];
static unsigned count,skipped;
#define MAX_INSPECT 48u
typedef enum ExtractReason {
    EXTRACT_OK=0,
    EXTRACT_NO_STARTUP,
    EXTRACT_BAD_STARTUP,
    EXTRACT_BAD_ENV,
    EXTRACT_BAD_GEOMETRY,
    EXTRACT_BAD_DRIVER_BSTR,
    EXTRACT_NOT_DEVICE_DRIVER,
    EXTRACT_BAD_DOS_NAME,
    EXTRACT_DUPLICATE,
    EXTRACT_DEFER_TRACKDISK,
    EXTRACT_DEFER_CARDDISK,
    EXTRACT_LIMIT,
    EXTRACT_REASON_COUNT
} ExtractReason;
typedef struct InspectRecord {
    char dos_name[40];
    ExtractReason reason;
    ULONG startup;
    ULONG env_sizeblock;
    ULONG env_tablesize;
    ULONG env_surfaces;
    ULONG env_blocks_per_track;
    ULONG unit;
    ULONG driver_bptr,driver_addr,driver_memtype,driver_length;
    UBYTE driver_sample[8];
    unsigned driver_sample_count;
    char driver[DRIVER_LEN];
} InspectRecord;
static InspectRecord inspect_records[MAX_INSPECT];
static unsigned inspect_count,reason_counts[EXTRACT_REASON_COUNT];
static const char *const extract_reason_names[]={
    "accepted","no startup","startup pointer invalid",
    "environment pointer invalid","environment parameters rejected",
    "driver name pointer/bytes invalid","not a .device driver",
    "DOS name invalid","duplicate physical unit",
    "excluded: floppy (filesystem repair only)",
    "deferred: PCMCIA SRAM carddisk",
    "candidate limit"
};


/* Inspect DOS startup data as untrusted, driver-specific input.
 * A valid address alone is NOT enough. All expected fields, bounds and
 * the DosEnvec signature must be plausible before treating it as an FSSM.
 */
static int valid_region(const void *p,ULONG bytes) {
    const UBYTE *b=(const UBYTE *)p;
    return p && bytes &&
           TypeOfMem((APTR)b)!=0 &&
           TypeOfMem((APTR)(b+bytes-1u))!=0;
}
static int valid_bptr(BPTR b,ULONG len) {
    ULONG raw=(ULONG)b;
    if(!raw || (raw&0xc0000000ul))return 0;
    return valid_region(BADDR(b),len);
}
static int bstr_copy(char *dest,ULONG cap,BPTR b) {
    const UBYTE *bytes;
    ULONG i,n;
    if(!valid_bptr(b,1))return 0;
    bytes=(const UBYTE *)BADDR(b);
    n=bytes[0];
    if(!n || !valid_bptr(b,n+1u))return 0;
    /* Some filesystem startup records store a terminating NUL
     * within the length-prefixed name. The real text is then one
     * byte shorter than the BSTR length. Accept only a final NUL,
     * never an embedded NUL or other control character.
     */
    if(bytes[n]==0u)--n;
    if(!n || n>=cap)return 0;
    for(i=0;i<n;++i) {
        UBYTE ch=bytes[i+1u];
        if(ch<33u || ch>126u)return 0;
        dest[i]=(char)ch;
    }
    dest[n]='\0';
    return 1;
}
/* Record only bounded, independently validated bytes from the BSTR
 * address. We do not dereference a candidate that fails TypeOfMem.
 * This distinguishes a bad BPTR from a long/non-ASCII BSTR.
 */
static void inspect_driver_bstr(InspectRecord *record,BPTR b) {
    const UBYTE *bytes;
    unsigned i,n;
    record->driver_bptr=(ULONG)b;
    if(!b || ((ULONG)b&0xc0000000ul))return;
    bytes=(const UBYTE *)BADDR(b);
    record->driver_addr=(ULONG)bytes;
    record->driver_memtype=(ULONG)TypeOfMem((APTR)bytes);
    if(!record->driver_memtype)return;
    n=(unsigned)bytes[0];
    record->driver_length=n;
    if(!n || !valid_bptr(b,n+1u))return;
    if(n>8u)n=8u;
    for(i=0;i<n;++i)
        record->driver_sample[i]=bytes[i+1u];
    record->driver_sample_count=n;
}
static int driver_suffix(const char *name) {
    ULONG n=(ULONG)strlen(name),i;
    static const char suffix[]=".device";
    if(n<7u)return 0;
    for(i=0;i<7u;++i) {
        char a=name[n-7u+i],b=suffix[i];
        if(a>='A' && a<='Z')a=(char)(a-'A'+'a');
        if(a!=b)return 0;
    }
    return 1;
}
static ExtractReason extract(const struct DosList *entry,
                             ProbeCandidate *out,InspectRecord *record) {
    BPTR startup;
    const struct FileSysStartupMsg *f;
    const struct DosEnvec *env;
    if(!entry)return EXTRACT_NO_STARTUP;
    startup=(BPTR)entry->dol_misc.dol_handler.dol_Startup;
    record->startup=(ULONG)startup;
    if(!startup)return EXTRACT_NO_STARTUP;
    if(!valid_bptr(startup,sizeof(*f)))return EXTRACT_BAD_STARTUP;
    f=(const struct FileSysStartupMsg *)BADDR(startup);
    if(!valid_bptr(f->fssm_Environ,sizeof(ULONG)*11u))
        return EXTRACT_BAD_ENV;
    env=(const struct DosEnvec *)BADDR(f->fssm_Environ);
    record->env_tablesize=env->de_TableSize;
    record->env_sizeblock=env->de_SizeBlock;
    record->env_surfaces=env->de_Surfaces;
    record->env_blocks_per_track=env->de_BlocksPerTrack;
    record->unit=f->fssm_Unit;
    /* These are filesystem/partition geometry hints, not driver
     * identity. Some virtual controllers provide zero or synthetic
     * heads/sectors-per-track. Never reject the underlying device
     * solely because these optional geometry hints are unusual.
     */
    if(env->de_TableSize<10ul || env->de_TableSize>256ul ||
       env->de_SizeBlock<64ul || env->de_SizeBlock>4096ul)
        return EXTRACT_BAD_GEOMETRY;
    inspect_driver_bstr(record,f->fssm_Device);
    if(!bstr_copy(out->driver,sizeof(out->driver),f->fssm_Device))
        return EXTRACT_BAD_DRIVER_BSTR;
    strcpy(record->driver,out->driver);
    if(!driver_suffix(out->driver))return EXTRACT_NOT_DEVICE_DRIVER;
    if(!bstr_copy(out->volume,sizeof(out->volume),entry->dol_Name))
        return EXTRACT_BAD_DOS_NAME;
    out->unit=f->fssm_Unit;
    return EXTRACT_OK;
}
/* Keep trackdisk.device out of generic disk imaging/recovery
 * selection: floppy handling belongs to the filesystem repair
 * module. Defer carddisk.device until real PCMCIA SRAM tests.
 * These names are never passed to OpenDevice by this diagnostic.
 */
static ExtractReason excluded_physical_driver(const char *driver) {
    if(strcmp(driver,"trackdisk.device")==0)
        return EXTRACT_DEFER_TRACKDISK;
    if(strcmp(driver,"carddisk.device")==0)
        return EXTRACT_DEFER_CARDDISK;
    return EXTRACT_OK;
}
static void snapshot(void) {
    const ULONG flags=LDF_DEVICES|LDF_READ;
    struct DosList *e;
    if(!DOSBase)return;
    e=LockDosList(flags);
    if(!e)return;
    while((e=NextDosEntry(e,LDF_DEVICES))!=NULL) {
        ProbeCandidate candidate;
        InspectRecord rec;
        ExtractReason reason;
        unsigned i;
        memset(&candidate,0,sizeof(candidate));
        memset(&rec,0,sizeof(rec));
        strcpy(rec.dos_name,"<invalid>");
        (void)bstr_copy(rec.dos_name,sizeof(rec.dos_name),e->dol_Name);
        reason=extract(e,&candidate,&rec);
        if(reason==EXTRACT_OK)
            reason=excluded_physical_driver(candidate.driver);
        if(reason==EXTRACT_OK) {
            for(i=0;i<count;++i)
                if(candidates[i].unit==candidate.unit &&
                   strcmp(candidates[i].driver,candidate.driver)==0)break;
            if(i<count)reason=EXTRACT_DUPLICATE;
            else if(count<MAX_CANDIDATES)candidates[count++]=candidate;
            else reason=EXTRACT_LIMIT;
        }
        rec.reason=reason;
        if((unsigned)reason<EXTRACT_REASON_COUNT)
            ++reason_counts[reason];
        if(reason!=EXTRACT_OK && reason!=EXTRACT_DUPLICATE)
            ++skipped;
        if(inspect_count<MAX_INSPECT)
            inspect_records[inspect_count++]=rec;
    }
    UnLockDosList(flags);
}
static void report_inspection(void) {
    unsigned i;
    puts("DOS device entry diagnostics (no physical I/O):");
    for(i=0;i<inspect_count;++i) {
        const InspectRecord *r=&inspect_records[i];
        printf("  %s: %s",r->dos_name,
               extract_reason_names[r->reason]);
        if(r->reason==EXTRACT_BAD_DRIVER_BSTR) {
            unsigned j;
            printf(" [bptr=%08lx addr=%08lx mem=%08lx len=%lu",
                   (unsigned long)r->driver_bptr,
                   (unsigned long)r->driver_addr,
                   (unsigned long)r->driver_memtype,
                   (unsigned long)r->driver_length);
            if(r->driver_sample_count) {
                printf(" bytes=");
                for(j=0;j<r->driver_sample_count;++j)
                    printf("%02lx",(unsigned long)r->driver_sample[j]);
            }
            printf("]");
        }
        if(r->env_tablesize || r->env_sizeblock) {
            printf(" (table=%lu blockwords=%lu heads=%lu spt=%lu",
                   (unsigned long)r->env_tablesize,
                   (unsigned long)r->env_sizeblock,
                   (unsigned long)r->env_surfaces,
                   (unsigned long)r->env_blocks_per_track);
            if(r->driver[0])
                printf(" drv=%s unit=%lu",
                       r->driver,(unsigned long)r->unit);
            printf(")");
        }
        printf("\n");
    }
    if(reason_counts[EXTRACT_BAD_STARTUP] ||
       reason_counts[EXTRACT_BAD_ENV])
        puts("Note: DOS startup values are handler-specific; invalid "
             "pointers are not dereferenced.");
}
static void sanitize(char *dest,ULONG cap,const UBYTE *src,ULONG len) {
    ULONG i;
    if(!cap)return;
    if(len>=cap)len=cap-1u;
    for(i=0;i<len;++i) {
        UBYTE c=src[i];
        dest[i]=(c>=32u && c<=126u)?(char)c:'?';
    }
    while(len && dest[len-1u]==' ')--len;
    dest[len]='\0';
}
/* Model and capacity are independent fields. A driver can support
 * TD_GETGEOMETRY but not HD_SCSICMD, or the other way round.
 * Keep status explicit and never derive bytes from SCSI INQUIRY.
 */
static void probe(const ProbeCandidate *c) {
    struct MsgPort *port=NULL;
    struct IOStdReq *io=NULL;
    struct DriveGeometry geom;
    struct SCSICmd sc;
    UBYTE *data=NULL;
    char name[64],size[40];
    LONG status;
    int opened=0;
    strcpy(name,"Unavailable");
    strcpy(size,"Unavailable");
    port=CreateMsgPort();
    if(!port)goto done;
    io=(struct IOStdReq *)CreateIORequest(port,sizeof(*io));
    if(!io)goto done;
    status=OpenDevice((STRPTR)c->driver,c->unit,
                      (struct IORequest *)io,0);
    if(status!=0) {
        printf("  OpenDevice failed: %ld\n",status);
        goto done;
    }
    opened=1;
    memset(&geom,0,sizeof(geom));
    io->io_Command=TD_GETGEOMETRY;
    io->io_Data=&geom;
    io->io_Length=sizeof(geom);
    io->io_Offset=0;
    if(DoIO((struct IORequest *)io)==0 &&
       geom.dg_SectorSize && geom.dg_TotalSectors) {
        uint64_t bytes=(uint64_t)geom.dg_TotalSectors*
                       (uint64_t)geom.dg_SectorSize;
        unsigned long mib=(unsigned long)(bytes/1048576u);
        sprintf(size,"%lu MiB",mib);
    }
    /* Both the SCSI CDB and data buffer are in Chip RAM for old DMA
     * controllers. No physical sectors are read or changed.
     */
    data=(UBYTE *)AllocMem(64,MEMF_CHIP|MEMF_CLEAR);
    if(data) {
        char vendor[9],product[17];
        memset(&sc,0,sizeof(sc));
        data[40]=0x12u; /* SCSI INQUIRY */
        data[44]=36u;
        sc.scsi_Data=(UWORD *)data;
        sc.scsi_Length=36;
        sc.scsi_Command=data+40;
        sc.scsi_CmdLength=6;
        sc.scsi_Flags=SCSIF_READ;
        io->io_Command=HD_SCSICMD;
        io->io_Data=&sc;
        io->io_Length=sizeof(sc);
        io->io_Offset=0;
        if(DoIO((struct IORequest *)io)==0 &&
           sc.scsi_Status==0 && sc.scsi_Actual>=36) {
            sanitize(vendor,sizeof(vendor),data+8,8);
            sanitize(product,sizeof(product),data+16,16);
            if(vendor[0] && product[0])
                sprintf(name,"%s %s",vendor,product);
            else if(product[0])
                strcpy(name,product);
            else if(vendor[0])
                strcpy(name,vendor);
        }
    }
done:
    if(opened)CloseDevice((struct IORequest *)io);
    if(data)FreeMem(data,64);
    if(io)DeleteIORequest((struct IORequest *)io);
    if(port)DeleteMsgPort(port);
    printf("  Device name: %s\n",name);
    printf("  Size: %s\n",size);
}
int main(int argc,char **argv) {
    unsigned i,selected=MAX_CANDIDATES;
    int perform=argc>1 && strcmp(argv[1],"--probe")==0;
    if(perform) {
        const char *arg=argc>2 ? argv[2] : "";
        unsigned n=0;
        if(!*arg) {
            puts("Usage: dolly-physical-probe --probe INDEX");
            return 5;
        }
        while(*arg>='0' && *arg<='9') {
            n=n*10u+(unsigned)(*arg-'0');
            if(n>=MAX_CANDIDATES)break;
            ++arg;
        }
        if(*arg!='\0' || n>=MAX_CANDIDATES) {
            puts("Invalid candidate index.");
            return 5;
        }
        selected=n;
    }
    puts("Dolly M1.23: read-only AmigaDOS physical candidates");
    snapshot();
    printf("Candidates: %u (skipped/unknown: %u)\n",count,skipped);
    if(!count || (argc>1 && strcmp(argv[1],"--inspect")==0))
        report_inspection();
    for(i=0;i<count;++i) {
        printf("%u: %s unit %lu (from %s:)\n",i,
               candidates[i].driver,(unsigned long)candidates[i].unit,
               candidates[i].volume);
        if(perform && selected==i)probe(&candidates[i]);
    }
    if(!perform)
        puts("No device opened. Use --probe INDEX for read-only queries.");
    else if(selected>=count)
        puts("Selected candidate not found; no device opened.");
    return 0;
}
