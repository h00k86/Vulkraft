#pragma once
#include "ui_state_types_internal.h"

/* One capture policy shared by core widgets, navigation and overlays. The
 * claim flags survive popup dismissal until the next frame to prevent click
 * propagation. Dropdown/menu rendering and modal focus remain module-owned. */
void ui_capture_begin_frame(UIContext *ctx);
bool ui_capture_scroll_allowed(UIRect viewport);
bool ui_capture_widget_press(UIContext *ctx);
bool ui_capture_click_claimed(const UIContext *ctx);
bool ui_capture_keyboard_allowed(UIContext *ctx);
bool ui_capture_dropdown_press(UIContext *ctx,const UIDropdownState *state,bool enabled);
void ui_capture_claim_dropdown(UIContext *ctx,UIDropdownState *state);
void ui_capture_forget_dropdown(UIContext *ctx,const UIDropdownState *state);
void ui_capture_claim_menu(UIContext *ctx);
bool ui_capture_consume_escape(UIContext *ctx);
void ui_capture_mark_escape(UIContext *ctx);
bool ui_capture_editor_escape_allowed(const UIContext *ctx);
bool ui_capture_modal_escape_allowed(UIContext *ctx,const char *id);
bool ui_key_pressed(int key);
bool ui_any_dropdown_is_open(void);
bool ui_any_slider_dragging(void);
bool ui_file_picker_capture_active(void);
