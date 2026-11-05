/* Unico adattatore finestra/eventi della piattaforma nativa. Include e handle
 * specifici restano qui; il ponte Vulkan usa soltanto l'accessore privato. */
#include "glfw_private.h"
#include <stdlib.h>
/* Il wrapper possiede native; user appartiene al chiamante. Il puntatore
 * utente della finestra nativa punta al wrapper, non direttamente ad APP. */
struct PlatformWindow {
  GLFWwindow *native;
  void *user;
  int width,height;
  PlatformEventCallback event;
  PlatformKeyCallback key;
  PlatformCharCallback character;
  PlatformFocusCallback focus;
  PlatformScrollCallback scroll;
  PlatformResizeCallback resize;
  PlatformIconifyCallback iconify;
};
/* Il conteggio evita di terminare la libreria mentre esistono altre finestre.
 * Questo non rende multi-istanza lo stato globale della UI e dell'input. */
static unsigned window_count;
GLFWwindow *platform_glfw_native(PlatformWindow *w) { return w ? w->native : NULL; }
/* La traduzione e esplicita: i codici del framework non devono dipendere
 * dai valori numerici scelti dalla libreria nativa. */
static int translate_key(int native) {
  switch(native) {
    case GLFW_KEY_SPACE: return PLATFORM_KEY_SPACE;
    case GLFW_KEY_APOSTROPHE: return PLATFORM_KEY_APOSTROPHE;
    case GLFW_KEY_COMMA: return PLATFORM_KEY_COMMA;
    case GLFW_KEY_MINUS: return PLATFORM_KEY_MINUS;
    case GLFW_KEY_PERIOD: return PLATFORM_KEY_PERIOD;
    case GLFW_KEY_SLASH: return PLATFORM_KEY_SLASH;
    case GLFW_KEY_0: return PLATFORM_KEY_0;
    case GLFW_KEY_1: return PLATFORM_KEY_1;
    case GLFW_KEY_2: return PLATFORM_KEY_2;
    case GLFW_KEY_3: return PLATFORM_KEY_3;
    case GLFW_KEY_4: return PLATFORM_KEY_4;
    case GLFW_KEY_5: return PLATFORM_KEY_5;
    case GLFW_KEY_6: return PLATFORM_KEY_6;
    case GLFW_KEY_7: return PLATFORM_KEY_7;
    case GLFW_KEY_8: return PLATFORM_KEY_8;
    case GLFW_KEY_9: return PLATFORM_KEY_9;
    case GLFW_KEY_SEMICOLON: return PLATFORM_KEY_SEMICOLON;
    case GLFW_KEY_EQUAL: return PLATFORM_KEY_EQUAL;
    case GLFW_KEY_A: return PLATFORM_KEY_A;
    case GLFW_KEY_B: return PLATFORM_KEY_B;
    case GLFW_KEY_C: return PLATFORM_KEY_C;
    case GLFW_KEY_D: return PLATFORM_KEY_D;
    case GLFW_KEY_E: return PLATFORM_KEY_E;
    case GLFW_KEY_F: return PLATFORM_KEY_F;
    case GLFW_KEY_G: return PLATFORM_KEY_G;
    case GLFW_KEY_H: return PLATFORM_KEY_H;
    case GLFW_KEY_I: return PLATFORM_KEY_I;
    case GLFW_KEY_J: return PLATFORM_KEY_J;
    case GLFW_KEY_K: return PLATFORM_KEY_K;
    case GLFW_KEY_L: return PLATFORM_KEY_L;
    case GLFW_KEY_M: return PLATFORM_KEY_M;
    case GLFW_KEY_N: return PLATFORM_KEY_N;
    case GLFW_KEY_O: return PLATFORM_KEY_O;
    case GLFW_KEY_P: return PLATFORM_KEY_P;
    case GLFW_KEY_Q: return PLATFORM_KEY_Q;
    case GLFW_KEY_R: return PLATFORM_KEY_R;
    case GLFW_KEY_S: return PLATFORM_KEY_S;
    case GLFW_KEY_T: return PLATFORM_KEY_T;
    case GLFW_KEY_U: return PLATFORM_KEY_U;
    case GLFW_KEY_V: return PLATFORM_KEY_V;
    case GLFW_KEY_W: return PLATFORM_KEY_W;
    case GLFW_KEY_X: return PLATFORM_KEY_X;
    case GLFW_KEY_Y: return PLATFORM_KEY_Y;
    case GLFW_KEY_Z: return PLATFORM_KEY_Z;
    case GLFW_KEY_LEFT_BRACKET: return PLATFORM_KEY_LEFT_BRACKET;
    case GLFW_KEY_BACKSLASH: return PLATFORM_KEY_BACKSLASH;
    case GLFW_KEY_RIGHT_BRACKET: return PLATFORM_KEY_RIGHT_BRACKET;
    case GLFW_KEY_GRAVE_ACCENT: return PLATFORM_KEY_GRAVE_ACCENT;
    case GLFW_KEY_WORLD_1: return PLATFORM_KEY_WORLD_1;
    case GLFW_KEY_WORLD_2: return PLATFORM_KEY_WORLD_2;
    case GLFW_KEY_ESCAPE: return PLATFORM_KEY_ESCAPE;
    case GLFW_KEY_ENTER: return PLATFORM_KEY_ENTER;
    case GLFW_KEY_TAB: return PLATFORM_KEY_TAB;
    case GLFW_KEY_BACKSPACE: return PLATFORM_KEY_BACKSPACE;
    case GLFW_KEY_INSERT: return PLATFORM_KEY_INSERT;
    case GLFW_KEY_DELETE: return PLATFORM_KEY_DELETE;
    case GLFW_KEY_RIGHT: return PLATFORM_KEY_RIGHT;
    case GLFW_KEY_LEFT: return PLATFORM_KEY_LEFT;
    case GLFW_KEY_DOWN: return PLATFORM_KEY_DOWN;
    case GLFW_KEY_UP: return PLATFORM_KEY_UP;
    case GLFW_KEY_PAGE_UP: return PLATFORM_KEY_PAGE_UP;
    case GLFW_KEY_PAGE_DOWN: return PLATFORM_KEY_PAGE_DOWN;
    case GLFW_KEY_HOME: return PLATFORM_KEY_HOME;
    case GLFW_KEY_END: return PLATFORM_KEY_END;
    case GLFW_KEY_CAPS_LOCK: return PLATFORM_KEY_CAPS_LOCK;
    case GLFW_KEY_SCROLL_LOCK: return PLATFORM_KEY_SCROLL_LOCK;
    case GLFW_KEY_NUM_LOCK: return PLATFORM_KEY_NUM_LOCK;
    case GLFW_KEY_PRINT_SCREEN: return PLATFORM_KEY_PRINT_SCREEN;
    case GLFW_KEY_PAUSE: return PLATFORM_KEY_PAUSE;
    case GLFW_KEY_F1: return PLATFORM_KEY_F1;
    case GLFW_KEY_F2: return PLATFORM_KEY_F2;
    case GLFW_KEY_F3: return PLATFORM_KEY_F3;
    case GLFW_KEY_F4: return PLATFORM_KEY_F4;
    case GLFW_KEY_F5: return PLATFORM_KEY_F5;
    case GLFW_KEY_F6: return PLATFORM_KEY_F6;
    case GLFW_KEY_F7: return PLATFORM_KEY_F7;
    case GLFW_KEY_F8: return PLATFORM_KEY_F8;
    case GLFW_KEY_F9: return PLATFORM_KEY_F9;
    case GLFW_KEY_F10: return PLATFORM_KEY_F10;
    case GLFW_KEY_F11: return PLATFORM_KEY_F11;
    case GLFW_KEY_F12: return PLATFORM_KEY_F12;
    case GLFW_KEY_F13: return PLATFORM_KEY_F13;
    case GLFW_KEY_F14: return PLATFORM_KEY_F14;
    case GLFW_KEY_F15: return PLATFORM_KEY_F15;
    case GLFW_KEY_F16: return PLATFORM_KEY_F16;
    case GLFW_KEY_F17: return PLATFORM_KEY_F17;
    case GLFW_KEY_F18: return PLATFORM_KEY_F18;
    case GLFW_KEY_F19: return PLATFORM_KEY_F19;
    case GLFW_KEY_F20: return PLATFORM_KEY_F20;
    case GLFW_KEY_F21: return PLATFORM_KEY_F21;
    case GLFW_KEY_F22: return PLATFORM_KEY_F22;
    case GLFW_KEY_F23: return PLATFORM_KEY_F23;
    case GLFW_KEY_F24: return PLATFORM_KEY_F24;
    case GLFW_KEY_F25: return PLATFORM_KEY_F25;
    case GLFW_KEY_KP_0: return PLATFORM_KEY_KP_0;
    case GLFW_KEY_KP_1: return PLATFORM_KEY_KP_1;
    case GLFW_KEY_KP_2: return PLATFORM_KEY_KP_2;
    case GLFW_KEY_KP_3: return PLATFORM_KEY_KP_3;
    case GLFW_KEY_KP_4: return PLATFORM_KEY_KP_4;
    case GLFW_KEY_KP_5: return PLATFORM_KEY_KP_5;
    case GLFW_KEY_KP_6: return PLATFORM_KEY_KP_6;
    case GLFW_KEY_KP_7: return PLATFORM_KEY_KP_7;
    case GLFW_KEY_KP_8: return PLATFORM_KEY_KP_8;
    case GLFW_KEY_KP_9: return PLATFORM_KEY_KP_9;
    case GLFW_KEY_KP_DECIMAL: return PLATFORM_KEY_KP_DECIMAL;
    case GLFW_KEY_KP_DIVIDE: return PLATFORM_KEY_KP_DIVIDE;
    case GLFW_KEY_KP_MULTIPLY: return PLATFORM_KEY_KP_MULTIPLY;
    case GLFW_KEY_KP_SUBTRACT: return PLATFORM_KEY_KP_SUBTRACT;
    case GLFW_KEY_KP_ADD: return PLATFORM_KEY_KP_ADD;
    case GLFW_KEY_KP_ENTER: return PLATFORM_KEY_KP_ENTER;
    case GLFW_KEY_KP_EQUAL: return PLATFORM_KEY_KP_EQUAL;
    case GLFW_KEY_LEFT_SHIFT: return PLATFORM_KEY_LEFT_SHIFT;
    case GLFW_KEY_LEFT_CONTROL: return PLATFORM_KEY_LEFT_CONTROL;
    case GLFW_KEY_LEFT_ALT: return PLATFORM_KEY_LEFT_ALT;
    case GLFW_KEY_LEFT_SUPER: return PLATFORM_KEY_LEFT_SUPER;
    case GLFW_KEY_RIGHT_SHIFT: return PLATFORM_KEY_RIGHT_SHIFT;
    case GLFW_KEY_RIGHT_CONTROL: return PLATFORM_KEY_RIGHT_CONTROL;
    case GLFW_KEY_RIGHT_ALT: return PLATFORM_KEY_RIGHT_ALT;
    case GLFW_KEY_RIGHT_SUPER: return PLATFORM_KEY_RIGHT_SUPER;
    case GLFW_KEY_MENU: return PLATFORM_KEY_MENU;
    default: return PLATFORM_KEY_UNKNOWN;
  }
}
static int native_key(int key) {
  switch(key) {
    case PLATFORM_KEY_SPACE: return GLFW_KEY_SPACE;
    case PLATFORM_KEY_APOSTROPHE: return GLFW_KEY_APOSTROPHE;
    case PLATFORM_KEY_COMMA: return GLFW_KEY_COMMA;
    case PLATFORM_KEY_MINUS: return GLFW_KEY_MINUS;
    case PLATFORM_KEY_PERIOD: return GLFW_KEY_PERIOD;
    case PLATFORM_KEY_SLASH: return GLFW_KEY_SLASH;
    case PLATFORM_KEY_0: return GLFW_KEY_0;
    case PLATFORM_KEY_1: return GLFW_KEY_1;
    case PLATFORM_KEY_2: return GLFW_KEY_2;
    case PLATFORM_KEY_3: return GLFW_KEY_3;
    case PLATFORM_KEY_4: return GLFW_KEY_4;
    case PLATFORM_KEY_5: return GLFW_KEY_5;
    case PLATFORM_KEY_6: return GLFW_KEY_6;
    case PLATFORM_KEY_7: return GLFW_KEY_7;
    case PLATFORM_KEY_8: return GLFW_KEY_8;
    case PLATFORM_KEY_9: return GLFW_KEY_9;
    case PLATFORM_KEY_SEMICOLON: return GLFW_KEY_SEMICOLON;
    case PLATFORM_KEY_EQUAL: return GLFW_KEY_EQUAL;
    case PLATFORM_KEY_A: return GLFW_KEY_A;
    case PLATFORM_KEY_B: return GLFW_KEY_B;
    case PLATFORM_KEY_C: return GLFW_KEY_C;
    case PLATFORM_KEY_D: return GLFW_KEY_D;
    case PLATFORM_KEY_E: return GLFW_KEY_E;
    case PLATFORM_KEY_F: return GLFW_KEY_F;
    case PLATFORM_KEY_G: return GLFW_KEY_G;
    case PLATFORM_KEY_H: return GLFW_KEY_H;
    case PLATFORM_KEY_I: return GLFW_KEY_I;
    case PLATFORM_KEY_J: return GLFW_KEY_J;
    case PLATFORM_KEY_K: return GLFW_KEY_K;
    case PLATFORM_KEY_L: return GLFW_KEY_L;
    case PLATFORM_KEY_M: return GLFW_KEY_M;
    case PLATFORM_KEY_N: return GLFW_KEY_N;
    case PLATFORM_KEY_O: return GLFW_KEY_O;
    case PLATFORM_KEY_P: return GLFW_KEY_P;
    case PLATFORM_KEY_Q: return GLFW_KEY_Q;
    case PLATFORM_KEY_R: return GLFW_KEY_R;
    case PLATFORM_KEY_S: return GLFW_KEY_S;
    case PLATFORM_KEY_T: return GLFW_KEY_T;
    case PLATFORM_KEY_U: return GLFW_KEY_U;
    case PLATFORM_KEY_V: return GLFW_KEY_V;
    case PLATFORM_KEY_W: return GLFW_KEY_W;
    case PLATFORM_KEY_X: return GLFW_KEY_X;
    case PLATFORM_KEY_Y: return GLFW_KEY_Y;
    case PLATFORM_KEY_Z: return GLFW_KEY_Z;
    case PLATFORM_KEY_LEFT_BRACKET: return GLFW_KEY_LEFT_BRACKET;
    case PLATFORM_KEY_BACKSLASH: return GLFW_KEY_BACKSLASH;
    case PLATFORM_KEY_RIGHT_BRACKET: return GLFW_KEY_RIGHT_BRACKET;
    case PLATFORM_KEY_GRAVE_ACCENT: return GLFW_KEY_GRAVE_ACCENT;
    case PLATFORM_KEY_WORLD_1: return GLFW_KEY_WORLD_1;
    case PLATFORM_KEY_WORLD_2: return GLFW_KEY_WORLD_2;
    case PLATFORM_KEY_ESCAPE: return GLFW_KEY_ESCAPE;
    case PLATFORM_KEY_ENTER: return GLFW_KEY_ENTER;
    case PLATFORM_KEY_TAB: return GLFW_KEY_TAB;
    case PLATFORM_KEY_BACKSPACE: return GLFW_KEY_BACKSPACE;
    case PLATFORM_KEY_INSERT: return GLFW_KEY_INSERT;
    case PLATFORM_KEY_DELETE: return GLFW_KEY_DELETE;
    case PLATFORM_KEY_RIGHT: return GLFW_KEY_RIGHT;
    case PLATFORM_KEY_LEFT: return GLFW_KEY_LEFT;
    case PLATFORM_KEY_DOWN: return GLFW_KEY_DOWN;
    case PLATFORM_KEY_UP: return GLFW_KEY_UP;
    case PLATFORM_KEY_PAGE_UP: return GLFW_KEY_PAGE_UP;
    case PLATFORM_KEY_PAGE_DOWN: return GLFW_KEY_PAGE_DOWN;
    case PLATFORM_KEY_HOME: return GLFW_KEY_HOME;
    case PLATFORM_KEY_END: return GLFW_KEY_END;
    case PLATFORM_KEY_CAPS_LOCK: return GLFW_KEY_CAPS_LOCK;
    case PLATFORM_KEY_SCROLL_LOCK: return GLFW_KEY_SCROLL_LOCK;
    case PLATFORM_KEY_NUM_LOCK: return GLFW_KEY_NUM_LOCK;
    case PLATFORM_KEY_PRINT_SCREEN: return GLFW_KEY_PRINT_SCREEN;
    case PLATFORM_KEY_PAUSE: return GLFW_KEY_PAUSE;
    case PLATFORM_KEY_F1: return GLFW_KEY_F1;
    case PLATFORM_KEY_F2: return GLFW_KEY_F2;
    case PLATFORM_KEY_F3: return GLFW_KEY_F3;
    case PLATFORM_KEY_F4: return GLFW_KEY_F4;
    case PLATFORM_KEY_F5: return GLFW_KEY_F5;
    case PLATFORM_KEY_F6: return GLFW_KEY_F6;
    case PLATFORM_KEY_F7: return GLFW_KEY_F7;
    case PLATFORM_KEY_F8: return GLFW_KEY_F8;
    case PLATFORM_KEY_F9: return GLFW_KEY_F9;
    case PLATFORM_KEY_F10: return GLFW_KEY_F10;
    case PLATFORM_KEY_F11: return GLFW_KEY_F11;
    case PLATFORM_KEY_F12: return GLFW_KEY_F12;
    case PLATFORM_KEY_F13: return GLFW_KEY_F13;
    case PLATFORM_KEY_F14: return GLFW_KEY_F14;
    case PLATFORM_KEY_F15: return GLFW_KEY_F15;
    case PLATFORM_KEY_F16: return GLFW_KEY_F16;
    case PLATFORM_KEY_F17: return GLFW_KEY_F17;
    case PLATFORM_KEY_F18: return GLFW_KEY_F18;
    case PLATFORM_KEY_F19: return GLFW_KEY_F19;
    case PLATFORM_KEY_F20: return GLFW_KEY_F20;
    case PLATFORM_KEY_F21: return GLFW_KEY_F21;
    case PLATFORM_KEY_F22: return GLFW_KEY_F22;
    case PLATFORM_KEY_F23: return GLFW_KEY_F23;
    case PLATFORM_KEY_F24: return GLFW_KEY_F24;
    case PLATFORM_KEY_F25: return GLFW_KEY_F25;
    case PLATFORM_KEY_KP_0: return GLFW_KEY_KP_0;
    case PLATFORM_KEY_KP_1: return GLFW_KEY_KP_1;
    case PLATFORM_KEY_KP_2: return GLFW_KEY_KP_2;
    case PLATFORM_KEY_KP_3: return GLFW_KEY_KP_3;
    case PLATFORM_KEY_KP_4: return GLFW_KEY_KP_4;
    case PLATFORM_KEY_KP_5: return GLFW_KEY_KP_5;
    case PLATFORM_KEY_KP_6: return GLFW_KEY_KP_6;
    case PLATFORM_KEY_KP_7: return GLFW_KEY_KP_7;
    case PLATFORM_KEY_KP_8: return GLFW_KEY_KP_8;
    case PLATFORM_KEY_KP_9: return GLFW_KEY_KP_9;
    case PLATFORM_KEY_KP_DECIMAL: return GLFW_KEY_KP_DECIMAL;
    case PLATFORM_KEY_KP_DIVIDE: return GLFW_KEY_KP_DIVIDE;
    case PLATFORM_KEY_KP_MULTIPLY: return GLFW_KEY_KP_MULTIPLY;
    case PLATFORM_KEY_KP_SUBTRACT: return GLFW_KEY_KP_SUBTRACT;
    case PLATFORM_KEY_KP_ADD: return GLFW_KEY_KP_ADD;
    case PLATFORM_KEY_KP_ENTER: return GLFW_KEY_KP_ENTER;
    case PLATFORM_KEY_KP_EQUAL: return GLFW_KEY_KP_EQUAL;
    case PLATFORM_KEY_LEFT_SHIFT: return GLFW_KEY_LEFT_SHIFT;
    case PLATFORM_KEY_LEFT_CONTROL: return GLFW_KEY_LEFT_CONTROL;
    case PLATFORM_KEY_LEFT_ALT: return GLFW_KEY_LEFT_ALT;
    case PLATFORM_KEY_LEFT_SUPER: return GLFW_KEY_LEFT_SUPER;
    case PLATFORM_KEY_RIGHT_SHIFT: return GLFW_KEY_RIGHT_SHIFT;
    case PLATFORM_KEY_RIGHT_CONTROL: return GLFW_KEY_RIGHT_CONTROL;
    case PLATFORM_KEY_RIGHT_ALT: return GLFW_KEY_RIGHT_ALT;
    case PLATFORM_KEY_RIGHT_SUPER: return GLFW_KEY_RIGHT_SUPER;
    case PLATFORM_KEY_MENU: return GLFW_KEY_MENU;
    default: return GLFW_KEY_UNKNOWN;
  }
}
static int translate_mods(int mods) {
  int out=0;
  if (mods & GLFW_MOD_SHIFT) out|=PLATFORM_MOD_SHIFT;
  if (mods & GLFW_MOD_CONTROL) out|=PLATFORM_MOD_CONTROL;
  if (mods & GLFW_MOD_ALT) out|=PLATFORM_MOD_ALT;
  if (mods & GLFW_MOD_SUPER) out|=PLATFORM_MOD_SUPER;
  if (mods & GLFW_MOD_CAPS_LOCK) out|=PLATFORM_MOD_CAPS_LOCK;
  if (mods & GLFW_MOD_NUM_LOCK) out|=PLATFORM_MOD_NUM_LOCK;
  return out;
}
static PlatformWindow *owner(GLFWwindow *w) { return glfwGetWindowUserPointer(w); }
/* Le callback specifiche alimentano input/resize; l'osservatore generale
 * riceve una copia logica dello stesso evento senza sostituire quei gestori. */
