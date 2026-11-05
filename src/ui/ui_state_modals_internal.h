#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context modals storage. */
typedef struct {
  UIModalOpenState g_modal_states[MAX_UI_MODALS];
  int g_modal_state_count;
  bool g_modal_rect_valid;
  bool g_modal_rect_seen_this_frame;
  UIRect g_modal_rect;
  int g_modal_scope_depth;
  int g_modal_open_count_prev;
} UIModalsState;
UIModalsState *ui_state_modals(UIContext *ctx);
