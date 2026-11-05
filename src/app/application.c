#include "app/application.h"
#include "platform/window.h"
#include "platform/paths.h"
#include "input/mouse.h"
#include "input/scroll.h"
#include "input/keyboard.h"
#include "ui/ui.h"
#include "ui/app_adapter.h"
#include "render/text_renderer.h"
#include "log/log.h"
#include "platform/window.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
float g_virtualWidth = 1280.0f;
float g_virtualHeight = 800.0f;

/* I metadati del font vengono validati prima di creare risorse GPU.
 * Le callback input si installano solo dopo che gfx_init ha creato la finestra. */
bool app_init(APP *app) {
  if (!app) return false;
  if (app->initialized || app->ui || app->context || app->renderer.gpu || app->window) {
    app->error = (AppError){APP_ERROR_STATE, "application already initialized"};
    return false;
  }
  if (app->settings.video.width <= 0) app->settings.video.width = 1280;
  if (app->settings.video.height <= 0) app->settings.video.height = 800;
  if (!app->title) app->title = "Graphics App Base";
  char path[APP_PATH_MAX];
  if (!app_resource_path(app, APP_RESOURCE_FONT_METADATA, path, sizeof(path)) ||
      !font_load(&app->renderer.font, path,
                 app->resources.atlas_width ? app->resources.atlas_width : 1024,
                 app->resources.atlas_height ? app->resources.atlas_height : 1024)) {
    app->error = (AppError){APP_ERROR_RESOURCE, "font metadata"};
    return false;
  }
  if (!gfx_init(app)) return false;
  app->ui=ui_context_create();
  if (!app->ui) {
    app->error=(AppError){APP_ERROR_MEMORY,"UI context"};
    gfx_shutdown(app);
    return false;
  }
  if (!ui_bind_app_renderer(app)) {
    app->error=(AppError){APP_ERROR_STATE,"UI renderer binding"};
    app_shutdown(app);
    return false;
  }
  input_scroll_register(app->window);
  input_keyboard_register(app->window);
  input_mouse_register(app->window);
  app->initialized = true;
  return true;
}

/* Scollegare i gestori prima di distruggere la finestra: unregister deve ancora
 * poter ripristinare le callback. gfx_shutdown conserva la diagnostica originale. */
void app_shutdown(APP *app) {
  if (!app) return;
  if (app->running) {
    app->error=(AppError){APP_ERROR_STATE,"shutdown during application loop"};
    return;
  }
  if (!ui_context_destroy(app->ui)) {
    if (!app->error.code) app->error=(AppError){APP_ERROR_STATE,"shutdown during UI frame"};
    return;
  }
  app->ui=NULL;
  if (app) {
    input_mouse_unregister(app->window);
    input_keyboard_unregister(app->window);
    input_scroll_unregister(app->window);
  }
  gfx_shutdown(app);
}

static void app_autostart_frame_hook(APP *app) {
  const char *seconds = getenv("APP_AUTOSTART_SECONDS");
  if (seconds && atof(seconds) > 0.0 && app->time.totalTime >= atof(seconds))
    platform_window_set_should_close(app->window, PLATFORM_TRUE);
}

void app_request_close(APP *app) {
  if (app && app->initialized && app->window)
    platform_window_set_should_close(app->window, PLATFORM_TRUE);
}

static bool app_accept_close(APP *app, const AppCallbacks *callbacks, void *user_data) {
  if (!platform_window_should_close(app->window)) return false;
  /* Consume before notifying: a veto must permit subsequent UI frames. */
  platform_window_set_should_close(app->window, PLATFORM_FALSE);
  bool accept = !callbacks->close_requested || callbacks->close_requested(app, user_data);
  platform_window_set_should_close(app->window, accept ? PLATFORM_TRUE : PLATFORM_FALSE);
  return accept;
}

void app_run(APP *app, const AppCallbacks *callbacks, void *user_data) {
  if (!app || !app->initialized || app->error.code) return;
  if (app->running || !callbacks || !callbacks->draw) {
    app->error=(AppError){APP_ERROR_STATE,"invalid application callbacks or nested loop"};
    return;
  }
  const AppCallbacks handlers = *callbacks;
  UIContext *previous_ui=ui_context_get_current();
  if (!app->ui || !ui_context_set_current(app->ui)) {
    app->error=(AppError){APP_ERROR_STATE,"UI context selection"};
    return;
  }
  app->running = true;
  double lastTime = platform_time();
  while (!app->error.code) {
    if (app_accept_close(app, &handlers, user_data) || app->error.code) break;
    double currentTime = platform_time();
    double deltaTime = currentTime - lastTime;
    if (deltaTime < 0.0) deltaTime = 0.0;
    // Evita un salto enorme nei widget animati dopo una pausa o un breakpoint.
    if (deltaTime > 0.25) deltaTime = 0.25;
    lastTime = currentTime;
    app->time.dt = (float)deltaTime;
    app->time.totalTime += app->time.dt;
    // Pubblica gli eventi nativi prima che ui_begin_frame congeli il batch input.
    platform_poll_events();
    if (app->error.code || app_accept_close(app, &handlers, user_data) || app->error.code) break;
    int width = 0, height = 0;
    platform_framebuffer_size(app->window, &width, &height);
    // Una superficie senza pixel non puo essere presentata: attendi nuovi eventi.
    if (width == 0 || height == 0) { platform_wait_events(); continue; }
    if (app->framebuffer_resized) {
      gfx_recreate_surface(app);
      app->framebuffer_resized = false;
      if (app->error.code || app_accept_close(app, &handlers, user_data) || app->error.code) break;
    }
    // Non riscrivere i buffer dello slot finche la GPU non ha finito di usarli.
    if (!wait_for_current_frame(app)) break;
    Point p;
    bool mouse_cancelled;
    bool mouse_down = input_mouse_frame(app, &p, &mouse_cancelled);
    if (mouse_cancelled) ui_release_input_capture();
    ui_begin_frame(app, (float)p.x, (float)p.y, mouse_down, app->time.dt);
    text_begin(app);
    // Punto di estensione: l'app compone i widget, il framework gestisce il frame.
    handlers.draw(app, user_data);
    // I popup vengono disegnati dopo i controlli per non restare coperti da essi.
    if (!app->error.code) ui_draw_dropdown_popups();
    ui_end_frame();
    if (app->error.code || app_accept_close(app, &handlers, user_data) || app->error.code) break;
    // text_end prepara la geometria; draw_frame registra, invia e presenta.
    // Dopo un errore non si avvia un nuovo frame: alcune fence possono essere non segnalate.
    text_end(app);
    if (app->error.code || !draw_frame(app)) break;
    app_autostart_frame_hook(app);
  }
  app->running = false;
  (void)ui_context_set_current(previous_ui);
}
