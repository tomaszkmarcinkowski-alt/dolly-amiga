#include "dolly_amiga_fonts.h"
#include "dolly_amiga_io_settings.h"
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <workbench/startup.h>
#include <string.h>
#include <strings.h>

struct Library *IconBase;

#define PREFS_VERSION 7u
#define PREFS_MOUNT_DESTINATION_DEFAULT 32u
#define PREFS_FONT_PAYLOAD (DOLLY_FONT_NAME_BYTES+8u)
#define PREFS_FONT_CHUNK (8u+PREFS_FONT_PAYLOAD)
#define PREFS_ALLOW_OUTSIDE_RANGE 16u
#define PREFS_RETIRED_RESTORE_MISMATCH 8u /* Accepted for existing prefs. */
#define PREFS_LEGACY_LIMIT 1u
#define PREFS_VERIFY_WRITES 2u
#define PREFS_SUPPRESS_SAME_UNIT_WARNING 4u
#define PREFS_DEFAULT "ENVARC:dolly.prefs"
#define PREFS_RECORD (DOLLY_PHYS_DRIVER_LEN+20u)
static char prefs_path[256]=PREFS_DEFAULT;

static uint32_t get32(const unsigned char *p) {
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 |
           (uint32_t)p[2]<<8 | p[3];
}
static void put32(unsigned char *p,uint32_t value) {
    p[0]=(unsigned char)(value>>24);p[1]=(unsigned char)(value>>16);
    p[2]=(unsigned char)(value>>8);p[3]=(unsigned char)value;
}
static int set_path(const char *path) {
    size_t n=strlen(path);
    if(n>=2 && path[0]=='"' && path[n-1]=='"') {
        ++path;
        n-=2;
    }
    if(!n || n>=sizeof(prefs_path))return 0;
    memcpy(prefs_path,path,n);
    prefs_path[n]=0;
    return 1;
}
static int select_path(int argc,char **argv) {
    int i;
    if(argc==0 && argv) {
        const struct WBStartup *wb=(const struct WBStartup *)argv;
        if(wb->sm_NumArgs>0 && wb->sm_ArgList[0].wa_Name) {
            struct DiskObject *icon;
            BPTR previous=CurrentDir(wb->sm_ArgList[0].wa_Lock);
            int valid=1;
            IconBase=OpenLibrary("icon.library",36);
            if(IconBase) {
                icon=GetDiskObject(wb->sm_ArgList[0].wa_Name);
                if(icon) {
                    STRPTR path=icon->do_ToolTypes?FindToolType(icon->do_ToolTypes,"PREFS"):NULL;
                    if(path) {
                        char absolute[sizeof(prefs_path)];
                        valid=set_path((const char *)path);
                        if(valid && !strchr(prefs_path,':')) {
                            valid=NameFromLock(wb->sm_ArgList[0].wa_Lock,absolute,
                                               sizeof(absolute)) &&
                                  AddPart(absolute,prefs_path,sizeof(absolute));
                            if(valid)set_path(absolute);
                        }
                    }
                    FreeDiskObject(icon);
                }
                CloseLibrary(IconBase);
            }
            CurrentDir(previous);
            return valid;
        }
    }
    for(i=1;i<argc;++i) {
        if(strncasecmp(argv[i],"PREFS=",6)==0) {
            if(!set_path(argv[i]+6))return 0;
        } else if(strcasecmp(argv[i],"PREFS")==0) {
            if(++i>=argc || !set_path(argv[i]))return 0;
        }
    }
    return 1;
}

