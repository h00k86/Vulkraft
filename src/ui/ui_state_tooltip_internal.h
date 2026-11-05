#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context tooltip storage. */
typedef struct {
  char tooltip_id[UI_ID_CAPACITY],tooltip_text[512],tooltip_modal[UI_ID_CAPACITY];
  UIRect tooltip_anchor;
  Font *tooltip_font;
  float tooltip_elapsed,tooltip_delay;
  unsigned long long tooltip_frame;
} UITooltipState;
UITooltipState *ui_state_tooltip(UIContext *ctx);
