/* Same Vulkan regression scene routed through the deferred UI backend. */
#define app_draw immediate_scene
#include "runtime_scene.c"
#undef app_draw
#include "ui/draw_list.h"
#include "ui/app_adapter.h"
static UIDrawList *scene_list;
void app_draw(APP *app) {
  if (!ui_draw_list_begin(scene_list)) {
    app->error=(AppError){APP_ERROR_STATE,"draw list begin"}; return;
  }
  immediate_scene(app);
  /* Record deferred popups before replay; the normal loop's second pass is inert. */
  ui_draw_dropdown_popups();
  if (!ui_draw_list_replay(scene_list))
    app->error=(AppError){APP_ERROR_STATE,"draw list replay"};
}
static void draw_list_loop(APP *app) {
  scene_list=ui_draw_list_create(ui_app_renderer(),app);
  if (!scene_list || !ui_context_set_renderer(app->ui,ui_draw_list_renderer(),scene_list)) {
    app->error=(AppError){APP_ERROR_MEMORY,"draw list setup"};
    ui_draw_list_destroy(scene_list); scene_list=NULL; return;
  }
  app_loop(app);
  /* Last frame was submitted. Restore binding before destroying the borrowed user. */
  if (!ui_bind_app_renderer(app)) app->error=(AppError){APP_ERROR_STATE,"draw list detach"};
  ui_draw_list_destroy(scene_list); scene_list=NULL;
}
#define app_loop draw_list_loop
#include "../examples/main.c"
#undef app_loop
