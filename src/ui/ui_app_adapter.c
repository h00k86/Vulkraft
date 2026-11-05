#include "ui/app_adapter.h"
/* Compatibilita con APP: solo questo adattatore raccoglie input dalla finestra.
 * Il core riceve un frame esplicito e conserva una copia degli eventi. */
#include "app/application.h"
#include "input/keyboard.h"
#include "input/scroll.h"
#include "ui/ui.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static bool app_clipboard_read(void *user,const char **text,size_t *bytes) {
  *text=platform_clipboard_text((PlatformWindow *)user);
  if(!*text) return false;
  *bytes=strlen(*text); return true;
}
static bool app_clipboard_write(void *user,const char *text) {
  return platform_clipboard_set_text((PlatformWindow *)user,text);
}
bool ui_bind_app_clipboard(APP *app) {
  const UIClipboard clipboard={.read=app_clipboard_read,.write=app_clipboard_write};
  return app && app->window && ui_context_set_clipboard(app->ui,&clipboard,app->window);
}

void ui_begin_frame(APP *app, float x, float y, bool down, float dt) {
  input_keyboard_begin_frame(app ? app->window : NULL);
  float scroll=input_scroll_take();
  UIFrameInput input = {
    .width=VIRTUAL_W, .height=VIRTUAL_H,
    .mouse_x=x, .mouse_y=y, .mouse_down=down, .delta_time=dt,
    .focus_lost=input_keyboard_focus_lost(),
    /* Preserve ui_set_scroll_delta's previous handling of invalid totals. */
    .scroll_delta=isfinite(scroll)?scroll:0
  };
  input.events=input_keyboard_events(&input.event_count);
#ifdef DEBUG
  if (app && app->window)
    input.debug_keys_down =
      platform_key_state(app->window, PLATFORM_KEY_F3)==PLATFORM_PRESS ||
      platform_key_state(app->window, PLATFORM_KEY_G)==PLATFORM_PRESS ||
      platform_key_state(app->window, PLATFORM_KEY_R)==PLATFORM_PRESS ||
      platform_key_state(app->window, PLATFORM_KEY_S)==PLATFORM_PRESS;
#endif
  bool started=ui_begin_frame_input(&input);
  assert(started && "Invalid or unbalanced UI frame");
  (void)started;
}
