#ifndef DOLLY_VERSION_H
#define DOLLY_VERSION_H
/* Standard AmigaDOS $VER: identification string, separate from CRC32.
 * Program version/revision is exposed to C:Version FILE on classic OS.
 * Actual release values are to be generated from one build-version source.
 */
extern const char dolly_version_tag[];
const char *dolly_version_text(void);
#endif
