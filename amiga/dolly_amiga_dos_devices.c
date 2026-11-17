#include "dolly_amiga_dos_devices.h"
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <string.h>
extern struct DosLibrary *DOSBase;
/* AmigaDOS has no generic DLT_DEVICE flag guaranteeing block media.
 * Exclude known non-block handlers; this is a conservative filter,
 * not proof that every remaining entry is a disk.
 */
static int excluded_handler(const UBYTE *bstr,unsigned n) {
    static const char *const excluded[]={
        "RAW","CON","RAM","SER","PAR","PRT",NULL
    };
    unsigned j,k;
    for(j=0;excluded[j];++j) {
        const char *name=excluded[j];
        for(k=0;k<n && name[k];++k) {
            UBYTE ch=bstr[k+1u];
            if(ch>='a' && ch<='z')ch=(UBYTE)(ch-'a'+'A');
            if(ch!=(UBYTE)name[k])break;
        }
        if(k==n && name[k]=='\0')return 1;
    }
    return 0;
}
void dolly_amiga_discover_dos_devices(DollyDosDevices *out) {
    struct DosList *entry;
    const ULONG flags=LDF_DEVICES|LDF_READ;
    if(!out)return;
    memset(out,0,sizeof(*out));
    if(!DOSBase)return;
    entry=LockDosList(flags);
    if(!entry)return;
    out->available=1;
    while((entry=NextDosEntry(entry,LDF_DEVICES))!=NULL) {
        const UBYTE *bstr;
        unsigned n,i;
        char *dest;
        if(!entry->dol_Name)continue;
        bstr=(const UBYTE *)BADDR(entry->dol_Name);
        if(!bstr)continue;
        n=(unsigned)bstr[0];
        if(!n)continue;
        if(excluded_handler(bstr,n))continue;
        if(out->count>=DOLLY_DOS_MAX_DEVICES) {
            out->truncated=1;
            continue;
        }
        /* Never make an incorrect device alias by truncating its
         * identity. Skip entries whose full NAME: cannot fit.
         */
        if(n>DOLLY_DOS_NAME_BYTES-2u) {
            out->truncated=1;
            continue;
        }
        dest=out->names[out->count];
        for(i=0;i<n;++i)dest[i]=(char)bstr[i+1u];
        dest[n++]=':';
        dest[n]='\0';
        ++out->count;
    }
    UnLockDosList(flags);
}