const char *dolly_amiga_prefs_load(int argc,char **argv) {
    unsigned char header[42],record[PREFS_RECORD],font[PREFS_FONT_CHUNK],extra;
    uint32_t bytes,flags;
    BPTR f;
    const char *error="Dolly could not read the preferences. Defaults will be used.";
    if(!select_path(argc,argv)) {
        set_path(PREFS_DEFAULT);
        return "The PREFS path is invalid. Defaults will be used.";
    }
    f=Open(prefs_path,MODE_OLDFILE);
    if(!f)return IoErr()==ERROR_OBJECT_NOT_FOUND?NULL:error;
    if(Read(f,header,26)!=26 || memcmp(header,"FORM",4) ||
       memcmp(header+8,"PREFPRHD",8) || get32(header+16)!=6)goto finish;
    if(header[20]!=PREFS_VERSION) {
        error="The preferences file version is incompatible.\nDefaults will be used.";
        goto finish;
    }
    if(Read(f,header+26,16)!=16 || memcmp(header+26,"DOIO",4) ||
       (bytes=get32(header+30))<8 || bytes>8u+PREFS_RECORD*DOLLY_PREFS_MAX ||
       (bytes-8)%PREFS_RECORD || get32(header+4)!=26u+bytes+PREFS_FONT_CHUNK ||
       get32(header+38)!=(bytes-8)/PREFS_RECORD)goto finish;
    flags=get32(header+34);
    if(flags&~(PREFS_LEGACY_LIMIT|PREFS_VERIFY_WRITES|PREFS_SUPPRESS_SAME_UNIT_WARNING|PREFS_RETIRED_RESTORE_MISMATCH|PREFS_ALLOW_OUTSIDE_RANGE|PREFS_MOUNT_DESTINATION_DEFAULT))goto finish;
    while(bytes>8) {
        DollyPhysicalEntry device;
        DollyIoHints hints;
        uint32_t present;
        if(Read(f,record,sizeof(record))!=(LONG)sizeof(record) ||
           !record[0] || !memchr(record,0,DOLLY_PHYS_DRIVER_LEN))goto finish;
        memcpy(device.driver,record,DOLLY_PHYS_DRIVER_LEN);
        device.unit=get32(record+DOLLY_PHYS_DRIVER_LEN);
        present=get32(record+DOLLY_PHYS_DRIVER_LEN+4);
        if(!present || present>DOLLY_IO_ALL)goto finish;
        hints.present=(unsigned)present;
        hints.mask=get32(record+DOLLY_PHYS_DRIVER_LEN+8);
        hints.buffer_memory=get32(record+DOLLY_PHYS_DRIVER_LEN+12);
        hints.max_transfer=get32(record+DOLLY_PHYS_DRIVER_LEN+16);
        if(!dolly_io_hints_valid(hints) ||
           dolly_amiga_io_override(&device).present ||
           !dolly_amiga_io_store(&device,hints))goto finish;
        bytes-=PREFS_RECORD;
    }
    if(Read(f,font,sizeof(font))!=(LONG)sizeof(font) ||
       memcmp(font,"DFNT",4) || get32(font+4)!=PREFS_FONT_PAYLOAD ||
       !memchr(font+8,0,DOLLY_FONT_NAME_BYTES) ||
       get32(font+8+DOLLY_FONT_NAME_BYTES)>65535u ||
       get32(font+12+DOLLY_FONT_NAME_BYTES)>255u ||
       (!!font[8] != !!get32(font+8+DOLLY_FONT_NAME_BYTES)) ||
       Read(f,&extra,1)!=0)goto finish;
    memcpy(dolly_amiga_font_choice.name,font+8,DOLLY_FONT_NAME_BYTES);
    dolly_amiga_font_choice.size=(UWORD)get32(font+8+DOLLY_FONT_NAME_BYTES);
    dolly_amiga_font_choice.style=(UBYTE)get32(font+12+DOLLY_FONT_NAME_BYTES);
    dolly_amiga_set_legacy_override((flags&PREFS_LEGACY_LIMIT)!=0);
    dolly_amiga_set_verify_writes((flags&PREFS_VERIFY_WRITES)!=0);
    dolly_amiga_set_suppress_same_unit_warning((flags&PREFS_SUPPRESS_SAME_UNIT_WARNING)!=0);
    dolly_amiga_set_allow_outside_range((flags&PREFS_ALLOW_OUTSIDE_RANGE)!=0);
    dolly_amiga_set_mount_destination_default((flags&PREFS_MOUNT_DESTINATION_DEFAULT)!=0);
    error=NULL;
finish:
    Close(f);
    if(error)dolly_amiga_io_settings_clear();
    return error;
}

