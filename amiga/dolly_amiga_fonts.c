#include "dolly_amiga_fonts.h"
#include <exec/libraries.h>
#include <graphics/gfxbase.h>
#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <proto/asl.h>
#include <proto/diskfont.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <string.h>

extern struct GfxBase *GfxBase;
extern struct Library *AslBase;
struct Library *DiskfontBase;
DollyFontChoice dolly_amiga_font_choice;
static DollyFontChoice applied_choice;
static struct TextFont *topaz,*screen_font,*gui_font,*log_font,*opened,*previous;
static struct TextAttr gui_attr,log_attr;

static struct TextAttr font_attr(struct TextFont *font) {
    struct TextAttr attr;
    attr.ta_Name=(STRPTR)font->tf_Message.mn_Node.ln_Name;
    attr.ta_YSize=font->tf_YSize;
    attr.ta_Style=font->tf_Style;
    attr.ta_Flags=font->tf_Flags;
    return attr;
}
static struct TextFont *load_choice(const DollyFontChoice *choice) {
    struct TextAttr attr={(STRPTR)choice->name,choice->size,choice->style,0};
    struct TextFont *font=OpenFont(&attr);

    if(font && font->tf_YSize!=choice->size) {
        CloseFont(font);
        font=NULL;
    }
    if(!font && DiskfontBase)font=OpenDiskFont(&attr);
    if(font && font->tf_YSize!=choice->size) {
        CloseFont(font);
        return NULL;
    }
    return font;
}
int dolly_amiga_fonts_init(struct Screen *screen) {
    struct TextAttr fallback={(STRPTR)"topaz.font",8,FS_NORMAL,FPF_ROMFONT};
    topaz=OpenFont(&fallback);
    screen_font=screen->RastPort.Font?screen->RastPort.Font:topaz;
    log_font=GfxBase->DefaultFont?GfxBase->DefaultFont:topaz;
    if(!screen_font || !log_font)return 0;
    DiskfontBase=OpenLibrary("diskfont.library",36);
    log_attr=font_attr(log_font);
    dolly_amiga_fonts_apply();
    return gui_font!=NULL;
}
int dolly_amiga_fonts_changed(void) {
    return strcmp(dolly_amiga_font_choice.name,applied_choice.name)!=0 ||
           dolly_amiga_font_choice.size!=applied_choice.size ||
           dolly_amiga_font_choice.style!=applied_choice.style;
}
void dolly_amiga_fonts_apply(void) {
    previous=opened;
    opened=dolly_amiga_font_choice.name[0]?load_choice(&dolly_amiga_font_choice):NULL;
    gui_font=opened?opened:(dolly_amiga_font_choice.name[0] && topaz?topaz:screen_font);
    gui_attr=font_attr(gui_font);
    applied_choice=dolly_amiga_font_choice;
}
int dolly_amiga_fonts_use_topaz(void) {
    if(!topaz || gui_font==topaz)return 0;
    gui_font=topaz;
    gui_attr=font_attr(gui_font);
    return 1;
}
void dolly_amiga_fonts_release_previous(void) {
    if(previous)CloseFont(previous);
    previous=NULL;
}
void dolly_amiga_fonts_close(void) {
    dolly_amiga_fonts_release_previous();
    if(opened)CloseFont(opened);
    if(topaz)CloseFont(topaz);
    if(DiskfontBase)CloseLibrary(DiskfontBase);
}
struct TextFont *dolly_amiga_font(int log_view) {
    return log_view?log_font:gui_font;
}
struct TextAttr *dolly_amiga_text_attr(int log_view) {
    return log_view?&log_attr:&gui_attr;
}
void dolly_amiga_window_font(struct Window *window,int log_view) {
    SetFont(window->RPort,dolly_amiga_font(log_view));
}
int dolly_amiga_choose_font(struct Window *parent,DollyFontChoice *choice) {
    struct FontRequester *request;
    DollyFontChoice selected={0};
    int ok=0;
    LONG max_height=((LONG)parent->WScreen->Height-parent->WScreen->BarHeight-
                    parent->BorderTop-parent->BorderBottom-41)/12-5;
    if(max_height>16)max_height=16;
    if(max_height<6)return 0;
    if(!AslBase)return 0;
    request=AllocAslRequestTags(ASL_FontRequest,ASLFO_Window,(ULONG)parent,
        ASLFO_TitleText,(ULONG)"Dolly - Choose interface font",
        ASLFO_InitialName,(ULONG)(choice->name[0]?(STRPTR)choice->name:gui_attr.ta_Name),
        ASLFO_InitialSize,(ULONG)(choice->name[0]?choice->size:gui_attr.ta_YSize),
        ASLFO_MinHeight,6UL,ASLFO_MaxHeight,(ULONG)max_height,TAG_DONE);
    if(!request)return 0;
    if(AslRequest(request,NULL) && request->fo_Attr.ta_Name &&
       strlen((const char *)request->fo_Attr.ta_Name)<sizeof(selected.name) &&
       request->fo_Attr.ta_YSize>=6u && request->fo_Attr.ta_YSize<=(ULONG)max_height) {
        struct TextFont *font;
        strcpy(selected.name,(const char *)request->fo_Attr.ta_Name);
        selected.size=request->fo_Attr.ta_YSize;
        selected.style=request->fo_Attr.ta_Style;
        font=load_choice(&selected);
        if(font) {
            CloseFont(font);
            *choice=selected;
            ok=1;
        }
    }
    FreeAslRequest(request);
    return ok;
}
