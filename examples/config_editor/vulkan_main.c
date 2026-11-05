#include "view.h"
#include "app/application.h"
#include "platform/window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  ConfigEditor editor;
  bool smoke, smoke_finished;
  int frame;
  char last_navigation[256];
} EditorApplication;
/* Make keeps its historical resource layout. The external CMake consumer
 * deploys resources beside the executable through the SDK helper. */
#ifndef CONFIG_EDITOR_DEFAULT_RESOURCES
#define CONFIG_EDITOR_DEFAULT_RESOURCES NULL
#endif
static bool close_requested(APP *app, void *user_data) {
  (void)app;
  EditorApplication *state=user_data;
  if(state->smoke_finished) return true;
  config_editor_request(&state->editor,CONFIG_ACTION_CLOSE);
  return state->editor.close_ready;
}
static void draw(APP *app, void *user_data) {
  EditorApplication *state=user_data;
  ConfigEditor *editor=&state->editor;
  if(state->smoke && state->frame==12) app_request_close(app);
  config_editor_draw(editor,&app->renderer.font,VIRTUAL_W,VIRTUAL_H);
  /* Opt-in diagnostics for owned-window automation. Text editing focus and
   * keyboard navigation are different; query the public navigation API. */
  if(getenv("UI_INPUT_DEBUG")) {
    const char *id=ui_keyboard_focus_id();
    if(!id || !*id) id="-";
    if(strcmp(state->last_navigation,id)) {
      snprintf(state->last_navigation,sizeof(state->last_navigation),"%s",id);
      printf("[EDITOR NAV] focus=%s\n",id);
    }
  }
  if(editor->close_ready) app_request_close(app);
  if(state->smoke) {
    InputKeyEvent event;
    if(config_editor_smoke_event(state->frame+1,&event)) {
      if(event.kind==INPUT_CHARACTER) {
        PlatformCharCallback callback=platform_set_char_callback(app->window,NULL);
        platform_set_char_callback(app->window,callback); if(callback) callback(app->window,event.codepoint);
      } else {
        PlatformKeyCallback callback=platform_set_key_callback(app->window,NULL);
        platform_set_key_callback(app->window,callback); if(callback) callback(app->window,event.key,0,event.action,event.mods);
      }
    }
    if(state->frame++==30) {
      bool ok=config_editor_smoke_result(editor); printf("Config editor Vulkan: %s\n",ok?"PASS":"FAIL");
      if(!ok) app->error=(AppError){APP_ERROR_STATE,"config editor smoke"};
      state->smoke_finished=true;
      app_request_close(app);
    }
  }
}
int main(int argc,char **argv) {
  EditorApplication state={0};
  ConfigEditor *editor=&state.editor;
  const AppCallbacks callbacks={.draw=draw,.close_requested=close_requested};
  const char *root=".",*open=NULL,*resources=CONFIG_EDITOR_DEFAULT_RESOURCES;
  for(int i=1;i<argc;++i) {
    if(!strcmp(argv[i],"--root") && i+1<argc) root=argv[++i];
    else if(!strcmp(argv[i],"--open") && i+1<argc) open=argv[++i];
    else if(!strcmp(argv[i],"--smoke")) state.smoke=true;
    else if(!strcmp(argv[i],"--resources") && i+1<argc) resources=argv[++i];
    else { fprintf(stderr,"Usage: %s [--root dir] [--open file] [--resources dir] [--smoke]\n",argv[0]); return 2; }
  }
  if(!config_editor_init(editor,root)) { fprintf(stderr,"%s\n",editor->message); return 1; }
  if(open) { config_editor_request(editor,CONFIG_ACTION_OPEN); if(!config_editor_choose(editor,open)) { fprintf(stderr,"%s\n",editor->message); return 1; } }
  APP *app=calloc(1,sizeof(*app)); if(!app) return 1;
  app->title="Editor di configurazione";
  app->resources.root_directory=resources;
  if(!app_init(app)) { fprintf(stderr,"Initialization: %s\n",app->error.stage); app_shutdown(app); free(app); return 1; }
  app_run(app,&callbacks,&state);
  app_shutdown(app);
  int result=app->error.code?1:0;
  if(result) fprintf(stderr,"Editor: %s\n",app->error.stage);
  free(app); return result;
}
