#ifndef DOLLY_AMIGA_DESTRUCTIVE_H
#define DOLLY_AMIGA_DESTRUCTIVE_H
struct Window;
/* Call after preflight and immediately before revalidating the destination. */
int dolly_amiga_confirm_destructive(struct Window *parent,const char *reason);
#endif
