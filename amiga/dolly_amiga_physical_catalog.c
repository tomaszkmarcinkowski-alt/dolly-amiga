#include "dolly_amiga_physical_catalog.h"
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <string.h>
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
void dolly_physical_catalog_snapshot(DollyPhysicalCatalog *out) {
    const ULONG flags=LDF_DEVICES|LDF_READ;
    struct DosList *entry;
    if(!out)return;
    memset(out,0,sizeof(*out));
    if(!DOSBase)return;
    entry=LockDosList(flags);
    if(!entry)return;
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
        for(i=0;i<out->count;++i)
            if(out->entries[i].unit==found.unit &&
               strcmp(out->entries[i].driver,found.driver)==0)break;
        if(i<out->count)continue;
        if(out->count<DOLLY_PHYS_MAX)
            out->entries[out->count++]=found;
        else out->truncated=1;
    }
    UnLockDosList(flags);
}
