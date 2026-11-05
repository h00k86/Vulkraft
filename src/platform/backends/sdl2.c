/* SDL2 host for the existing APP/Vulkan path; main thread, one active window. */
#include "sdl2_private.h"
#include "sdl2_keys.h"
#include "text/utf8.h"
#include <stdlib.h>
#include <string.h>

#if !SDL_VERSION_ATLEAST(2,26,0)
#error SDL2 platform backend requires SDL 2.26 or newer
#endif

struct PlatformWindow {
  SDL_Window *native;
  Uint32 id;
  void *user;
  int width,height,fb_width,fb_height,close,mods;
  double x,y;
  bool minimized,started_text_input;
  unsigned char keys[PLATFORM_KEY_LAST],buttons[PLATFORM_MOUSE_BUTTON_COUNT];
  char *clipboard;
  const char **extensions;
  unsigned extension_capacity;
  PlatformEventCallback event;
  PlatformKeyCallback key;
  PlatformCharCallback character;
  PlatformFocusCallback focus;
  PlatformScrollCallback scroll;
  PlatformResizeCallback resize;
  PlatformIconifyCallback iconify;
};
static PlatformWindow *active;
static Uint64 clock_origin,clock_frequency;

SDL_Window *platform_sdl2_native(PlatformWindow *w) { return w&&w==active?w->native:NULL; }
SDL_Window *platform_sdl2_active_native(void) { return active?active->native:NULL; }
const char **platform_sdl2_extension_storage(unsigned count) {
  if(!active || !count) return NULL;
  if(count>active->extension_capacity) {
    size_t bytes=(size_t)count*sizeof(*active->extensions);
    if(bytes/sizeof(*active->extensions)!=count) { SDL_OutOfMemory(); return NULL; }
    const char **storage=realloc(active->extensions,bytes);
    if(!storage) { SDL_OutOfMemory(); return NULL; }
    active->extensions=storage; active->extension_capacity=count;
  }
  return active->extensions;
}
static void emit(PlatformWindow *w,PlatformEvent event) { if(w->event) w->event(w,&event); }
static void pixels(PlatformWindow *w,int *width,int *height) {
  if(w->minimized) { if(width) *width=0; if(height) *height=0; }
  else SDL_GetWindowSizeInPixels(w->native,width,height);
}
static void refresh_framebuffer(PlatformWindow *w) {
  int width,height; pixels(w,&width,&height);
  if(width==w->fb_width && height==w->fb_height) return;
  w->fb_width=width; w->fb_height=height;
  if(w->resize) w->resize(w,width,height);
  emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_FRAMEBUFFER,.width=width,.height=height});
}
static void logical_size(PlatformWindow *w,int width,int height) {
  if(width==w->width && height==w->height) return;
  w->width=width; w->height=height;
  emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_SIZE,.width=width,.height=height});
}
static void close_request(PlatformWindow *w) {
  w->close=1;
  /* Observer may cancel, as with the GLFW/native-close contract. */
  emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_CLOSE});
}
static int mouse_button(Uint8 button) {
  switch(button) {
  case SDL_BUTTON_LEFT: return PLATFORM_MOUSE_BUTTON_LEFT;
  case SDL_BUTTON_RIGHT: return PLATFORM_MOUSE_BUTTON_RIGHT;
  case SDL_BUTTON_MIDDLE: return PLATFORM_MOUSE_BUTTON_MIDDLE;
  default: return button>=4 && button<=8?PLATFORM_MOUSE_BUTTON_4+button-4:-1;
  }
}
static void window_event(PlatformWindow *w,const SDL_WindowEvent *event) {
  switch(event->event) {
  case SDL_WINDOWEVENT_FOCUS_LOST: case SDL_WINDOWEVENT_FOCUS_GAINED: {
    bool focused=event->event==SDL_WINDOWEVENT_FOCUS_GAINED;
    if(!focused) {
      memset(w->keys,0,sizeof(w->keys)); memset(w->buttons,0,sizeof(w->buttons));
      w->mods=0; w->x=w->y=-1;
    } else w->mods=platform_sdl2_mods(SDL_GetModState());
    if(w->focus) w->focus(w,focused);
    emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_FOCUS,.value=focused});
    break;
  }
  case SDL_WINDOWEVENT_LEAVE:
    w->x=w->y=-1;
    emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_CURSOR,.x=w->x,.y=w->y}); break;
  case SDL_WINDOWEVENT_ENTER: {
    int x,y; SDL_GetMouseState(&x,&y); w->x=x; w->y=y;
    emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_CURSOR,.x=w->x,.y=w->y}); break;
  }
  case SDL_WINDOWEVENT_RESIZED: case SDL_WINDOWEVENT_SIZE_CHANGED:
    logical_size(w,event->data1,event->data2); refresh_framebuffer(w); break;
  case SDL_WINDOWEVENT_MINIMIZED: case SDL_WINDOWEVENT_RESTORED: case SDL_WINDOWEVENT_MAXIMIZED: {
    bool minimized=event->event==SDL_WINDOWEVENT_MINIMIZED;
    if(w->minimized!=minimized) {
      w->minimized=minimized;
      if(w->iconify) w->iconify(w,minimized);
      emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_ICONIFY,.value=minimized});
    }
    refresh_framebuffer(w); break;
  }
  case SDL_WINDOWEVENT_MOVED: case SDL_WINDOWEVENT_DISPLAY_CHANGED:
    refresh_framebuffer(w); break;
  case SDL_WINDOWEVENT_CLOSE: close_request(w); break;
  }
}
static void dispatch(const SDL_Event *event) {
  /* This host owns the SDL queue. Release ignored SDL-owned payloads too. */
  if(event->type==SDL_DROPFILE || event->type==SDL_DROPTEXT) { SDL_free(event->drop.file); return; }
  if(event->type==SDL_TEXTEDITING_EXT) { SDL_free(event->editExt.text); return; }
  PlatformWindow *w=active; if(!w) return;
  if(event->type==SDL_QUIT) { close_request(w); return; }
  switch(event->type) {
  case SDL_WINDOWEVENT:
    if(event->window.windowID==w->id) window_event(w,&event->window);
    break;
  case SDL_KEYDOWN: case SDL_KEYUP: {
    if(event->key.windowID!=w->id) break;
    int key=platform_sdl2_key(event->key.keysym.scancode);
    int action=event->type==SDL_KEYUP?PLATFORM_RELEASE:event->key.repeat?PLATFORM_REPEAT:PLATFORM_PRESS;
    w->mods=platform_sdl2_mods((SDL_Keymod)event->key.keysym.mod);
    if(key>PLATFORM_KEY_UNKNOWN && key<PLATFORM_KEY_LAST) w->keys[key]=action!=PLATFORM_RELEASE;
    if(w->key) w->key(w,key,event->key.keysym.scancode,action,w->mods);
    emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_KEY,.key=key,.scancode=event->key.keysym.scancode,.action=action,.mods=w->mods});
    break;
  }
  case SDL_TEXTINPUT: {
    if(event->text.windowID!=w->id) break;
    size_t count=0; while(count<sizeof(event->text.text) && event->text.text[count]) ++count;
    for(size_t at=0;at<count;) {
      uint32_t cp; at+=utf8_decode(event->text.text+at,count-at,&cp);
      if(w->character) w->character(w,cp);
      emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_CHARACTER,.codepoint=cp});
    }
    break;
  }
  case SDL_MOUSEMOTION:
    if(event->motion.windowID!=w->id) break;
    w->x=event->motion.x; w->y=event->motion.y;
    emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_CURSOR,.x=w->x,.y=w->y}); break;
  case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: {
    if(event->button.windowID!=w->id) break;
    int button=mouse_button(event->button.button); if(button<0) break;
    int action=event->type==SDL_MOUSEBUTTONDOWN?PLATFORM_PRESS:PLATFORM_RELEASE;
    w->x=event->button.x; w->y=event->button.y; w->buttons[button]=action;
    /* Coordinates are committed before observers enqueue a transition. */
    emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_BUTTON,.button=button,.action=action,.mods=w->mods}); break;
  }
  case SDL_MOUSEWHEEL: {
    if(event->wheel.windowID!=w->id) break;
    double sign=event->wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-1:1;
    double x=sign*event->wheel.preciseX,y=sign*event->wheel.preciseY;
    if(w->scroll) w->scroll(w,x,y);
    emit(w,(PlatformEvent){.kind=PLATFORM_EVENT_SCROLL,.x=x,.y=y}); break;
  }
  }
}
PlatformWindow *platform_window_create(const PlatformWindowConfig *config) {
  if(active || !config || config->width<=0 || config->height<=0 ||
     config->mode<PLATFORM_WINDOWED || config->mode>PLATFORM_BORDERLESS) {
    SDL_SetError("SDL2 platform requires valid dimensions/mode and one active window"); return NULL;
  }
  PlatformWindow *w=calloc(1,sizeof(*w)); if(!w) { SDL_OutOfMemory(); return NULL; }
  if(SDL_InitSubSystem(SDL_INIT_VIDEO)!=0) { free(w); return NULL; }
  Uint32 flags=SDL_WINDOW_VULKAN|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI;
  flags|=config->hidden?SDL_WINDOW_HIDDEN:SDL_WINDOW_SHOWN;
  w->native=SDL_CreateWindow(config->title?config->title:"Graphics application",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,config->width,config->height,flags);
  if(!w->native) goto fail;
  if(config->mode==PLATFORM_FULLSCREEN) {
    SDL_DisplayMode requested={.w=config->width,.h=config->height},selected;
    if(!SDL_GetClosestDisplayMode(0,&requested,&selected) && SDL_GetDesktopDisplayMode(0,&selected)!=0) goto fail;
    if(SDL_SetWindowDisplayMode(w->native,&selected)!=0 || SDL_SetWindowFullscreen(w->native,SDL_WINDOW_FULLSCREEN)!=0) goto fail;
  } else if(config->mode==PLATFORM_BORDERLESS && SDL_SetWindowFullscreen(w->native,SDL_WINDOW_FULLSCREEN_DESKTOP)!=0) goto fail;
  if(config->hidden) SDL_HideWindow(w->native);
  w->id=SDL_GetWindowID(w->native); w->x=w->y=-1;
  SDL_GetWindowSize(w->native,&w->width,&w->height); pixels(w,&w->fb_width,&w->fb_height);
  w->mods=platform_sdl2_mods(SDL_GetModState());
  w->started_text_input=SDL_IsTextInputActive()!=SDL_TRUE;
  if(w->started_text_input) SDL_StartTextInput();
  clock_frequency=SDL_GetPerformanceFrequency(); clock_origin=SDL_GetPerformanceCounter();
  active=w; return w;
fail:
  if(w->native) SDL_DestroyWindow(w->native);
  free(w); SDL_QuitSubSystem(SDL_INIT_VIDEO); return NULL;
}
void destroy_window(PlatformWindow *w) {
  if(!w) return;
  if(w->started_text_input) SDL_StopTextInput();
  SDL_DestroyWindow(w->native); SDL_free(w->clipboard); free(w->extensions);
  active=NULL; free(w); SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
void platform_poll_events(void) { SDL_Event event; while(SDL_PollEvent(&event)) dispatch(&event); }
void platform_wait_events(void) {
  SDL_Event event;
  if(SDL_WaitEvent(&event)) dispatch(&event);
  platform_poll_events();
}
double platform_time(void) { return clock_frequency?(double)(SDL_GetPerformanceCounter()-clock_origin)/(double)clock_frequency:0; }
int platform_window_should_close(PlatformWindow *w) { return w->close; }
void platform_window_set_should_close(PlatformWindow *w,int close) { w->close=close!=0; }
void platform_window_size(PlatformWindow *w,int *width,int *height) { if(width) *width=w->width; if(height) *height=w->height; }
void platform_framebuffer_size(PlatformWindow *w,int *width,int *height) { pixels(w,width,height); }
void platform_cursor_position(PlatformWindow *w,double *x,double *y) { if(x) *x=w->x; if(y) *y=w->y; }
int platform_key_state(PlatformWindow *w,int key) { return key>PLATFORM_KEY_UNKNOWN&&key<PLATFORM_KEY_LAST&&w->keys[key]?PLATFORM_PRESS:PLATFORM_RELEASE; }
int platform_mouse_button(PlatformWindow *w,int button) { return button>=0&&button<PLATFORM_MOUSE_BUTTON_COUNT?w->buttons[button]:PLATFORM_RELEASE; }
const char *platform_clipboard_text(PlatformWindow *w) {
  if(!w) return NULL;
  SDL_free(w->clipboard); SDL_ClearError(); w->clipboard=SDL_GetClipboardText();
  if(SDL_GetError()[0]) { SDL_free(w->clipboard); w->clipboard=NULL; }
  return w->clipboard;
}
bool platform_clipboard_set_text(PlatformWindow *w,const char *text) {
  if(!w || !text) return false;
  SDL_free(w->clipboard); w->clipboard=NULL; return SDL_SetClipboardText(text)==0;
}
void platform_window_set_user_pointer(PlatformWindow *w,void *user) { w->user=user; }
void *platform_window_user_pointer(PlatformWindow *w) { return w->user; }
#define CALLBACK(name,type,member) type platform_set_##name##_callback(PlatformWindow *w,type callback) { type old=w->member; w->member=callback; return old; }
CALLBACK(key,PlatformKeyCallback,key)
CALLBACK(char,PlatformCharCallback,character)
CALLBACK(focus,PlatformFocusCallback,focus)
CALLBACK(scroll,PlatformScrollCallback,scroll)
CALLBACK(resize,PlatformResizeCallback,resize)
CALLBACK(iconify,PlatformIconifyCallback,iconify)
CALLBACK(event,PlatformEventCallback,event)
#undef CALLBACK
