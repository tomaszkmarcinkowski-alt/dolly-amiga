#include "dolly_iff.h"
#include <stdio.h>
#include <inttypes.h>
static int print_chunk(void *ignored,const DollyIffChunk *ch){
 (void)ignored;
 printf("  %-4s at byte %" PRIu64 ": %" PRIu32 " payload bytes\n",
        ch->id,ch->data_offset,ch->data_size);
 return 0;
}
int main(int argc,char **argv){
 DollyBlockSource *source=NULL;DollyIffSummary s;DollyIffStatus status;
 if(argc!=2){fprintf(stderr,"Usage: dolly-iff image.dly\n");return 20;}
 if(dolly_block_open_file(argv[1],&source)!=DOLLY_BLOCK_OK){
  fprintf(stderr,"Could not open %s\n",argv[1]);return 20;
 }
 status=dolly_iff_scan(source,print_chunk,NULL,&s);
 dolly_block_close(source);
 if(status!=DOLLY_IFF_OK){fprintf(stderr,"IFF: %s\n",dolly_iff_status_name(status));return 10;}
 printf("IFF FORM DOLY envelope valid; %" PRIu32 " chunks\n",s.chunk_count);
 puts("DOLY payload schemas and checksums NOT YET validated.");return 0;
}
