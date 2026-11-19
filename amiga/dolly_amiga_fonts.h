#ifndef DOLLY_AMIGA_FONTS_H
#define DOLLY_AMIGA_FONTS_H
#include <graphics/text.h>

struct Screen;
struct Window;

#define DOLLY_FONT_NAME_BYTES 64
typedef struct DollyFontChoice {
    char name[DOLLY_FONT_NAME_BYTES];
    UWORD size;
    UBYTE style;
} DollyFontChoice;

extern DollyFontChoice dolly_amiga_font_choice;
int dolly_amiga_fonts_init(struct Screen *screen);
int dolly_amiga_fonts_changed(void);
void dolly_amiga_fonts_apply(void);
int dolly_amiga_fonts_use_topaz(void);
void dolly_amiga_fonts_release_previous(void);
void dolly_amiga_fonts_close(void);
struct TextFont *dolly_amiga_font(int log_view);
struct TextAttr *dolly_amiga_text_attr(int log_view);
void dolly_amiga_window_font(struct Window *window,int log_view);
int dolly_amiga_choose_font(struct Window *parent,DollyFontChoice *choice);
#endif
