#include "app/application.h"
#include "platform/window.h"
#include "ui_reference_scene.h"
#include <stdlib.h>
#include <stdio.h>
void app_draw(APP *app) {
  static UIReferenceScene scene;
  static bool initialized;
  static bool navigation,editing,layout,color,basic;
  static int frame;
  if(!initialized) {
    ui_reference_init(&scene); initialized=true;
    basic=getenv("APP_REFERENCE_BASIC_SMOKE")!=NULL; scene.basic_dialog=basic;
    color=getenv("APP_REFERENCE_COLOR_SMOKE")!=NULL; scene.options=color;
    layout=getenv("APP_REFERENCE_LAYOUT_SMOKE")!=NULL; scene.layout_demo=layout;
    navigation=getenv("APP_REFERENCE_NAV_SMOKE")!=NULL;
    editing=getenv("APP_REFERENCE_EDIT_SMOKE")!=NULL;
    if(navigation||editing) scene.text[0]=0;
  }
  ui_reference_draw(&scene,&app->renderer.font,VIRTUAL_W,VIRTUAL_H,".");
  if(navigation||editing||layout||color||basic) {
    /* Feed the registered platform callbacks for the next host frame. This
     * exercises APP's collector without sending input to the desktop. */
    InputKeyEvent event;
    if(basic?ui_reference_basic_events(frame+1,&event):color?ui_reference_color_events(frame+1,&event):layout?ui_reference_layout_events(frame+1,&event):editing?ui_reference_editing_events(frame+1,&event):ui_reference_navigation_events(frame+1,&event)) {
      if(event.kind==INPUT_CHARACTER) {
        PlatformCharCallback callback=platform_set_char_callback(app->window,NULL);
        platform_set_char_callback(app->window,callback);
        if(callback) callback(app->window,event.codepoint);
      } else {
        PlatformKeyCallback callback=platform_set_key_callback(app->window,NULL);
        platform_set_key_callback(app->window,callback);
        if(callback) callback(app->window,event.key,0,event.action,event.mods);
      }
    }
  }
  if((navigation||editing||layout||color||basic) && frame++==30) {
    bool passed=basic?ui_reference_basic_result(&scene):color?ui_reference_color_result(&scene):layout?ui_reference_layout_result(&scene):editing?ui_reference_editing_result(&scene):ui_reference_navigation_result(&scene);
    printf("Vulkan %s: %s focus=%s clicks=%d choice=%d value=%g\n",
           basic?"basic":color?"color":layout?"layout":editing?"editing":"navigation",passed?"PASS":"FAIL",ui_keyboard_focus_id(),scene.clicks,scene.choice,scene.value);
    if(!passed) app->error=(AppError){APP_ERROR_STATE,"navigation smoke mismatch"};
    platform_window_set_should_close(app->window,true);
  }
}
