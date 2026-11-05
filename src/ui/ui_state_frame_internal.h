#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context frame storage. */
typedef struct {
  bool frame_active;
  float canvas_width, canvas_height;
  UIRect layout_input_clips[128];
  int layout_input_clip_count;
  bool debug_keys_down;
  InputKeyEvent key_events[INPUT_KEYBOARD_CAPACITY];
  size_t key_event_count;
  unsigned long long g_ui_frame_serial;
  UIInputRegion g_input_regions[MAX_UI_INPUT_REGIONS];
  int g_input_region_count;
  float g_mouse_y;
  float g_mouse_x;
  bool g_mouse_down;
  bool g_mouse_down_prev;
  bool g_mouse_clicked;
  bool g_mouse_released;
  bool g_escape_pressed;
  bool g_escape_consumed;
  bool g_click_claimed_by_dropdown_list;
  bool g_click_claimed_by_menu;
  float g_delta_time;
  bool g_any_hover;
  float g_scroll_delta;
  bool g_scroll_consumed;
} UIFrameState;
UIFrameState *ui_state_frame(UIContext *ctx);
const UIFrameState *ui_state_frame_read(const UIContext *ctx);
