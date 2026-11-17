#include "dolly_probe.h"
#include <inttypes.h>
#include <stdio.h>
int main(int argc, char **argv) {
    DollyProbeResult result;
    DollyProbeError error;
    FILE *file;
    if (argc != 2) {
        fprintf(stderr, "Usage: dolly-inspect <image.adf|image.hdf|image.dly>\n");
        return 20;
    }
    file = fopen(argv[1], "rb");
    if (!file) { perror(argv[1]); return 20; }
    error = dolly_probe_stream(file, &result);
    if (fclose(file) != 0) { perror("close"); return 20; }
    if (error != DOLLY_PROBE_OK) {
        fprintf(stderr, "Dolly: %s\n", dolly_probe_error_name(error));
        return 10;
    }
    printf("Dolly M0 Inspector (read-only)\n");
    printf("Kind: %s\n", dolly_kind_name(result.kind));
    printf("Bytes: %" PRIu64 "\n", result.file_size);
    if (result.looks_like_iff) printf("IFF FORM size: %" PRIu32 "\n", result.iff_form_size);
    printf("Note: %s\n", result.note);
    return result.kind == DOLLY_IMAGE_UNKNOWN ? 5 : 0;
}
