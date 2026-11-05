#pragma once
#include "ui_state_types_internal.h"
#include <assert.h>

/* Shared lifecycle hooks for frame orchestration and overlay transitions.
 * All operate on the selected context; public widget APIs use the same context. */
static inline UIContext *ui_overlay_context(void) {
  UIContext *ctx=ui_context_get_current();
  assert(ctx && "Select a UIContext before using overlays/input capture");
  return ctx;
}
void ui_context_clear_preview(UIContext *ctx);
bool ui_any_modal_open(void);
void ui_menu_begin_frame(void);
void ui_close_all_dropdowns(void);
void ui_reset_dropdown_popup_pass(void);
void ui_modal_input_begin_frame(void);
void ui_modal_input_end_frame(void);
