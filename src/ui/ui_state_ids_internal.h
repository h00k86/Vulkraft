#pragma once
#include "ui_state_types_internal.h"

/* Borrowed per-context ids storage. */
typedef struct {
  char g_id_stack[MAX_UI_ID_STACK][UI_ID_CAPACITY];
  int g_id_stack_depth;
  char g_scoped_id_buffers[UI_SCOPED_ID_BUFFER_COUNT][UI_ID_CAPACITY];
  unsigned g_scoped_id_buffer_index;
} UIIdsState;
UIIdsState *ui_state_ids(UIContext *ctx);
