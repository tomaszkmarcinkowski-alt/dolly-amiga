#include "dolly_paths.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    char buf[80];
    char tiny[10];
    assert(dolly_path_resource("Catalogs/dolly.catalog",buf,sizeof(buf))==DOLLY_PATH_OK);
    assert(!strcmp(buf,"DOLLY:Catalogs/dolly.catalog"));
    assert(dolly_path_resource("Plugins/ReAction.gui",buf,sizeof(buf))==DOLLY_PATH_OK);
    assert(!strcmp(buf,"DOLLY:Plugins/ReAction.gui"));
    assert(dolly_path_resource("Docs/Help.guide",buf,sizeof(buf))==DOLLY_PATH_OK);
    assert(dolly_path_resource("DOLLY:Docs/test",buf,sizeof(buf))==DOLLY_PATH_INVALID_RELATIVE);
    assert(buf[0]=='\0');
    assert(dolly_path_resource("../SYS:Startup-Sequence",buf,sizeof(buf))==DOLLY_PATH_INVALID_RELATIVE);
    assert(dolly_path_resource("Presets/../Docs/test",buf,sizeof(buf))==DOLLY_PATH_INVALID_RELATIVE);
    assert(dolly_path_resource("a//b",buf,sizeof(buf))==DOLLY_PATH_INVALID_RELATIVE);
    assert(dolly_path_resource("a/",buf,sizeof(buf))==DOLLY_PATH_INVALID_RELATIVE);
    assert(dolly_path_resource("./a",buf,sizeof(buf))==DOLLY_PATH_INVALID_RELATIVE);
    assert(dolly_path_resource("/a",buf,sizeof(buf))==DOLLY_PATH_INVALID_RELATIVE);
    assert(dolly_path_resource("a\\b",buf,sizeof(buf))==DOLLY_PATH_INVALID_RELATIVE);
    assert(dolly_path_resource("a\nb",buf,sizeof(buf))==DOLLY_PATH_INVALID_RELATIVE);
    assert(dolly_path_resource("Catalogs/dolly.catalog",tiny,sizeof(tiny))==DOLLY_PATH_TOO_LONG);
    assert(tiny[0]=='\0');
    assert(dolly_path_resource("",buf,sizeof(buf))==DOLLY_PATH_BAD_ARGUMENT);
    assert(dolly_path_resource(NULL,buf,sizeof(buf))==DOLLY_PATH_BAD_ARGUMENT);
    assert(dolly_path_resource("x",buf,1)==DOLLY_PATH_TOO_LONG);
    assert(dolly_path_resource("x",NULL,0)==DOLLY_PATH_BAD_ARGUMENT);
    puts("DOLLY assign resource path checks passed");
    return 0;
}
