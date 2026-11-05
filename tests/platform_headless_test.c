#include "../src/ui/ui_state_frame_internal.h"
/* Real UI/input/platform adapter with the headless backend, no GLFW or Vulkan linkage. */
#include "../src/ui/ui.c"
#include "ui_test_renderer.h"
#include "ui_app_test_adapter.h"
#include "platform/headless.h"
#include "input/scroll.h"
#include "input/mouse.h"
#include <assert.h>
float g_virtualWidth=1280,g_virtualHeight=800;
void draw_quad(APP *a,float x,float y,float w,float h,float r,float g,float b) {}
void draw_hud_paper_panel(APP *a,float x,float y,float w,float h,const float f[4],const float c[3],bool focus,uint32_t seed) {}
float measure_text_width(Font *f,const char *s,float size) { return 0; }
void draw_text_colored(APP *a,Font *f,const char *s,float x,float y,float size,float r,float g,float b) {}
static int observed, event_count;
static PlatformEventKind kinds[1024];
static void events(PlatformWindow *w,const PlatformEvent *e) {
  assert(event_count<1024); kinds[event_count++]=e->kind;
}
static void observer(PlatformWindow *w,int key,int scan,int action,int mods) {
  assert(key==PLATFORM_KEY_A); ++observed;
}
static void post(PlatformWindow *w,PlatformEvent event) { assert(platform_headless_post(w,event)); }
static void mouse_button(PlatformWindow *w,int action) {
  post(w,(PlatformEvent){.kind=PLATFORM_EVENT_BUTTON,.button=PLATFORM_MOUSE_BUTTON_LEFT,.action=action});
}
static bool mouse_frame(APP *app,bool expect_down,bool expect_cancel,double x,double y) {
  Point p; bool cancelled;
  bool down=input_mouse_frame(app,&p,&cancelled);
  assert(down==expect_down && cancelled==expect_cancel);
  assert(fabs(p.x-x)<.001 && fabs(p.y-y)<.001);
  if(cancelled) ui_release_input_capture();
  ui_begin_frame(app,p.x,p.y,down,.016f);
  Font font={.ascender=1};
  bool clicked=ui_button_labeled("mouse/test","Click",&font,ANCHOR_TOP_LEFT,10,1,0,0,NULL);
  ui_end_frame();
  return clicked;
}
int main(void) {
  UIContext *test_ui=ui_context_create();
  bind_test_renderer(test_ui);
  assert(test_ui && ui_context_set_current(test_ui));
  for(int cycle=0;cycle<3;++cycle) {
    APP app={.title="Headless",.settings.video={.width=640,.height=400}};
    app.window=create_window(&app); assert(app.window);
    assert(platform_window_user_pointer(app.window)==&app);
    event_count=0; platform_set_event_callback(app.window,events);
    platform_set_key_callback(app.window,observer);
    input_keyboard_register(app.window); input_scroll_register(app.window);
    assert(platform_time()==0); platform_headless_advance(0.25); assert(platform_time()==0.25);
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_KEY,.key=PLATFORM_KEY_A,.action=PLATFORM_PRESS,.mods=PLATFORM_MOD_SHIFT});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_CHARACTER,.codepoint=0xe8});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_KEY,.key=PLATFORM_KEY_A,.action=PLATFORM_REPEAT,.mods=PLATFORM_MOD_SHIFT});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_CHARACTER,.codepoint=0x1f642});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_KEY,.key=PLATFORM_KEY_A,.action=PLATFORM_RELEASE});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_CURSOR,.x=320,.y=200});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_BUTTON,.button=PLATFORM_MOUSE_BUTTON_LEFT,.action=PLATFORM_PRESS});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_SCROLL,.y=2.5});
    platform_poll_events(); ui_keyboard_begin_frame(&app);
    assert(event_count==8 && kinds[0]==PLATFORM_EVENT_KEY && kinds[1]==PLATFORM_EVENT_CHARACTER && kinds[7]==PLATFORM_EVENT_SCROLL);
    size_t count; InputKeyEvent *e=input_keyboard_events(&count);
    assert(count==5 && e[0].mods==PLATFORM_MOD_SHIFT && e[2].action==PLATFORM_REPEAT);
    assert(platform_key_state(app.window,PLATFORM_KEY_A)==PLATFORM_RELEASE);
    assert(platform_mouse_button(app.window,PLATFORM_MOUSE_BUTTON_LEFT)==PLATFORM_PRESS);
    Point mouse=get_mouse_coordinate(&app); assert(mouse.x==640 && mouse.y==400);
    assert(input_scroll_take()==2.5f && input_scroll_take()==0);
    char text[32]=""; Font font={.ascender=1};
    ui_text_input_focus("test");
    ui_text_input("test",&font,ANCHOR_TOP_LEFT,0,14,0,0,100,text,sizeof(text),NULL);
    assert(!strcmp(text,"è🙂") && observed==(cycle+1)*3);
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_CHARACTER,.codepoint='x'});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_FOCUS,.value=0});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_SIZE,.width=800,.height=600});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_FRAMEBUFFER,.width=1600,.height=1200});
    platform_wait_events(); ui_keyboard_begin_frame(&app);
    assert(!ui_text_input_is_focused("test") && input_keyboard_focus_lost());
    assert(platform_mouse_button(app.window,PLATFORM_MOUSE_BUTTON_LEFT)==PLATFORM_RELEASE);
    assert(app.framebuffer_resized);
    int w,h; platform_window_size(app.window,&w,&h); assert(w==800 && h==600);
    platform_framebuffer_size(app.window,&w,&h); assert(w==1600 && h==1200);
    /* The APP adapter must deliver wheel input in the same focus-aware batch. */
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_FOCUS,.value=1});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_SCROLL,.y=1.25});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_SCROLL,.y=2.5});
    platform_poll_events();
    ui_begin_frame(&app,0,0,false,.016f);
    assert(ui_scroll_delta()==3.75f && input_scroll_take()==0);
    ui_end_frame();
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_SCROLL,.y=4});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_FOCUS,.value=0});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_SCROLL,.y=8});
    platform_poll_events();
    ui_begin_frame(&app,0,0,false,.016f);
    assert(input_keyboard_focus_lost() && ui_scroll_delta()==0 && input_scroll_take()==0);
    ui_end_frame();
    /* Further frames in the background must stay cancelled, including wheel. */
    ui_text_input_focus("test");
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_SCROLL,.y=16});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_CHARACTER,.codepoint='z'});
    platform_poll_events();
    ui_begin_frame(&app,0,0,true,.016f);
    assert(input_keyboard_focus_lost() && ui_scroll_delta()==0 && !ui_text_input_is_focused("test"));
    assert(!ui_state_frame(ui_ctx())->g_mouse_down && !ui_state_frame(ui_ctx())->key_event_count && input_scroll_take()==0);
    ui_end_frame();
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_FOCUS,.value=1});
    platform_poll_events();
    ui_begin_frame(&app,0,0,false,.016f);
    assert(ui_scroll_delta()==0); /* Cancelled deltas must not leak into a later frame. */
    ui_end_frame();
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_SCROLL,.y=NAN});
    platform_poll_events();
    ui_begin_frame(&app,0,0,false,.016f);
    assert(ui_scroll_delta()==0 && input_scroll_take()==0);
    ui_end_frame();
    assert(!platform_headless_post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_KEY,.key=PLATFORM_KEY_LAST}));
    input_mouse_register(app.window);
    input_mouse_register(app.window); /* Idempotent; must not chain to itself. */
    /* Two complete taps and later motion in a single poll. Final polling state
     * is released, but both clicks must arrive at their original position. */
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_CURSOR,.x=10,.y=10});
    for(int tap=0;tap<2;++tap) {
      mouse_button(app.window,PLATFORM_PRESS);
      mouse_button(app.window,PLATFORM_RELEASE);
    }
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_CURSOR,.x=700,.y=500});
    platform_poll_events();
    assert(platform_mouse_button(app.window,PLATFORM_MOUSE_BUTTON_LEFT)==PLATFORM_RELEASE);
    for(int tap=0;tap<2;++tap) {
      assert(!mouse_frame(&app,true,false,16,800.0/60));
      assert(mouse_frame(&app,false,false,16,800.0/60));
    }
    assert(!mouse_frame(&app,false,false,1120,800.0*5/6)); /* No replay. */
    /* Release outside the button must not activate it. */
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_CURSOR,.x=10,.y=10});
    mouse_button(app.window,PLATFORM_PRESS);
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_CURSOR,.x=700,.y=500});
    mouse_button(app.window,PLATFORM_RELEASE);
    platform_poll_events();
    assert(!mouse_frame(&app,true,false,16,800.0/60));
    assert(!mouse_frame(&app,false,false,1120,800.0*5/6));
    /* Cancel an active press and queued release on focus loss, even when focus
     * returns in the same batch. Events later in that batch stay discarded. */
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_CURSOR,.x=10,.y=10});
    mouse_button(app.window,PLATFORM_PRESS); platform_poll_events();
    assert(!mouse_frame(&app,true,false,16,800.0/60));
    mouse_button(app.window,PLATFORM_RELEASE);
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_FOCUS,.value=0});
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_FOCUS,.value=1});
    mouse_button(app.window,PLATFORM_PRESS); mouse_button(app.window,PLATFORM_RELEASE);
    platform_poll_events();
    assert(!mouse_frame(&app,false,true,16,800.0/60));
    assert(!mouse_frame(&app,false,false,16,800.0/60));
    /* An overloaded batch cancels capture instead of committing a prefix. */
    mouse_button(app.window,PLATFORM_PRESS); platform_poll_events();
    assert(!mouse_frame(&app,true,false,16,800.0/60));
    for(int i=0;i<INPUT_MOUSE_CAPACITY+2;++i)
      mouse_button(app.window,i%2?PLATFORM_PRESS:PLATFORM_RELEASE);
    platform_poll_events();
    assert(!mouse_frame(&app,false,true,16,800.0/60));
    assert(!mouse_frame(&app,false,false,16,800.0/60));
    mouse_button(app.window,PLATFORM_RELEASE); platform_poll_events();
    assert(!mouse_frame(&app,false,false,16,800.0/60));
    mouse_button(app.window,PLATFORM_PRESS); mouse_button(app.window,PLATFORM_RELEASE);
    platform_poll_events();
    assert(!mouse_frame(&app,true,false,16,800.0/60));
    assert(mouse_frame(&app,false,false,16,800.0/60));
    input_mouse_unregister(app.window);
    assert(platform_set_event_callback(app.window,events)==events);
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_CLOSE}); platform_poll_events();
    assert(platform_window_should_close(app.window));
    input_keyboard_unregister(app.window); input_scroll_unregister(app.window);
    assert(platform_set_key_callback(app.window,NULL)==observer);
    // Queue stale events deliberately: destruction must discard them.
    post(app.window,(PlatformEvent){.kind=PLATFORM_EVENT_CHARACTER,.codepoint='x'});
    ui_text_input_reset(); destroy_window(app.window);
  }
  puts("Platform headless: ordered Unicode/repeat/modifiers, pointer, scroll, focus, HiDPI resize, close, 3 lifecycles: PASS");
  assert(ui_context_destroy(test_ui));
}

void texture_manager_preview_clear(APP *app) {}
