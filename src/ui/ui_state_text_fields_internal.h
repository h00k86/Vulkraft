#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context text fields storage. */
typedef struct {
  UITextInputState text_input_states[MAX_UI_TEXT_INPUTS];
  int text_input_state_count;
  bool warned_text_capacity;
} UITextFieldsState;
UITextFieldsState *ui_state_text_fields(UIContext *ctx);

UITextInputState *ui_text_field_state(const char *id);
