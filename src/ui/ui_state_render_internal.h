#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context render storage. */
typedef struct {
  UIRenderer renderer;
  void *renderer_user;
  bool preview_owned;
} UIRenderState;
UIRenderState *ui_state_render(UIContext *ctx);
