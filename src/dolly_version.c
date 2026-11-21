#include "dolly_version.h"
/* The leading NUL makes $VER detectable in AmigaDOS file scanning. */
const char dolly_version_tag[]="\0$VER: Dolly " DOLLY_VERSION_NUMBER " (" DOLLY_RELEASE_DATE ")";
const char *dolly_version_text(void){return dolly_version_tag+1;}
