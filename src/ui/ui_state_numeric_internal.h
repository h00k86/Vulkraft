#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context numeric storage. */
typedef struct {
  UIFloatFieldState float_field_states[MAX_UI_FLOAT_FIELDS];
  int float_field_state_count;
} UINumericState;
UINumericState *ui_state_numeric(UIContext *ctx);

UIFloatFieldState *ui_numeric_state(const char *id);
