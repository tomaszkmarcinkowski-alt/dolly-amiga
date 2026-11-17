#include "dolly_paths.h"
#include <string.h>
DollyPathStatus dolly_path_resource(const char *relative,char *out,
                                    size_t capacity){
    const char *p,*component;
    size_t used,root_len=sizeof(DOLLY_RESOURCE_ROOT)-1;
    DollyPathStatus error=DOLLY_PATH_INVALID_RELATIVE;
    if(!out||capacity==0)return DOLLY_PATH_BAD_ARGUMENT;
    out[0]='\0';
    if(!relative||!relative[0])return DOLLY_PATH_BAD_ARGUMENT;
    if(relative[0]=='/'||relative[0]=='\\')return error;
    component=relative;
    for(p=relative;;++p){
        unsigned char ch=(unsigned char)*p;
        if(ch==':'||ch=='\\'||ch<32||ch==127)return error;
        if(ch=='/'||ch=='\0'){
            size_t n=(size_t)(p-component);
            if(n==0 || (n==1&&component[0]=='.') ||
               (n==2&&component[0]=='.'&&component[1]=='.'))
                return error;
            if(ch=='\0')break;
            component=p+1;
        }
    }
    used=(size_t)(p-relative);
    if(root_len>=capacity || used>capacity-root_len-1)
        return DOLLY_PATH_TOO_LONG;
    memcpy(out,DOLLY_RESOURCE_ROOT,root_len);
    memcpy(out+root_len,relative,used+1);
    return DOLLY_PATH_OK;
}
const char *dolly_path_status_name(DollyPathStatus status){
    switch(status){
    case DOLLY_PATH_OK:return "OK";
    case DOLLY_PATH_BAD_ARGUMENT:return "Missing path or output buffer";
    case DOLLY_PATH_INVALID_RELATIVE:return "Invalid relative resource path";
    case DOLLY_PATH_TOO_LONG:return "Resource path is too long";
    default:return "Unknown resource path status";
    }
}
