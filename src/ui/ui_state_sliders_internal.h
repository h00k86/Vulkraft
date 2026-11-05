#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context sliders storage. */
typedef struct {
  UISliderState slider_states[MAX_UI_SLIDERS];
  int slider_state_count;
  bool warned_slider_capacity;
} UISlidersState;
UISlidersState *ui_state_sliders(UIContext *ctx);

UISliderState *ui_slider_state(const char *id);
