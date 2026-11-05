/* Opt-in X11 probe: hidden window, no cursor motion and no desktop focus. */
#include "../src/ui/ui.c"
#include "ui_test_renderer.h"
#include "ui_app_test_adapter.h"
#include "../src/platform/backends/glfw_private.h"
#define GLFW_EXPOSE_NATIVE_X11
#define Font X11Font
#define Glyph X11Glyph
#include <GLFW/glfw3native.h>
#undef Font
#undef Glyph
#include <X11/keysym.h>
#include <assert.h>
float g_virtualWidth=1280, g_virtualHeight=800;
void draw_quad(APP *a,float x,float y,float w,float h,float r,float g,float b) {}
void draw_hud_paper_panel(APP *a,float x,float y,float w,float h,const float f[4],const float c[3],bool focus,uint32_t seed) {}
float measure_text_width(Font *f,const char *t,float s) { return 0; }
void draw_text_colored(APP *a,Font *f,const char *t,float x,float y,float s,float r,float g,float b) {}
static int presses, releases;
static void observed(PlatformWindow *w,int key,int scancode,int action,int mods) {
  if(key==PLATFORM_KEY_Z) { presses += action==PLATFORM_PRESS; releases += action==PLATFORM_RELEASE; }
}
static void send_key(PlatformWindow *window,int type) {
  Display *display=glfwGetX11Display();
  XEvent event={0};
  event.xkey=(XKeyEvent){.type=type,.display=display,.window=glfwGetX11Window(platform_glfw_native(window)),
      .root=DefaultRootWindow(display),.same_screen=True,
      .keycode=XKeysymToKeycode(display,XK_z),.time=CurrentTime};
  assert(XSendEvent(display,event.xkey.window,False,type==KeyPress?KeyPressMask:KeyReleaseMask,&event));
  XSync(display,False);
}
static void field(APP *app,char buffer[32]) {
  Font font={.ascender=1};
  ui_keyboard_begin_frame(app);
  ui_text_input("probe",&font,ANCHOR_TOP_LEFT,0,14,0,0,100,buffer,32,NULL);
}
int main(void) {
  UIContext *test_ui=ui_context_create();
  bind_test_renderer(test_ui);
  assert(test_ui && ui_context_set_current(test_ui));
  PlatformWindowConfig config={.width=200,.height=100,.title="Hidden key event probe",.hidden=true};
  PlatformWindow *window=platform_window_create(&config);
  if(!window) return 2;
  platform_set_key_callback(window,observed);
  input_keyboard_register(window);
  APP app={.window=window}; char text[32]="";
  UITextInputState *state=ui_text_field_state("probe");
  state->focused=true;
  platform_poll_events(); field(&app,text);
  send_key(window,KeyPress); platform_poll_events(); field(&app,text);
  send_key(window,KeyRelease); platform_poll_events(); field(&app,text);
  bool control=!strcmp(text,"z") && presses==1 && releases==1 && state->focused;
  printf("separate frames: text='%s', press=%d release=%d focus=%d control=%s\n",text,presses,releases,state->focused,control?"PASS":"FAIL");
  text[0]=0; state->caret=0; presses=releases=0;
  send_key(window,KeyPress); send_key(window,KeyRelease);
  platform_poll_events(); field(&app,text);
  bool delivered=presses==1 && releases==1 && state->focused;
  bool preserved=!strcmp(text,"z");
  printf("same frame: text='%s', press=%d release=%d focus=%d preserved=%s\n",text,presses,releases,state->focused,preserved?"PASS":"FAIL");
  input_keyboard_unregister(window);
  destroy_window(window);
  assert(ui_context_destroy(test_ui));
  return !control || !delivered ? 2 : preserved ? 0 : 1;
}

void texture_manager_preview_clear(APP *app) {}
