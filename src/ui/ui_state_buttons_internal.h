#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context buttons storage. */
typedef struct {
  UIState ui_states[MAX_UI_ELEMENTS];
  int ui_state_count;
  bool warned_state_capacity;
} UIButtonsState;
UIButtonsState *ui_state_buttons(UIContext *ctx);

UIState *ui_button_state(const char *id);
