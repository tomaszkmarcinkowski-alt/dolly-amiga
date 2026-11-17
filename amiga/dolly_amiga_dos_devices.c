#include "dolly_amiga_dos_devices.h"
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <string.h>
extern struct DosLibrary *DOSBase;
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
        if(out->count>=DOLLY_DOS_MAX_DEVICES) {
            out->truncated=1;
            continue;
        }
        dest=out->names[out->count];
        if(n>DOLLY_DOS_NAME_BYTES-2u)n=DOLLY_DOS_NAME_BYTES-2u;
        for(i=0;i<n;++i)dest[i]=(char)bstr[i+1u];
        dest[n++]=':';
        dest[n]='\0';
        ++out->count;
    }
    UnLockDosList(flags);
}
