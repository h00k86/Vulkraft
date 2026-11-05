#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context picker storage. */
typedef struct {
  void *file_picker_state; /* Owned, free-compatible allocation; no owned children. Optional module defines contents. */
  bool file_picker_open;
  char file_picker_id[UI_ID_CAPACITY];
} UIPickerState;
UIPickerState *ui_state_picker(UIContext *ctx);
