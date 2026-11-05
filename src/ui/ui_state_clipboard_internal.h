#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context clipboard storage. */
typedef struct {
  UIClipboard clipboard;
  void *clipboard_user;
} UIClipboardState;
UIClipboardState *ui_state_clipboard(UIContext *ctx);
