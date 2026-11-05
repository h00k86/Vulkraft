#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context style storage. */
typedef struct {
  UITheme g_theme;
  bool g_theme_initialized;
  bool sentence_case; /* Opt-in label normalization; default preserves bytes. */
  UIToast g_toasts[MAX_UI_TOASTS];
  int g_toast_count;
  UIFeedbackFn g_feedback;
  void *g_feedbackUser;
  float g_toast_top;
  bool feedback_pressed_last;
  bool feedback_last_down;
  bool feedback_last_any;
  unsigned feedback_last_hover;
} UIStyleState;
UIStyleState *ui_state_style(UIContext *ctx);
