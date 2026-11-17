#include "dolly_selfcheck.h"
const char *dolly_selfcheck_message(DollySelfCheckResult result){
    switch(result){
    case DOLLY_SELF_OK:
        return "Dolly version and embedded CRC32 passed.";
    case DOLLY_SELF_INVALID:
        return "Dolly integrity check is unavailable. Disk functions disabled.";
    case DOLLY_SELF_OPEN_FAILED:
        return "Cannot locate/open the running Dolly executable. Disk functions disabled.";
    case DOLLY_SELF_READ_FAILED:
        return "Cannot read the Dolly executable completely. Disk functions disabled.";
    case DOLLY_SELF_VERSION_MISMATCH:
        return "Dolly executable release does not match its embedded version.";
    case DOLLY_SELF_SIZE_MISMATCH:
        return "Dolly executable size does not match its expected structure.";
    case DOLLY_SELF_CRC_MISMATCH:
        return "Wilki pogryzly Dolly! Program file CRC32 is invalid. Reinstall Dolly.";
    case DOLLY_SELF_RECORD_MISSING:
        return "Dolly CRC32 record missing. Please reinstall the complete program.";
    case DOLLY_SELF_RECORD_INVALID:
        return "Dolly CRC32 record is invalid or inconsistent. Disk functions disabled.";
    case DOLLY_SELF_RECORD_UNSTAMPED:
        return "Dolly has not been stamped with its release CRC32. Disk functions disabled.";
    default:
        return "Unknown Dolly self-check result. Disk functions disabled.";
    }
}
