#pragma once
/* Physical context layout is private to ui.c and lifecycle tests. */
#include "ui_state_navigation_internal.h"
#include "ui_state_render_internal.h"
#include "ui_state_clipboard_internal.h"
#include "ui_state_tooltip_internal.h"
#include "ui_state_frame_internal.h"
#include "ui_state_ids_internal.h"
#include "ui_state_panels_internal.h"
#include "ui_state_buttons_internal.h"
#include "ui_state_modals_internal.h"
#include "ui_state_style_internal.h"
#include "ui_state_colors_internal.h"
#include "ui_state_menu_internal.h"
#include "ui_state_numeric_internal.h"
#include "ui_state_sliders_internal.h"
#include "ui_state_text_fields_internal.h"
#include "ui_state_dropdowns_internal.h"
#include "ui_state_picker_internal.h"
#include "ui_state_debug_internal.h"
#include "ui_state_errors_internal.h"

struct UIContext {
  UINavigationState navigation;
  UIRenderState render;
  UIClipboardState clipboard;
  UITooltipState tooltip;
  UIFrameState frame;
  UIIdsState ids;
  UIPanelsState panels;
  UIButtonsState buttons;
  UIModalsState modals;
  UIStyleState style;
  UIColorsState colors;
  UIMenuState menu;
  UINumericState numeric;
  UISlidersState sliders;
  UITextFieldsState text_fields;
  UIDropdownsState dropdowns;
  UIPickerState picker;
  UIDebugState debug;
  UIErrorsState errors;
};

void ui_context_clear_preview(UIContext *ctx);
