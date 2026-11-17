#include "dolly_version.h"
/* The leading NUL makes $VER detectable in AmigaDOS file scanning.
 * This is a provisional M0 version, not an announced public release.
 * Keep linked in the main executable by referencing it from startup.
 * Later release automation must update it together with the build ID. */
const char dolly_version_tag[]="\0$VER: Dolly 0.1 (17.11.2026)";
const char *dolly_version_text(void){return dolly_version_tag+1;}
