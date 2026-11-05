#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context dropdowns storage. */
typedef struct {
  UIDropdownState dropdown_states[MAX_UI_DROPDOWNS];
  int dropdown_state_count;
  UIDropdownState *g_dropdown_click_owner;
  int g_dropdown_submission_serial;
  bool g_dropdown_popup_pass_drawn;
  bool warned_dropdown_capacity;
} UIDropdownsState;
UIDropdownsState *ui_state_dropdowns(UIContext *ctx);