int dolly_amiga_prefs_save(void) {
    unsigned char header[42]={0},record[PREFS_RECORD],font[PREFS_FONT_CHUNK]={0};
    char drawer[sizeof(prefs_path)+12];
    char temp[sizeof(drawer)+5],backup[sizeof(drawer)+5];
    DollyPhysicalEntry device;
    DollyIoHints hints;
    uint32_t count=0,bytes,i;
    size_t prefix;
    BPTR f,lock;
    int ok,existing;
    for(i=0;dolly_amiga_io_entry(i,&device,&hints);++i)++count;
    bytes=8u+count*PREFS_RECORD;
    memcpy(header,"FORM",4);put32(header+4,26u+bytes+PREFS_FONT_CHUNK);
    memcpy(header+8,"PREFPRHD",8);put32(header+16,6);
    header[20]=PREFS_VERSION;
    memcpy(header+26,"DOIO",4);put32(header+30,bytes);
    put32(header+34,(dolly_amiga_legacy_override()?PREFS_LEGACY_LIMIT:0u) |
                    (dolly_amiga_verify_writes()?PREFS_VERIFY_WRITES:0u) |
                    (dolly_amiga_suppress_same_unit_warning()?PREFS_SUPPRESS_SAME_UNIT_WARNING:0u) |
                    (dolly_amiga_allow_outside_range()?PREFS_ALLOW_OUTSIDE_RANGE:0u) |
                    (dolly_amiga_mount_destination_default()?PREFS_MOUNT_DESTINATION_DEFAULT:0u));
    put32(header+38,count);
    prefix=(size_t)((const char *)FilePart((STRPTR)prefs_path)-prefs_path);
    memcpy(drawer,prefs_path,prefix);
    strcpy(drawer+prefix,".dolly-tmp");
    strcpy(temp,drawer);strcat(temp,"/new");
    strcpy(backup,drawer);strcat(backup,"/old");
    lock=CreateDir(drawer);
    if(!lock)return 0;
    UnLock(lock);
    f=Open(temp,MODE_NEWFILE);
    if(!f) {
        DeleteFile(drawer);
        return 0;
    }
    ok=Write(f,header,sizeof(header))==(LONG)sizeof(header);
    for(i=0;ok && dolly_amiga_io_entry(i,&device,&hints);++i) {
        memset(record,0,sizeof(record));
        memcpy(record,device.driver,strlen(device.driver));
        put32(record+DOLLY_PHYS_DRIVER_LEN,device.unit);
        put32(record+DOLLY_PHYS_DRIVER_LEN+4,hints.present);
        put32(record+DOLLY_PHYS_DRIVER_LEN+8,hints.mask);
        put32(record+DOLLY_PHYS_DRIVER_LEN+12,hints.buffer_memory);
        put32(record+DOLLY_PHYS_DRIVER_LEN+16,hints.max_transfer);
        ok=Write(f,record,sizeof(record))==(LONG)sizeof(record);
    }
    memcpy(font,"DFNT",4);
    put32(font+4,PREFS_FONT_PAYLOAD);
    memcpy(font+8,dolly_amiga_font_choice.name,strlen(dolly_amiga_font_choice.name));
    put32(font+8+DOLLY_FONT_NAME_BYTES,dolly_amiga_font_choice.size);
    put32(font+12+DOLLY_FONT_NAME_BYTES,dolly_amiga_font_choice.style);
    if(ok)ok=Write(f,font,sizeof(font))==(LONG)sizeof(font);
    if(!Close(f))ok=0;
    if(ok) {
        lock=Lock(prefs_path,ACCESS_READ);
        existing=lock!=0;
        if(lock) {
            struct FileInfoBlock info;
            ok=Examine(lock,&info) && info.fib_DirEntryType<0;
            UnLock(lock);
        } else if(IoErr()!=ERROR_OBJECT_NOT_FOUND)ok=0;
        if(ok && existing && !Rename(prefs_path,backup))ok=0;
        if(ok) {
            ok=Rename(temp,prefs_path)!=0;
            if(existing) {
                if(ok)DeleteFile(backup);
                else Rename(backup,prefs_path);
            }
        }
    }
    if(!ok)DeleteFile(temp);
    DeleteFile(drawer);
    return ok;
}
