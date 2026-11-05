#pragma once
#include "ui/ui.h"
#include "ui_state_types_internal.h"

/* Private navigation contract. Every operation acts on the supplied context;
 * no implicit current context, rendering or backend callbacks are used here.
 * begin/end bracket widget registration in an active frame. Contexts and IDs
 * are borrowed and non-NULL; event indices must be below key_event_count.
 * menu_open is the host core's menu-capture snapshot at frame start. */
enum { UI_NAV_BUTTON, UI_NAV_TEXT, UI_NAV_NUMBER, UI_NAV_SLIDER, UI_NAV_DROPDOWN };
/* Reserve before opening a public frame, so OOM leaves its input/state intact.
 * begin also reserves for direct module consumers and returns false on OOM. */
bool ui_nav_reserve_events(UIContext *c, size_t count);
bool ui_nav_begin(UIContext *c, bool cancelled, bool menu_open);
const char *ui_nav_scope(UIContext *c);
bool ui_nav_allowed(UIContext *c);
void ui_nav_set(UIContext *c, const char *id,bool keyboard);
int ui_nav_find(UIContext *c, const UINavigationEntry *entries,int count,const char *id);
bool ui_nav_event(UIContext *c, const char *id,size_t n);
bool ui_nav_has_events(UIContext *c, const char *id);
bool ui_nav_focused(UIContext *c, const char *id);
bool ui_nav_key(UIContext *c, const char *id,int key,bool repeat);
bool ui_nav_activate(UIContext *c, const char *id);
bool ui_nav_register(UIContext *c, const char *id,UIRect rect,int kind,bool enabled,bool hover);
void ui_nav_restore(UIContext *c, UIModalOpenState *modal);
void ui_nav_modal_enter(UIContext *c, UIModalOpenState *modal);
bool ui_nav_editor_escape(UIContext *c);
void ui_nav_end(UIContext *c);
void ui_nav_clear_editor_focus(UIContext *c);
