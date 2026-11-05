#define UI_TEXT_EDIT_NO_MAIN
#include "ui_text_edit_test.c"
#include "../src/ui/ui_app_adapter.c"
#include "platform/headless.h"
float g_virtualWidth=640,g_virtualHeight=480;

static void app_press(APP *app,char *s,int keycode) {
  CHECK(platform_headless_post(app->window,(PlatformEvent){.kind=PLATFORM_EVENT_KEY,
        .key=keycode,.action=PLATFORM_PRESS,.mods=PLATFORM_MOD_CONTROL}));
  platform_poll_events();
  ui_begin_frame(app,-1,-1,false,.016f);
  ui_text_input("edit",font,ANCHOR_TOP_LEFT,0,14,20,20,100,s,128,NULL);
  ui_end_frame();
}
int main(void) {
  UIContext *c=context();
  PlatformWindowConfig config={.title="clipboard test",.width=640,.height=480};
  APP app={.ui=c,.window=platform_window_create(&config)}; CHECK(app.window);
  CHECK(ui_bind_app_clipboard(&app)); input_keyboard_register(app.window);
  char s[128]="è🙂"; ui_text_input_focus("edit");
  app_press(&app,s,PLATFORM_KEY_A); app_press(&app,s,PLATFORM_KEY_C);
  CHECK(!strcmp(platform_clipboard_text(app.window),"è🙂"));
  app_press(&app,s,PLATFORM_KEY_X); CHECK(!*s);
  app_press(&app,s,PLATFORM_KEY_V); CHECK(!strcmp(s,"è🙂"));
  app_press(&app,s,PLATFORM_KEY_Z); CHECK(!*s);
  app_press(&app,s,PLATFORM_KEY_Y); CHECK(!strcmp(s,"è🙂"));
  CHECK(ui_context_destroy(c)); input_keyboard_unregister(app.window); destroy_window(app.window);
  puts("APP text editing: PASS (headless platform clipboard, callback collector, cut/paste/undo/redo)");
  return 0;
}
