/* SDL dummy window + replay at Poll/WaitEvent + real collectors. Window flags,
 * framebuffer scale and surfaces are wrapped: this suite is not native/GPU proof. */
#include "../src/platform/backends/sdl2_private.h"
#include "../src/platform/backends/sdl2_keys.h"
#include "render/platform_surface.h"
#include "input/keyboard.h"
#include "input/mouse.h"
#include "input/scroll.h"
#include "backends/ui_sdl2.h"
#include <SDL_vulkan.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"SDL platform:%d: %s (%s)\n",__LINE__,#x,SDL_GetError()); exit(1); } } while(0)
float g_virtualWidth=1000,g_virtualHeight=800;
static bool fail_create,fail_reserve,fail_query,fail_surface;
static unsigned extension_count=2;
static Uint32 creation_flags;
static int pixel_scale=2;
static SDL_Event injected[4096];
static size_t injected_head,injected_count;
int __real_SDL_PollEvent(SDL_Event *);
int __real_SDL_WaitEvent(SDL_Event *);
static bool replay(SDL_Event *event) {
  if(injected_head==injected_count) { injected_head=injected_count=0; return false; }
  *event=injected[injected_head++]; return true;
}
int __wrap_SDL_PollEvent(SDL_Event *event) { return replay(event)?1:__real_SDL_PollEvent(event); }
int __wrap_SDL_WaitEvent(SDL_Event *event) { return replay(event)?1:__real_SDL_WaitEvent(event); }
SDL_Window *__real_SDL_CreateWindow(const char *,int,int,int,int,Uint32);
SDL_Window *__wrap_SDL_CreateWindow(const char *title,int x,int y,int width,int height,Uint32 flags) {
  creation_flags=flags;
  if(fail_create) { SDL_SetError("injected window failure"); return NULL; }
  return __real_SDL_CreateWindow(title,x,y,width,height,flags&~SDL_WINDOW_VULKAN);
}
void __wrap_SDL_GetWindowSizeInPixels(SDL_Window *window,int *width,int *height) {
  int w,h; SDL_GetWindowSize(window,&w,&h);
  if(width) *width=w*pixel_scale;
  if(height) *height=h*pixel_scale;
}
void *__real_realloc(void *,size_t);
void *__wrap_realloc(void *pointer,size_t bytes) { return fail_reserve?NULL:__real_realloc(pointer,bytes); }
SDL_bool __wrap_SDL_Vulkan_GetInstanceExtensions(SDL_Window *window,unsigned *count,const char **names) {
  CHECK(window);
  if(fail_query) return SDL_FALSE;
  if(names) { CHECK(*count>=extension_count); for(unsigned i=0;i<extension_count;++i) names[i]="VK_KHR_surface"; }
  *count=extension_count; return SDL_TRUE;
}
SDL_bool __wrap_SDL_Vulkan_CreateSurface(SDL_Window *window,VkInstance instance,VkSurfaceKHR *surface) {
  CHECK(window&&instance&&surface); *surface=(VkSurfaceKHR)(uintptr_t)0x1234;
  return fail_surface?SDL_FALSE:SDL_TRUE;
}
static PlatformEvent events[4096];
static size_t event_count;
static unsigned key_calls,char_calls,focus_calls,resize_calls,iconify_calls;
static bool cancel_close;
static void observe(PlatformWindow *window,const PlatformEvent *event) {
  CHECK(event_count<4096); events[event_count++]=*event;
  if(event->kind==PLATFORM_EVENT_CLOSE && cancel_close) platform_window_set_should_close(window,false);
  if(event->kind==PLATFORM_EVENT_BUTTON) {
    CHECK(platform_mouse_button(window,event->button)==event->action);
    double x,y; platform_cursor_position(window,&x,&y); CHECK(isfinite(x)&&isfinite(y));
  }
}
static void on_key(PlatformWindow *w,int key,int scan,int action,int mods) {
  ++key_calls;
  if(key!=PLATFORM_KEY_UNKNOWN) CHECK(platform_key_state(w,key)==(action==PLATFORM_RELEASE?PLATFORM_RELEASE:PLATFORM_PRESS));
}
static void on_char(PlatformWindow *w,unsigned cp) { ++char_calls; }
static void on_focus(PlatformWindow *w,int focused) { ++focus_calls; }
static void on_resize(PlatformWindow *w,int width,int height) { ++resize_calls; }
static void on_iconify(PlatformWindow *w,int minimized) { ++iconify_calls; }
/* SDL2-compat 2.32.74 rejects conversion of pushed TEXTINPUT events. Replay
 * at the poll boundary tests our translator without that producer limitation. */
