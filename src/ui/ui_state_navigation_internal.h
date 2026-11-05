#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context navigation storage. */
typedef struct {
  UINavigationEntry nav_previous[MAX_UI_ELEMENTS], nav_current[MAX_UI_ELEMENTS];
  int nav_previous_count, nav_count;
  char nav_focus[UI_ID_CAPACITY], nav_modal[UI_ID_CAPACITY];
  char nav_scopes[MAX_UI_MODALS][UI_ID_CAPACITY];
  int nav_scope_depth, nav_scroll_depth;
  int nav_scroll_stack[MAX_UI_SCROLL_SCOPES];
  UIRect nav_scroll_viewports[MAX_UI_SCROLL_SCOPES];
  /* Exact per-event snapshots, reserved for the actual batch size on demand. */
  char (*nav_event_owner)[UI_ID_CAPACITY];
  size_t nav_event_capacity;
  bool nav_enabled, nav_keyboard, nav_edit, nav_auto, nav_cancelled, nav_pointer_claimed;
  bool nav_editor_finished; /* Focus is retained after Enter/Esc, editing is not. */
  int nav_pending_step;
} UINavigationState;
UINavigationState *ui_state_navigation(UIContext *ctx);
