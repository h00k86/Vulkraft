#pragma once
#include "ui/ui.h"

/* Private editing state, embedded in UIContext but usable without a context,
 * renderer or active frame. Buffer storage remains owned by the caller. */
#define UI_TEXT_HISTORY_COUNT 17
#define UI_TEXT_HISTORY_BYTES 65536
#define UI_TEXT_SNAPSHOT_BYTES 32768
typedef struct { char *text; int caret, anchor; } UITextRevision;
typedef struct {
  char id[UI_ID_CAPACITY];
  bool focused;
  int caret, selection_anchor;
  UITextRevision history[UI_TEXT_HISTORY_COUNT];
  int history_count, history_position;
  size_t history_bytes;
  uint64_t observed_hash;
  size_t observed_length;
  bool observed;
  char original[256];
  char *original_long;
  bool needs_snapshot;
  unsigned long long last_seen_frame;
} UITextInputState;

/* state is non-NULL and zero-initialized on first use. prepare validates the
 * buffer/capacity and reconciles offsets before events are applied. All other
 * operations require a valid NUL-terminated UTF-8 buffer and byte-boundary
 * offsets within it. Clipboard callbacks/user are borrowed; an all-zero table
 * disables clipboard operations. observe records the final buffer after edits.
 * The widget owns focus transitions and release of original_long; history_clear
 * releases only history allocations. These headers/functions are not public API. */
void ui_text_history_clear(UITextInputState *state);
bool ui_text_snapshot(UITextInputState *state,const char *buffer);
bool ui_text_edit_prepare(UITextInputState *state,const char *buffer,int capacity,int *length);
void ui_text_edit_event(UITextInputState *state,char *buffer,int capacity,const InputKeyEvent *event,
                        const UIClipboard *clipboard,void *user);
void ui_text_edit_cancel(UITextInputState *state,char *buffer,int capacity);
void ui_text_edit_observe(UITextInputState *state,const char *buffer);
