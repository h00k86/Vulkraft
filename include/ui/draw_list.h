#pragma once
#include "ui/renderer.h"
#include <stddef.h>
typedef struct UIDrawList UIDrawList;
typedef enum {
  UI_DRAW_LIST_OK, UI_DRAW_LIST_STATE, UI_DRAW_LIST_MEMORY,
  UI_DRAW_LIST_LIMIT, UI_DRAW_LIST_CLIP, UI_DRAW_LIST_RESOURCE
} UIDrawListError;
/* Target/user and fonts/textures are borrowed until replay or discard.
 * Text and array arguments are copied. Text uses a reusable arena retained until
 * destroy (up to 16 MiB, including terminators). Target requires the same callbacks as UIContext; skin extensions are optional.
 * Complete/discard target geometry before next begin/destroy: pending preview
 * releases happen there, not at replay. Detach the context before destruction. */
UIDrawList *ui_draw_list_create(const UIRenderer *target, void *user);
void ui_draw_list_destroy(UIDrawList *list);
const UIRenderer *ui_draw_list_renderer(void);
/* Begin discards previous commands and clears errors; retains command/text capacity.
 * Call after the target starts its frame. Never during replay or a callback. */
bool ui_draw_list_begin(UIDrawList *list);
/* Abandon commands, owned text and deferred preview releases, without starting
 * a new recording. Retains allocation capacity and the first error until begin.
 * Complete/discard target geometry first if replay has already been attempted.
 * Does not undo target draws or modify target clips. Repeated calls are safe;
 * NULL and calls during replay return false. Never call from target callbacks. */
bool ui_draw_list_discard(UIDrawList *list);
/* One replay per begin; balanced clips required. Target failures can leave
 * partial geometry: caller must discard that render frame on false. */
bool ui_draw_list_replay(UIDrawList *list);
/* First error is retained until begin; failed frames reject new draws and
 * preview loads. Read-only metrics and resource cleanup remain available. */
UIDrawListError ui_draw_list_error(const UIDrawList *list);
size_t ui_draw_list_count(const UIDrawList *list);
