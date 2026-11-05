#pragma once
#include "ui_state_types_internal.h"
/* HSV hue/saturation are ambiguous at gray/black; keep them per picker ID. */
#define UI_COLOR_MEMORY_CAPACITY 64
typedef struct {
  char id[UI_ID_CAPACITY];
  UIColor last;
  UIColorHSV hsv;
  unsigned long long last_seen_frame;
} UIColorMemory;
typedef struct { UIColorMemory entries[UI_COLOR_MEMORY_CAPACITY]; int count; } UIColorsState;
UIColorsState *ui_state_colors(UIContext *ctx);
