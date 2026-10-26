#include "app/application.h"
#include "platform/window.h"
#include "platform/paths.h"
#include "input/mouse.h"
#include "ui/ui.h"
#include "render/text_renderer.h"
#include "log/log.h"
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
float g_virtualWidth = 1280.0f;
float g_virtualHeight = 800.0f;
void input_scroll_register(GLFWwindow *window);
float input_scroll_take(void);

void app_init(APP *app) {
  if (app->settings.video.width <= 0) app->settings.video.width = 1280;
  if (app->settings.video.height <= 0) app->settings.video.height = 800;
  if (!app->title) app->title = "Graphics App Base";
  gfx_init(app);
  char path[APP_PATH_MAX];
  paths_engine_file(path, sizeof(path), "assets/fonts/font2.json");
  if (!font_load(&app->renderer.font, path, 512, 512)) {
    LOG_ERROR("Font metadata missing: %s", path); exit(EXIT_FAILURE);
  }
  ui_set_theme(ui_theme_dark());
  input_scroll_register(app->window);
}

void app_shutdown(APP *app) {
  gfx_shutdown(app);
}

static void app_autostart_frame_hook(APP *app) {
  const char *seconds = getenv("APP_AUTOSTART_SECONDS");
  if (seconds && atof(seconds) > 0.0 && app->time.totalTime >= atof(seconds))
    glfwSetWindowShouldClose(app->window, GLFW_TRUE);
}

void app_loop(APP *app) {
  double lastTime = glfwGetTime();
  while (!glfwWindowShouldClose(app->window)) {
    double currentTime = glfwGetTime();
    double deltaTime = currentTime - lastTime;
    if (deltaTime < 0.0) deltaTime = 0.0;
    if (deltaTime > 0.25) deltaTime = 0.25;
    lastTime = currentTime;
    app->time.dt = (float)deltaTime;
    app->time.totalTime += app->time.dt;
    glfwPollEvents();
    int width = 0, height = 0;
    glfwGetFramebufferSize(app->window, &width, &height);
    if (width == 0 || height == 0) { glfwWaitEvents(); continue; }
    if (app->framebuffer_resized) {
      gfx_recreate_surface(app);
      app->framebuffer_resized = false;
    }
    wait_for_current_frame(app);
    Point p = get_mouse_coordinate(app);
    bool mouse_down = glfwGetMouseButton(app->window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    ui_begin_frame(app, (float)p.x, (float)p.y, mouse_down, app->time.dt);
    ui_set_scroll_delta(input_scroll_take());
    text_begin(app);
    app_draw(app);
    ui_draw_dropdown_popups(app);
    ui_end_frame();
    text_end(app);
    draw_frame(app);
    app_autostart_frame_hook(app);
  }
}
