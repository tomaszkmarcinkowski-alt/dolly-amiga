#ifndef DOLLY_VERSION_H
#define DOLLY_VERSION_H
#define DOLLY_VERSION_NUMBER "3.7"
#define DOLLY_RELEASE_DATE "21.11.2026"
#define DOLLY_WINDOW_TITLE "Dolly Alpha " DOLLY_VERSION_NUMBER

/* Standard AmigaDOS $VER: identification string, separate from CRC32. */
extern const char dolly_version_tag[];
const char *dolly_version_text(void);
#endif
