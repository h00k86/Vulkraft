#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context panels storage. */
typedef struct {
  UISectionState g_section_states[MAX_UI_SECTIONS];
  int g_section_state_count;
  UIScrollState g_scroll_states[MAX_UI_SCROLL_SCOPES];
  int g_scroll_state_count;
  UIPanelState panel_states[MAX_UI_PANELS];
  int panel_state_count;
  bool g_section_default_open;
  bool warned_section_capacity;
  bool warned_panel_capacity;
} UIPanelsState;
UIPanelsState *ui_state_panels(UIContext *ctx);
