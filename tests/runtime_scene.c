/* Interactive regression scene; uses the production application loop/backend. */
#include "app/application.h"
#include "platform/paths.h"
#include "render/text_renderer.h"
#include "render/vsync_settings.h"
#include "ui/ui.h"
#include "platform/window.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static PlatformKeyCallback previous_key_observer;
static void observe_key(PlatformWindow *window, int key, int scancode, int action, int mods) {
  if (key==PLATFORM_KEY_Z) printf("RUNTIME key=Z action=%d\n",action);
  else printf("RUNTIME key=%d action=%d\n",key,action);
  fflush(stdout);
  if (previous_key_observer) previous_key_observer(window,key,scancode,action,mods);
}
static void iconified(PlatformWindow *window, int value) {
  printf("RUNTIME iconified=%d\n", value); fflush(stdout);
}
void app_draw(APP *app) {
  /* Opt-in runtime checks without synthetic desktop input. */
  if (getenv("APP_RUNTIME_PASSIVE")) {
    static int phase;
    if (phase < 2 && app->time.totalTime >= (phase ? 5.0f : 2.0f)) {
      bool enabled = phase != 0;
      vsync_toggle_and_apply(app, enabled);
      printf("RUNTIME vsync=%d\n", enabled);
      ++phase;
      if (app->error.code) return;
    }
  }
  static bool ready;
  static char text[128]="initial", previous[128]="";
  static bool focused;
  static float value=0.5f;
  static int integer=0;
  Font *font=&app->renderer.font;
  if (!ready) {
    platform_set_iconify_callback(app->window,iconified);
    previous_key_observer=platform_set_key_callback(app->window,observe_key);
    if (getenv("APP_RUNTIME_UNICODE_DEMO")) strcpy(text,"Caffè, città, più: 20 €");
    ready=true; puts("RUNTIME ready");
  }
  UITheme t=ui_get_theme();
  draw_quad(app,0,0,VIRTUAL_W,VIRTUAL_H,t.editor_background.r,t.editor_background.g,t.editor_background.b);
  draw_text_colored(app,font,"Vulkan runtime validation",32,45,24,t.text.r,t.text.g,t.text.b);
  UIButtonStyle style={.min_width=240,.max_width=240,.min_height=34};
  ui_text_input("runtime/text",font,ANCHOR_TOP_LEFT,0,18,32,80,600,text,sizeof(text),&style);
  bool now=ui_text_input_is_focused("runtime/text");
  if(strcmp(previous,text) || now!=focused) {
    printf("RUNTIME text=%s focus=%d\n",text,now); strcpy(previous,text); focused=now;
  }
  UIPropertyLayout p=ui_property_layout_begin(32,140,600,1);
  if(ui_property_float(&p,"runtime/float",font,"Float",&value,-100,100))
    printf("RUNTIME float=%.9g\n",value);
  p=ui_property_layout_begin(32,200,600,1);
  if(ui_property_int(&p,"runtime/int",font,"Integer",&integer,INT_MIN,INT_MAX))
    printf("RUNTIME integer=%d\n",integer);
  if(ui_button_labeled("runtime/picker","Apri picker cartella",font,ANCHOR_TOP_LEFT,0,16,32,260,&style)) {
    char root[APP_PATH_MAX]; paths_engine_file(root,sizeof(root),"assets/fonts");
    UIFilePickerRequest request={.mode=UI_FILE_PICKER_DIRECTORY,.root_directory=root};
    ui_file_picker_open("runtime/picker-dialog","Runtime picker",&request);
    puts("RUNTIME picker=open");
  }
  if(ui_button_labeled("runtime/vsync","Cambia VSync",font,ANCHOR_TOP_LEFT,0,16,32,320,&style)) {
    bool enabled=!vsync_get_enabled(); vsync_toggle_and_apply(app,enabled);
    printf("RUNTIME vsync=%d\n",enabled);
  }
  char status[128]; snprintf(status,sizeof(status),"float=%.9g int=%d vsync=%d",value,integer,vsync_get_enabled());
  draw_text_colored(app,font,status,32,400,18,t.text.r,t.text.g,t.text.b);
  if (getenv("APP_RUNTIME_UNICODE_DEMO"))
    draw_text_colored(app,font,"à è é ì ò ù — Ω 中 🙂 (glifi mancanti: sostituzione)",32,450,18,t.text.r,t.text.g,t.text.b);
  char path[APP_PATH_MAX];
  if(ui_file_picker_draw(font,path,sizeof(path))) printf("RUNTIME picker_selected=%s\n",path);
  fflush(stdout);
}