static void emit(PlatformWindow *w,PlatformEvent e) { if(w->event) w->event(w,&e); }
static void key_event(GLFWwindow *n,int key,int scan,int action,int mods) {
  PlatformWindow *w=owner(n);
  int a=action==GLFW_PRESS ? PLATFORM_PRESS : action==GLFW_REPEAT ? PLATFORM_REPEAT : PLATFORM_RELEASE;
  int k=translate_key(key), m=translate_mods(mods);
  if(w->key) w->key(w,k,scan,a,m);
  emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_KEY,.key=k,.scancode=scan,.action=a,.mods=m});
}
static void char_event(GLFWwindow *n,unsigned cp) {
  PlatformWindow *w=owner(n); if(w->character) w->character(w,cp);
  emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_CHARACTER,.codepoint=cp});
}
static void focus_event(GLFWwindow *n,int focused) {
  PlatformWindow *w=owner(n); if(w->focus) w->focus(w,focused!=0);
  emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_FOCUS,.value=focused!=0});
}
static void scroll_event(GLFWwindow *n,double x,double y) {
  PlatformWindow *w=owner(n); if(w->scroll) w->scroll(w,x,y);
  emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_SCROLL,.x=x,.y=y});
}
static void resize_event(GLFWwindow *n,int x,int y) {
  PlatformWindow *w=owner(n); if(w->resize) w->resize(w,x,y);
  emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_FRAMEBUFFER,.width=x,.height=y});
}
static void size_event(GLFWwindow *n,int x,int y) {
  PlatformWindow *w=owner(n); w->width=x; w->height=y;
  emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_SIZE,.width=x,.height=y});
}
static void iconify_event(GLFWwindow *n,int value) {
  PlatformWindow *w=owner(n); if(w->iconify) w->iconify(w,value!=0);
  emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_ICONIFY,.value=value!=0});
}
static void cursor_event(GLFWwindow *n,double x,double y) {
  emit(owner(n),(PlatformEvent){.kind=PLATFORM_EVENT_CURSOR,.x=x,.y=y});
}
static void close_event(GLFWwindow *n) { emit(owner(n),(PlatformEvent){.kind=PLATFORM_EVENT_CLOSE}); }
static void button_event(GLFWwindow *n,int button,int action,int mods) {
  int b;
  switch(button) {
    case GLFW_MOUSE_BUTTON_LEFT: b=PLATFORM_MOUSE_BUTTON_LEFT; break;
    case GLFW_MOUSE_BUTTON_RIGHT: b=PLATFORM_MOUSE_BUTTON_RIGHT; break;
    case GLFW_MOUSE_BUTTON_MIDDLE: b=PLATFORM_MOUSE_BUTTON_MIDDLE; break;
    case GLFW_MOUSE_BUTTON_4: b=PLATFORM_MOUSE_BUTTON_4; break;
    case GLFW_MOUSE_BUTTON_5: b=PLATFORM_MOUSE_BUTTON_5; break;
    case GLFW_MOUSE_BUTTON_6: b=PLATFORM_MOUSE_BUTTON_6; break;
    case GLFW_MOUSE_BUTTON_7: b=PLATFORM_MOUSE_BUTTON_7; break;
    case GLFW_MOUSE_BUTTON_8: b=PLATFORM_MOUSE_BUTTON_8; break;
    default: return;
  }
  emit(owner(n),(PlatformEvent){.kind=PLATFORM_EVENT_BUTTON,.button=b,
    .action=action==GLFW_PRESS ? PLATFORM_PRESS : PLATFORM_RELEASE,.mods=translate_mods(mods)});
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

/* Ogni errore prima della restituzione libera le risorse locali. Solo dopo
 * l'allocazione del wrapper il conteggio delle finestre viene incrementato. */
PlatformWindow *platform_window_create(const PlatformWindowConfig *config) {

  if (!config || config->width<=0 || config->height<=0) return NULL;
  if (!window_count && !glfwInit()) return NULL;
  glfwDefaultWindowHints();
  glfwWindowHint(GLFW_VISIBLE,!config->hidden);

  // dont need to create opengl context
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  //
  // In autostart editor (hook di sviluppo per screenshot/regression
  // visiva) la finestra è a dimensione fissa: i tiling WM re-impaginano le
  // finestre resizable nella loro griglia, comprimendo la UI virtuale
  // 1920x1080 fino all'illeggibile. Uso normale: invariato (resizable).
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  int width = config->width;
  int height = config->height;
  GLFWmonitor *monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode *mode = monitor ? glfwGetVideoMode(monitor) : NULL;
  GLFWmonitor *fullscreenMonitor = NULL;

  if (config->mode == PLATFORM_FULLSCREEN && monitor) {
    fullscreenMonitor = monitor;
    const GLFWvidmode *selected =
        find_supported_video_mode(monitor, width, height);
    if (!selected)
      selected = mode;
    if (selected) {
      width = selected->width;
      height = selected->height;
    }
  } else if (config->mode == PLATFORM_BORDERLESS &&
             mode) {
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    width = mode->width;
    height = mode->height;
  }

  GLFWwindow *window = glfwCreateWindow(width, height, config->title,
                                        fullscreenMonitor, NULL);
  if (!window) {
    if (!window_count) glfwTerminate();
    return NULL;
  }
  if (config->mode == PLATFORM_BORDERLESS && monitor) {
    int monitor_x = 0, monitor_y = 0;
    glfwGetMonitorPos(monitor, &monitor_x, &monitor_y);
    glfwSetWindowPos(window, monitor_x, monitor_y);
  }
  PlatformWindow *w=calloc(1,sizeof(*w));
  if (!w) { glfwDestroyWindow(window); if (!window_count) glfwTerminate(); return NULL; }
  w->native=window; ++window_count;
  glfwSetWindowUserPointer(window,w);
  glfwGetWindowSize(window,&w->width,&w->height);
  glfwSetWindowSizeCallback(window,size_event);
  glfwSetFramebufferSizeCallback(window,resize_event);
  glfwSetKeyCallback(window,key_event);
  glfwSetCharCallback(window,char_event);
  glfwSetWindowFocusCallback(window,focus_event);
  glfwSetScrollCallback(window,scroll_event);
  glfwSetWindowIconifyCallback(window,iconify_event);
  glfwSetCursorPosCallback(window,cursor_event);
  glfwSetMouseButtonCallback(window,button_event);
  glfwSetWindowCloseCallback(window,close_event);
  glfwSetInputMode(window,GLFW_LOCK_KEY_MODS,GLFW_TRUE);
  return w;
}

void destroy_window(PlatformWindow *w) {
  if (!w) return;
  glfwDestroyWindow(w->native); free(w);
  if (--window_count==0) glfwTerminate();
}
void platform_poll_events(void) { glfwPollEvents(); }
void platform_wait_events(void) { glfwWaitEvents(); }
double platform_time(void) { return glfwGetTime(); }
int platform_window_should_close(PlatformWindow *w) { return glfwWindowShouldClose(w->native); }
void platform_window_set_should_close(PlatformWindow *w,int value) { glfwSetWindowShouldClose(w->native,value!=0); }
void platform_window_size(PlatformWindow *w,int *x,int *y) { if(x)*x=w->width; if(y)*y=w->height; }
void platform_framebuffer_size(PlatformWindow *w,int *x,int *y) { glfwGetFramebufferSize(w->native,x,y); }
void platform_cursor_position(PlatformWindow *w,double *x,double *y) { glfwGetCursorPos(w->native,x,y); }
int platform_key_state(PlatformWindow *w,int key) {
  int k=native_key(key);
  return k==GLFW_KEY_UNKNOWN ? PLATFORM_RELEASE : glfwGetKey(w->native,k)==GLFW_PRESS ? PLATFORM_PRESS : PLATFORM_RELEASE;
}
int platform_mouse_button(PlatformWindow *w,int button) {
  static const int buttons[]={GLFW_MOUSE_BUTTON_LEFT,GLFW_MOUSE_BUTTON_RIGHT,GLFW_MOUSE_BUTTON_MIDDLE,
    GLFW_MOUSE_BUTTON_4,GLFW_MOUSE_BUTTON_5,GLFW_MOUSE_BUTTON_6,GLFW_MOUSE_BUTTON_7,GLFW_MOUSE_BUTTON_8};
  return button<0 || button>=PLATFORM_MOUSE_BUTTON_COUNT ? PLATFORM_RELEASE :
    glfwGetMouseButton(w->native,buttons[button])==GLFW_PRESS ? PLATFORM_PRESS : PLATFORM_RELEASE;
}
void platform_window_set_user_pointer(PlatformWindow *w,void *p) { w->user=p; }
void *platform_window_user_pointer(PlatformWindow *w) { return w->user; }
PlatformKeyCallback platform_set_key_callback(PlatformWindow *w,PlatformKeyCallback fn) { PlatformKeyCallback old=w->key; w->key=fn; return old; }
PlatformCharCallback platform_set_char_callback(PlatformWindow *w,PlatformCharCallback fn) { PlatformCharCallback old=w->character; w->character=fn; return old; }
PlatformFocusCallback platform_set_focus_callback(PlatformWindow *w,PlatformFocusCallback fn) { PlatformFocusCallback old=w->focus; w->focus=fn; return old; }
PlatformScrollCallback platform_set_scroll_callback(PlatformWindow *w,PlatformScrollCallback fn) { PlatformScrollCallback old=w->scroll; w->scroll=fn; return old; }
PlatformResizeCallback platform_set_resize_callback(PlatformWindow *w,PlatformResizeCallback fn) { PlatformResizeCallback old=w->resize; w->resize=fn; return old; }
PlatformIconifyCallback platform_set_iconify_callback(PlatformWindow *w,PlatformIconifyCallback fn) { PlatformIconifyCallback old=w->iconify; w->iconify=fn; return old; }

PlatformEventCallback platform_set_event_callback(PlatformWindow *w,PlatformEventCallback fn) { PlatformEventCallback old=w->event; w->event=fn; return old; }

const char *platform_clipboard_text(PlatformWindow *w) {
  return w?glfwGetClipboardString(w->native):NULL;
}
bool platform_clipboard_set_text(PlatformWindow *w,const char *text) {
  if(!w || !text) return false;
  glfwGetError(NULL);
  glfwSetClipboardString(w->native,text);
  return glfwGetError(NULL)==GLFW_NO_ERROR;
}
