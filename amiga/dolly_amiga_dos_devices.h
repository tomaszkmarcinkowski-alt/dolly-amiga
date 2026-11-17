#ifndef DOLLY_AMIGA_DOS_DEVICES_H
#define DOLLY_AMIGA_DOS_DEVICES_H
/* AmigaDOS 2.04+ read-only enumeration of DOS device names.
 * Snapshot owns every name; no DOS-list pointer escapes the lock.
 * No OpenDevice, no disk access, no writes.
 */
#define DOLLY_DOS_MAX_DEVICES 24u
#define DOLLY_DOS_NAME_BYTES 40u
typedef struct DollyDosDevices {
    unsigned count;
    unsigned truncated;
    int available;
    char names[DOLLY_DOS_MAX_DEVICES][DOLLY_DOS_NAME_BYTES];
} DollyDosDevices;
void dolly_amiga_discover_dos_devices(DollyDosDevices *out);
#endif
