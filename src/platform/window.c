

#include "platform/window.h"



#include "app/application.h"


GLFWmonitor *glfw_helper_get_window_monitor(GLFWwindow *window) {
  if (!window)
    return glfwGetPrimaryMonitor();

  GLFWmonitor *fullscreen_monitor = glfwGetWindowMonitor(window);
  if (fullscreen_monitor)
    return fullscreen_monitor;

  int window_x = 0, window_y = 0, window_w = 0, window_h = 0;
  glfwGetWindowPos(window, &window_x, &window_y);
  glfwGetWindowSize(window, &window_w, &window_h);

  int monitor_count = 0;
  GLFWmonitor **monitors = glfwGetMonitors(&monitor_count);
  GLFWmonitor *best_monitor = NULL;
  long long best_area = 0;
  for (int i = 0; monitors && i < monitor_count; ++i) {
    int monitor_x = 0, monitor_y = 0;
    glfwGetMonitorPos(monitors[i], &monitor_x, &monitor_y);
    const GLFWvidmode *mode = glfwGetVideoMode(monitors[i]);
    if (!mode)
      continue;

    int left = window_x > monitor_x ? window_x : monitor_x;
    int top = window_y > monitor_y ? window_y : monitor_y;
    int right = (window_x + window_w) < (monitor_x + mode->width)
                    ? (window_x + window_w)
                    : (monitor_x + mode->width);
    int bottom = (window_y + window_h) < (monitor_y + mode->height)
                     ? (window_y + window_h)
                     : (monitor_y + mode->height);
    long long area = right > left && bottom > top
                         ? (long long)(right - left) * (bottom - top)
                         : 0;
    if (area > best_area) {
      best_area = area;
      best_monitor = monitors[i];
    }
  }

  return best_monitor ? best_monitor : glfwGetPrimaryMonitor();
}

int glfw_helper_get_monitor_index(GLFWmonitor *monitor) {
  int count = 0;
  GLFWmonitor **monitors = glfwGetMonitors(&count);
  for (int i = 0; monitors && i < count; ++i) {
    if (monitors[i] == monitor)
      return i;
  }
  return -1;
}

static const GLFWvidmode *find_supported_video_mode(GLFWmonitor *monitor,
                                                     int width, int height) {
  if (!monitor)
    return NULL;

  int count = 0;
  const GLFWvidmode *modes = glfwGetVideoModes(monitor, &count);
  const GLFWvidmode *desktop = glfwGetVideoMode(monitor);
  const GLFWvidmode *fallback = NULL;
  for (int i = 0; modes && i < count; ++i) {
    if (modes[i].width != width || modes[i].height != height)
      continue;
    if (desktop && modes[i].refreshRate == desktop->refreshRate)
      return &modes[i];
    if (!fallback)
      fallback = &modes[i];
  }
  return fallback;
}

/* P3-B: window size cache.
 *
 * get_mouse_coordinate() used to call glfwGetWindowSize() every frame to map
 * pixel cursor coordinates into virtual space. On X11 that is
 * XGetWindowAttributes -- a SYNCHRONOUS request/reply round trip to the
 * display server. Measured over 19787 real frames it cost a median 0.89 ms
 * and 10.18% of ALL frame time, with individual round trips reaching 94 ms
 * while the thread sat off-CPU waiting for the reply.
 *
 * The size only changes when the window is resized, and GLFW will tell us.
 * So it is cached here and refreshed from the callback; the per-frame round
 * trip is gone. Values are ints written and read on the main thread only. */
static int g_cached_window_w = 0, g_cached_window_h = 0;

void glfw_helper_window_size(int *outWidth, int *outHeight) {
  if (outWidth) *outWidth = g_cached_window_w;
  if (outHeight) *outHeight = g_cached_window_h;
}

static void windowSizeCallback(GLFWwindow *window, int width, int height) {
  (void)window;
  g_cached_window_w = width;
  g_cached_window_h = height;
}

void framebufferResizeCallback(GLFWwindow *window, int width, int height) {
  // Recuperiamo il puntatore alla struct APP che abbiamo salvato nella
  // finestra
  APP *app = (APP *)glfwGetWindowUserPointer(window);
  app->framebuffer_resized = true;
}

GLFWwindow *create_window(APP *app) {

  if (!glfwInit()) return NULL;

  // dont need to create opengl context
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  //
  // In autostart editor (hook di sviluppo per screenshot/regression
  // visiva) la finestra è a dimensione fissa: i tiling WM re-impaginano le
  // finestre resizable nella loro griglia, comprimendo la UI virtuale
  // 1920x1080 fino all'illeggibile. Uso normale: invariato (resizable).
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  int width = app->settings.video.width;
  int height = app->settings.video.height;
  GLFWmonitor *monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode *mode = monitor ? glfwGetVideoMode(monitor) : NULL;
  GLFWmonitor *fullscreenMonitor = NULL;

  if (app->settings.video.window_mode == WINDOW_MODE_FULLSCREEN && monitor) {
    fullscreenMonitor = monitor;
    const GLFWvidmode *selected =
        find_supported_video_mode(monitor, width, height);
    if (!selected)
      selected = mode;
    if (selected) {
      width = selected->width;
      height = selected->height;
      app->settings.video.width = width;
      app->settings.video.height = height;
    }
  } else if (app->settings.video.window_mode == WINDOW_MODE_BORDERLESS &&
             mode) {
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    width = mode->width;
    height = mode->height;
  }

  GLFWwindow *window = glfwCreateWindow(width, height, app->title,
                                        fullscreenMonitor, NULL);
  if (!window)
    return NULL;
  if (app->settings.video.window_mode == WINDOW_MODE_BORDERLESS && monitor) {
    int monitor_x = 0, monitor_y = 0;
    glfwGetMonitorPos(monitor, &monitor_x, &monitor_y);
    glfwSetWindowPos(window, monitor_x, monitor_y);
  }
  glfwSetWindowUserPointer(window, app);
  glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
  /* Seed the cache once, then keep it current from the callback. */
  glfwGetWindowSize(window, &g_cached_window_w, &g_cached_window_h);
  glfwSetWindowSizeCallback(window, windowSizeCallback);

  return window;
}

void destroy_window(GLFWwindow *window) {

  glfwDestroyWindow(window);
  glfwTerminate();
}
