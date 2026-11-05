/* Backend per test senza display: simula stato ed eventi, non una GPU.
 * Supporta una sola finestra e non fornisce una superficie di presentazione Vulkan. */
#include "platform/headless.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
struct PlatformWindow {
  void *user;
  char *clipboard;
  int width,height,fb_width,fb_height,close;
  double x,y;
  int keys[PLATFORM_KEY_LAST],buttons[PLATFORM_MOUSE_BUTTON_COUNT];
  PlatformEventCallback event;
  PlatformKeyCallback key;
  PlatformCharCallback character;
  PlatformFocusCallback focus;
  PlatformScrollCallback scroll;
  PlatformResizeCallback resize;
  PlatformIconifyCallback iconify;
};
/* Deliberately single-window, matching the current app/input lifecycle. */
static PlatformWindow *active;
/* Coda limitata per prove deterministiche: post restituisce false a saturazione.
 * La distruzione elimina gli eventi pendenti prima del successivo ciclo di vita. */
static PlatformEvent queue[2048];
static size_t count;
static double now;
PlatformWindow *platform_window_create(const PlatformWindowConfig *config) {
  if(active || !config || config->width<=0 || config->height<=0 || config->mode!=PLATFORM_WINDOWED) return NULL;
  active=calloc(1,sizeof(*active));
  if(active) {
    active->width=active->fb_width=config->width;
    active->height=active->fb_height=config->height;
    count=0; now=0;
  }
  return active;
}
void destroy_window(PlatformWindow *w) {
  if(w && w==active) { free(w->clipboard); free(w); active=NULL; count=0; now=0; }
}
/* Inserire un evento non invoca subito le callback: la consegna avviene
 * durante poll/wait, mantenendo lo stesso ordine usato nei test dell'editor. */
bool platform_headless_post(PlatformWindow *w,PlatformEvent e) {
  if(!w || w!=active || count==2048 || e.kind<PLATFORM_EVENT_KEY || e.kind>PLATFORM_EVENT_CLOSE) return false;
  if(e.kind==PLATFORM_EVENT_KEY && (e.key<0 || e.key>=PLATFORM_KEY_LAST || e.action<PLATFORM_RELEASE || e.action>PLATFORM_REPEAT)) return false;
  if(e.kind==PLATFORM_EVENT_BUTTON && (e.button<0 || e.button>=PLATFORM_MOUSE_BUTTON_COUNT || e.action<PLATFORM_RELEASE || e.action>PLATFORM_PRESS)) return false;
  if((e.kind==PLATFORM_EVENT_SIZE || e.kind==PLATFORM_EVENT_FRAMEBUFFER) && (e.width<0 || e.height<0)) return false;
  queue[count++]=e; return true;
}
void platform_poll_events(void) {
  /* Snapshot the queue: callbacks can enqueue events for the next poll. */
  PlatformEvent batch[2048]; size_t n=count;
  memcpy(batch,queue,n*sizeof(*batch)); count=0;
  PlatformWindow *w=active;
  for(size_t i=0;i<n && active==w && w;++i) {
    PlatformEvent e=batch[i];
    switch(e.kind) {
    case PLATFORM_EVENT_KEY:
      w->keys[e.key]=e.action==PLATFORM_RELEASE ? PLATFORM_RELEASE : PLATFORM_PRESS;
      if(w->key) w->key(w,e.key,e.scancode,e.action,e.mods);
      break;
    case PLATFORM_EVENT_CHARACTER: if(w->character) w->character(w,e.codepoint); break;
    case PLATFORM_EVENT_FOCUS:
      if(!e.value) { memset(w->keys,0,sizeof(w->keys)); memset(w->buttons,0,sizeof(w->buttons)); }
      if(w->focus) w->focus(w,e.value);
      break;
    case PLATFORM_EVENT_CURSOR: w->x=e.x; w->y=e.y; break;
    case PLATFORM_EVENT_BUTTON: w->buttons[e.button]=e.action; break;
    case PLATFORM_EVENT_SCROLL: if(w->scroll) w->scroll(w,e.x,e.y); break;
    case PLATFORM_EVENT_SIZE: w->width=e.width; w->height=e.height; break;
    case PLATFORM_EVENT_FRAMEBUFFER:
      w->fb_width=e.width; w->fb_height=e.height;
      if(w->resize) w->resize(w,e.width,e.height);
      break;
    case PLATFORM_EVENT_ICONIFY: if(w->iconify) w->iconify(w,e.value); break;
    case PLATFORM_EVENT_CLOSE: w->close=1; break;
    }
    if(w->event) w->event(w,&e);
  }
}
void platform_wait_events(void) { platform_poll_events(); }
/* Orologio virtuale: nessuna attesa reale nei test. Le durate negative o
 * non finite vengono ignorate. */
