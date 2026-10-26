
#include "input/mouse.h"
#include "app/application.h"

#include "platform/window.h"

Point get_mouse_coordinate(APP *app) {
  Point raw;
  glfwGetCursorPos(app->window, &raw.x, &raw.y);
  int win_w, win_h;
  glfw_helper_window_size(&win_w, &win_h);
  Point p = raw;
  if (win_w > 0 && win_h > 0) {
    p.x = raw.x * (double)VIRTUAL_W / (double)win_w;
    p.y = raw.y * (double)VIRTUAL_H / (double)win_h;
  }
  return p;
}
