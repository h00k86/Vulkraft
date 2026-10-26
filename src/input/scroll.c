#include <GLFW/glfw3.h>
static float g_scrollAccum = 0.0f;

static void input_scroll_callback(GLFWwindow *w, double xoffset, double yoffset) {
  (void)w;
  (void)xoffset;
  g_scrollAccum += (float)yoffset;
}

void input_scroll_register(GLFWwindow *window) {
  if (window)
    glfwSetScrollCallback(window, input_scroll_callback);
}

float input_scroll_take(void) {
  float d = g_scrollAccum;
  g_scrollAccum = 0.0f;
  return d;
}