void platform_headless_advance(double seconds) { if(isfinite(seconds) && seconds>=0) now+=seconds; }
double platform_time(void) { return now; }
int platform_window_should_close(PlatformWindow *w) { return w->close; }
void platform_window_set_should_close(PlatformWindow *w,int value) { w->close=value!=0; }
void platform_window_size(PlatformWindow *w,int *x,int *y) { if(x)*x=w->width; if(y)*y=w->height; }
void platform_framebuffer_size(PlatformWindow *w,int *x,int *y) { if(x)*x=w->fb_width; if(y)*y=w->fb_height; }
void platform_cursor_position(PlatformWindow *w,double *x,double *y) { if(x)*x=w->x; if(y)*y=w->y; }
int platform_key_state(PlatformWindow *w,int key) { return key<0 || key>=PLATFORM_KEY_LAST ? PLATFORM_RELEASE : w->keys[key]; }
int platform_mouse_button(PlatformWindow *w,int b) { return b<0 || b>=PLATFORM_MOUSE_BUTTON_COUNT ? PLATFORM_RELEASE : w->buttons[b]; }
void platform_window_set_user_pointer(PlatformWindow *w,void *p) { w->user=p; }
void *platform_window_user_pointer(PlatformWindow *w) { return w->user; }
PlatformKeyCallback platform_set_key_callback(PlatformWindow *w,PlatformKeyCallback fn) { PlatformKeyCallback old=w->key; w->key=fn; return old; }
PlatformCharCallback platform_set_char_callback(PlatformWindow *w,PlatformCharCallback fn) { PlatformCharCallback old=w->character; w->character=fn; return old; }
PlatformFocusCallback platform_set_focus_callback(PlatformWindow *w,PlatformFocusCallback fn) { PlatformFocusCallback old=w->focus; w->focus=fn; return old; }
PlatformScrollCallback platform_set_scroll_callback(PlatformWindow *w,PlatformScrollCallback fn) { PlatformScrollCallback old=w->scroll; w->scroll=fn; return old; }
PlatformResizeCallback platform_set_resize_callback(PlatformWindow *w,PlatformResizeCallback fn) { PlatformResizeCallback old=w->resize; w->resize=fn; return old; }
PlatformIconifyCallback platform_set_iconify_callback(PlatformWindow *w,PlatformIconifyCallback fn) { PlatformIconifyCallback old=w->iconify; w->iconify=fn; return old; }

PlatformEventCallback platform_set_event_callback(PlatformWindow *w,PlatformEventCallback fn) { PlatformEventCallback old=w->event; w->event=fn; return old; }

/* In-memory host clipboard for deterministic adapter tests; never the desktop. */
const char *platform_clipboard_text(PlatformWindow *w) {
  return w && w==active?(w->clipboard?w->clipboard:""):NULL;
}
bool platform_clipboard_set_text(PlatformWindow *w,const char *text) {
  if(!w || w!=active || !text) return false;
  size_t n=strlen(text)+1; char *copy=malloc(n);
  if(!copy) return false;
  memcpy(copy,text,n); free(w->clipboard); w->clipboard=copy; return true;
}
