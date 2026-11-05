#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context menu storage. */
typedef struct {
  UIMenuHierarchy g_menu;
} UIMenuState;
UIMenuState *ui_state_menu(UIContext *ctx);
const UIMenuState *ui_state_menu_read(const UIContext *ctx);
