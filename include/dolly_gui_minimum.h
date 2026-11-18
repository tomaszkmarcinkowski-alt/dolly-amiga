#ifndef DOLLY_GUI_MINIMUM_H
#define DOLLY_GUI_MINIMUM_H
/* Portable input contract: every width is an ACTUAL measured pixel width.
 * No text metrics, GadTools or Intuition structures are guessed here.
 */
#include <stdint.h>
typedef enum DollyGuiWindowKind {
    DOLLY_GUI_WINDOW_MAIN=0,
    DOLLY_GUI_WINDOW_SHEEP=1
} DollyGuiWindowKind;
typedef struct DollyGuiMinMetrics {
    uint32_t border_left,border_right,border_top,border_bottom;
    uint32_t padding_x,padding_y;
    uint32_t line_height;
    /* Main-window layout's measured lowest extent (client coordinates). */
    uint32_t main_content_height;
    uint32_t main_build_label_width;
    uint32_t heading_width;
    uint32_t show_button_text_width;
    uint32_t quit_button_text_width;
    uint32_t hide_button_text_width,log_button_text_width;
} DollyGuiMinMetrics;
typedef struct DollyGuiMinimum {
    uint32_t outer_width,outer_height;
    uint8_t valid;
} DollyGuiMinimum;
/* Returns valid=0 for bad/unrepresentable inputs.
 * Optional explanatory text is not a min-size constraint; renderer must
 * suppress/wrap it if it does not fit. All controls remain mandatory.
 */
DollyGuiMinimum dolly_gui_measure_minimum(DollyGuiWindowKind kind,
                                         DollyGuiMinMetrics metrics);
#endif