static void queue(SDL_Event event) { CHECK(injected_count<4096); injected[injected_count++]=event; }
static void window_event(Uint32 id,Uint8 event) { queue((SDL_Event){.window={.type=SDL_WINDOWEVENT,.windowID=id,.event=event}}); }
static void native_key(Uint32 id,SDL_Scancode scan,bool down,bool repeat,Uint16 mods) {
  SDL_Event event={.key={.type=down?SDL_KEYDOWN:SDL_KEYUP,.windowID=id,.repeat=repeat,
                       .keysym={.scancode=scan,.mod=mods}}}; queue(event);
}
static void text(Uint32 id,const char *value) {
  SDL_Event event={.text={.type=SDL_TEXTINPUT,.windowID=id}};
  snprintf(event.text.text,sizeof(event.text.text),"%s",value); queue(event);
}
static void button(Uint32 id,bool down,int x,int y) {
  queue((SDL_Event){.button={.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP,.windowID=id,.button=SDL_BUTTON_LEFT,.x=x,.y=y}});
}
static void mapping(PlatformWindow *w,Uint32 id) {
  UISDL2Input reference={0};
  for(int scan=0;scan<SDL_NUM_SCANCODES;++scan) {
    ui_sdl2_input_begin(&reference);
    SDL_Event event={.key={.type=SDL_KEYDOWN,.windowID=id,.keysym={.scancode=(SDL_Scancode)scan}}};
    ui_sdl2_input_event(&reference,&event);
    event_count=0; queue(event); native_key(id,(SDL_Scancode)scan,false,false,0); platform_poll_events();
    CHECK(event_count==2&&events[0].kind==PLATFORM_EVENT_KEY&&events[1].action==PLATFORM_RELEASE);
    if(scan==SDL_SCANCODE_NONUSBACKSLASH) CHECK(events[0].key==PLATFORM_KEY_WORLD_1);
    else if(scan==SDL_SCANCODE_INTERNATIONAL1) CHECK(events[0].key==PLATFORM_KEY_WORLD_2);
    else CHECK(events[0].key==reference.events[0].key);
  }
  CHECK(platform_key_state(w,-1)==PLATFORM_RELEASE&&platform_key_state(w,PLATFORM_KEY_LAST)==PLATFORM_RELEASE);
  native_key(id,SDL_SCANCODE_A,true,true,KMOD_SHIFT|KMOD_CTRL|KMOD_ALT|KMOD_GUI|KMOD_CAPS|KMOD_NUM);
  event_count=0; platform_poll_events(); CHECK(event_count==1&&events[0].action==PLATFORM_REPEAT&&events[0].mods==63);
}
static void collectors(PlatformWindow *w,Uint32 id) {
  input_keyboard_register(w); input_mouse_register(w); input_scroll_register(w);
  event_count=0; text(id,"è€🙂"); native_key(id,SDL_SCANCODE_TAB,true,false,KMOD_SHIFT);
  button(id,true,64,48); button(id,false,64,48); button(id,true,320,240); button(id,false,320,240);
  queue((SDL_Event){.motion={.type=SDL_MOUSEMOTION,.windowID=id,.x=500,.y=400}});
  queue((SDL_Event){.wheel={.type=SDL_MOUSEWHEEL,.windowID=id,.preciseX=.25f,.preciseY=1.5f}});
  queue((SDL_Event){.wheel={.type=SDL_MOUSEWHEEL,.windowID=id,.preciseY=.5f,.direction=SDL_MOUSEWHEEL_FLIPPED}});
  platform_poll_events(); input_keyboard_begin_frame(w);
  size_t count; InputKeyEvent *keys=input_keyboard_events(&count);
  CHECK(count==4&&keys[0].codepoint==0xe8&&keys[1].codepoint==0x20ac&&keys[2].codepoint==0x1f642);
  CHECK(keys[3].key==PLATFORM_KEY_TAB&&keys[3].mods==PLATFORM_MOD_SHIFT);
  CHECK(input_scroll_take()==1&&input_scroll_take()==0);
  APP app={.window=w}; Point p; bool cancelled;
  for(int i=0;i<4;++i) {
    CHECK(input_mouse_frame(&app,&p,&cancelled)==(i%2==0)&&!cancelled);
    CHECK(fabs(p.x-(i<2?100:500))<.001&&fabs(p.y-(i<2?80:400))<.001);
  }
  text(id,"lost"); button(id,true,20,30); window_event(id,SDL_WINDOWEVENT_FOCUS_LOST);
  window_event(id,SDL_WINDOWEVENT_FOCUS_GAINED); text(id,"ignored same batch"); platform_poll_events();
  input_keyboard_begin_frame(w); input_keyboard_events(&count); CHECK(!count&&input_keyboard_focus_lost());
  CHECK(!input_mouse_frame(&app,&p,&cancelled)&&cancelled);
  CHECK(platform_key_state(w,PLATFORM_KEY_A)==PLATFORM_RELEASE&&platform_mouse_button(w,0)==PLATFORM_RELEASE);
  text(id,"x"); platform_poll_events(); input_keyboard_begin_frame(w); keys=input_keyboard_events(&count);
  CHECK(count==1&&keys[0].codepoint=='x'&&!input_keyboard_focus_lost());
  input_mouse_unregister(w); input_keyboard_unregister(w); input_scroll_unregister(w);
  CHECK(platform_set_key_callback(w,on_key)==on_key&&platform_set_event_callback(w,observe)==observe);
}
static void surfaces(PlatformWindow *w) {
  uint32_t count=99; CHECK(!platform_vulkan_extensions(NULL));
  fail_reserve=true; CHECK(!platform_vulkan_extensions(&count)&&!count); fail_reserve=false;
  const char **names=platform_vulkan_extensions(&count); CHECK(names&&count==2);
  CHECK(platform_vulkan_extensions(&count)==names);
  extension_count=4; fail_reserve=true; CHECK(!platform_vulkan_extensions(&count)&&!count); fail_reserve=false;
  CHECK(platform_vulkan_extensions(&count)&&count==4);
  fail_query=true; CHECK(!platform_vulkan_extensions(&count)&&!count); fail_query=false;
  VkSurfaceKHR surface=VK_NULL_HANDLE; VkInstance instance=(VkInstance)(uintptr_t)1;
  CHECK(platform_vulkan_surface(instance,w,&surface)==VK_SUCCESS&&surface!=VK_NULL_HANDLE);
  fail_surface=true; CHECK(platform_vulkan_surface(instance,w,&surface)==VK_ERROR_INITIALIZATION_FAILED&&surface==VK_NULL_HANDLE); fail_surface=false;
  CHECK(platform_vulkan_surface(VK_NULL_HANDLE,w,&surface)!=VK_SUCCESS);
  CHECK(platform_vulkan_surface(instance,NULL,&surface)!=VK_SUCCESS&&!surface);
  CHECK(platform_vulkan_surface(instance,w,NULL)!=VK_SUCCESS);
}
int main(void) {
  CHECK(!platform_window_create(NULL));
  PlatformWindowConfig config={.title="SDL platform test",.width=640,.height=480,.hidden=true};
  config.width=0; CHECK(!platform_window_create(&config)); config.width=640;
  fail_create=true; CHECK(!platform_window_create(&config)); fail_create=false;
  CHECK(!(SDL_WasInit(SDL_INIT_VIDEO)&SDL_INIT_VIDEO));
  CHECK(SDL_InitSubSystem(SDL_INIT_VIDEO)==0); SDL_StopTextInput();
  PlatformWindow *w=platform_window_create(&config); CHECK(w&&!platform_window_create(&config));
  CHECK(SDL_IsTextInputActive()&&creation_flags&SDL_WINDOW_VULKAN&&creation_flags&SDL_WINDOW_ALLOW_HIGHDPI);
  int width,height; platform_window_size(w,&width,&height); CHECK(width==640&&height==480);
  platform_framebuffer_size(w,&width,&height); CHECK(width==1280&&height==960);
  CHECK(platform_sdl2_active_native()==platform_sdl2_native(w));
  platform_window_set_user_pointer(w,&config); CHECK(platform_window_user_pointer(w)==&config);
  platform_set_event_callback(w,observe); platform_set_key_callback(w,on_key); platform_set_char_callback(w,on_char);
  platform_set_focus_callback(w,on_focus); platform_set_resize_callback(w,on_resize); platform_set_iconify_callback(w,on_iconify);
  platform_poll_events(); Uint32 id=SDL_GetWindowID(platform_sdl2_native(w));
  mapping(w,id); collectors(w,id); CHECK(key_calls&&char_calls&&focus_calls);
  event_count=0; native_key(id+100,SDL_SCANCODE_B,true,false,0); text(id+100,"foreign"); platform_poll_events(); CHECK(!event_count);
  window_event(id,SDL_WINDOWEVENT_MINIMIZED); platform_poll_events(); platform_framebuffer_size(w,&width,&height);
  CHECK(!width&&!height&&iconify_calls==1&&resize_calls==1);
  window_event(id,SDL_WINDOWEVENT_RESTORED); platform_wait_events(); platform_framebuffer_size(w,&width,&height);
  CHECK(width==1280&&height==960&&iconify_calls==2&&resize_calls==2);
  SDL_SetWindowSize(platform_sdl2_native(w),800,600); platform_poll_events();
  platform_window_size(w,&width,&height); CHECK(width==800&&height==600);
  platform_framebuffer_size(w,&width,&height); CHECK(width==1600&&height==1200);
  CHECK(platform_clipboard_set_text(w,"Clipboard è €")); CHECK(!strcmp(platform_clipboard_text(w),"Clipboard è €"));
  CHECK(!platform_clipboard_set_text(w,NULL)&&!platform_clipboard_set_text(NULL,"x")&&!platform_clipboard_text(NULL));
  cancel_close=true; window_event(id,SDL_WINDOWEVENT_CLOSE); platform_poll_events(); CHECK(!platform_window_should_close(w));
  cancel_close=false; queue((SDL_Event){.type=SDL_QUIT}); platform_wait_events(); CHECK(platform_window_should_close(w));
  platform_window_set_should_close(w,false); CHECK(!platform_window_should_close(w));
  double before=platform_time(); SDL_Delay(2); CHECK(platform_time()>=before);
  surfaces(w); destroy_window(w); destroy_window(NULL); CHECK(!platform_sdl2_active_native());
  uint32_t count=99; CHECK(!platform_vulkan_extensions(&count)&&count==0);
  CHECK(SDL_WasInit(SDL_INIT_VIDEO)&SDL_INIT_VIDEO); CHECK(!SDL_IsTextInputActive()); SDL_QuitSubSystem(SDL_INIT_VIDEO);
  w=platform_window_create(&config); CHECK(w); destroy_window(w); CHECK(!(SDL_WasInit(SDL_INIT_VIDEO)&SDL_INIT_VIDEO));
  puts("SDL platform: PASS (replayed SDL events, physical keys, UTF-8, callbacks/collectors, tap positions, focus cancellation, HiDPI contract, wait/resize/iconify/close, dummy clipboard, lifecycle and surface failures; native input/GPU not exercised)");
}
