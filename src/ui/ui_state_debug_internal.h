#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context debug storage. */
typedef struct {
  bool debug_key_was_down;
  char debug_previous_owner[128];
  bool debug_previous_capture;
  int debug_enabled;
} UIDebugState;
UIDebugState *ui_state_debug(UIContext *ctx);
