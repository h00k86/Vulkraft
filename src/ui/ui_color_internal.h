#pragma once
#include "ui/ui.h"
/* Stateful controls keep per-context HSV ambiguity out of pure composition. */
bool ui_color_hsv_begin(const char *id,UIColor color,UIColorHSV *hsv);
void ui_color_hsv_end(const char *id,UIColor color,UIColorHSV hsv);
bool ui_color_sv_pad(UIPropertyLayout *layout,UIColorHSV *hsv,bool *started);
