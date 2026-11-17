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
    if(!n || n>=cap || !valid_bptr(b,n+1u))return 0;
    for(i=0;i<n;++i) {
        UBYTE ch=bytes[i+1u];
        if(ch<33u || ch>126u)return 0;
        dest[i]=(char)ch;
    }
    dest[n]='\0';
    return 1;
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
static int extract(const struct DosList *entry,ProbeCandidate *out) {
    BPTR startup;
    const struct FileSysStartupMsg *f;
    const struct DosEnvec *env;
    if(!entry)return 0;
    startup=(BPTR)entry->dol_Startup;
    if(!valid_bptr(startup,sizeof(*f)))return 0;
    f=(const struct FileSysStartupMsg *)BADDR(startup);
    if(!valid_bptr(f->fssm_Environ,sizeof(ULONG)*11u))return 0;
    env=(const struct DosEnvec *)BADDR(f->fssm_Environ);
    if(env->de_TableSize<10ul || env->de_TableSize>256ul ||
       env->de_SizeBlock<64ul || env->de_SizeBlock>4096ul ||
       !env->de_Surfaces || env->de_Surfaces>256ul ||
       !env->de_BlocksPerTrack || env->de_BlocksPerTrack>1024ul)
        return 0;
    if(!bstr_copy(out->driver,sizeof(out->driver),f->fssm_Device) ||
       !driver_suffix(out->driver))
        return 0;
    if(!bstr_copy(out->volume,sizeof(out->volume),entry->dol_Name))
        return 0;
    out->unit=f->fssm_Unit;
    return 1;
}
static void snapshot(void) {
    const ULONG flags=LDF_DEVICES|LDF_READ;
    struct DosList *e;
    if(!DOSBase)return;
    e=LockDosList(flags);
    if(!e)return;
    while((e=NextDosEntry(e,LDF_DEVICES))!=NULL) {
        ProbeCandidate candidate;
        unsigned i;
        memset(&candidate,0,sizeof(candidate));
        if(!extract(e,&candidate)) {
            ++skipped;
            continue;
        }
        for(i=0;i<count;++i)
            if(candidates[i].unit==candidate.unit &&
               strcmp(candidates[i].driver,candidate.driver)==0)break;
        if(i<count)continue; /* multiple DOS partitions, one drive */
        if(count<MAX_CANDIDATES)candidates[count++]=candidate;
        else ++skipped;
    }
    UnLockDosList(flags);
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
