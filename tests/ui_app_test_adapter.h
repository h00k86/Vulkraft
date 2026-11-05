#pragma once
/* White-box compatibility for older tests that advance only the keyboard batch. */
#include "../src/ui/ui_app_adapter.c"
static void ui_keyboard_begin_frame(APP *app) {
  input_keyboard_begin_frame(app ? app->window : NULL);
  size_t count=0;
  InputKeyEvent *events=input_keyboard_events(&count);
  ui_keyboard_set_frame(events,count,input_keyboard_focus_lost());
}
