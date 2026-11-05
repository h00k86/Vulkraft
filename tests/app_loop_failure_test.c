/* Exercise actual loop gating, without creating a window or calling a driver. */
#ifdef NDEBUG
#undef NDEBUG /* Behavioral assertions must also run in Release CTest. */
#endif
#include "../src/app/application.c"
#include "../src/app/application_legacy.c"
#include <assert.h>
static int fail_at, waits, draws, callbacks, text_ends, ui_ends, popups, polls, shutdowns;
static bool close_window, frame_open, native_close;
bool ui_bind_app_renderer(APP *app) { return true; }
static UIContext *selected;
static bool fail_context, reject_destroy;
static int registrations, destroyed;
UIContext *ui_context_create(void) { return fail_context ? NULL : (UIContext *)1; }
bool app_resource_path(const APP *app,AppResource resource,char *out,size_t size) { snprintf(out,size,"font.json"); return true; }
int font_load(Font *f,const char *p,uint32_t w,uint32_t h) { return 1; }
bool gfx_init(APP *a) { a->window=(PlatformWindow *)1; a->error=(AppError){0}; return true; }
void input_keyboard_register(PlatformWindow *w) { ++registrations; }
void input_mouse_register(PlatformWindow *w) { ++registrations; }
void input_mouse_unregister(PlatformWindow *w) {}
bool input_mouse_frame(APP *a,Point *p,bool *cancelled) { *p=(Point){0}; *cancelled=false; return false; }
void ui_release_input_capture(void) {}
void input_scroll_register(PlatformWindow *w) { ++registrations; }
UIContext *ui_context_get_current(void) { return selected; }
bool ui_context_set_current(UIContext *ctx) { selected=ctx; return true; }
bool ui_context_destroy(UIContext *ctx) { if(reject_destroy) return false; if(ctx) ++destroyed; if(selected==ctx) selected=NULL; return true; }
static APP *current;
static void fail(APP *a,const char *stage) { a->error=(AppError){APP_ERROR_GRAPHICS,stage}; }
double platform_time(void) { static double t; return t+=0.01; }
int platform_window_should_close(PlatformWindow *w) { return close_window; }
void platform_window_set_should_close(PlatformWindow *w,int v) { close_window=v; }
void platform_poll_events(void) { ++polls; if(native_close && polls==1) close_window=true; if(fail_at==1) fail(current,"poll callback"); }
void platform_wait_events(void) { assert(0); }
void platform_framebuffer_size(PlatformWindow *w,int *x,int *y) { *x=800; *y=600; }
int platform_mouse_button(PlatformWindow *w,int b) { return PLATFORM_RELEASE; }
void gfx_recreate_surface(APP *a) { if(fail_at==2) fail(a,"recreate"); }
bool wait_for_current_frame(APP *a) { ++waits; if(fail_at==3) { fail(a,"wait"); return false; } return true; }
Point get_mouse_coordinate(APP *a) { return (Point){0}; }
void ui_begin_frame(APP *a,float x,float y,bool down,float dt) { assert(!frame_open); frame_open=true; }
float input_scroll_take(void) { return 0; }
void ui_set_scroll_delta(float d) {}
void text_begin(APP *a) {}
void app_draw(APP *a) { ++callbacks; if(fail_at==4) fail(a,"draw callback"); }
void ui_draw_dropdown_popups(void) { ++popups; }
void ui_end_frame(void) { assert(frame_open); frame_open=false; ++ui_ends; }
void text_end(APP *a) { ++text_ends; if(fail_at==5) fail(a,"vertex preparation"); }
bool draw_frame(APP *a) { ++draws; if(fail_at==6) { fail(a,"present"); return false; } if(fail_at) close_window=true; return true; }
void input_keyboard_unregister(PlatformWindow *w) {}
void gfx_shutdown(APP *a) { assert(!a->ui); ++shutdowns; a->window=NULL; a->initialized=false; }
typedef struct {
  APP *app;
  AppCallbacks *table;
  int draws, closes, mode;
} CallbackState;
static void callback_draw(APP *app, void *user_data) {
  CallbackState *state=user_data;
  assert(app==state->app && app->running && frame_open);
  ++state->draws;
  if(state->mode==1) { /* Native veto followed by an application close. */
    assert(state->closes==1);
    app_request_close(app);
  } else if(state->mode==2) app_shutdown(app);
  else if(state->mode==3) app_run(app,state->table,state);
  else if(state->mode==4) { /* The table is copied; caller may change it. */
    state->table->draw=NULL;
    if(state->draws==2) app_request_close(app);
  } else app_request_close(app);
}
static bool callback_close(APP *app, void *user_data) {
  CallbackState *state=user_data;
  assert(app==state->app && app->running && !frame_open);
  ++state->closes;
  if(state->mode==1) return state->closes==2;
  if(state->mode==5) app->error=(AppError){APP_ERROR_STATE,"close callback"};
  return state->mode!=5;
}
static void callback_tests(void) {
  for(int mode=0;mode<=7;++mode) {
    APP app={.initialized=true,.ui=(UIContext *)1,.window=(PlatformWindow *)1};
    AppCallbacks table={.draw=callback_draw,.close_requested=callback_close};
    CallbackState state={.app=&app,.table=&table,.mode=mode};
    current=&app; fail_at=0; polls=draws=callbacks=0;
    close_window=false; native_close=(mode==1 || mode==5);
    selected=(UIContext *)2;
    if(mode==6) table.draw=NULL;
    if(mode==7) table.close_requested=NULL;
    app_run(&app,mode==0?NULL:&table,&state);
    assert(!app.running && !frame_open && selected==(UIContext *)2);
    assert(!callbacks); /* app_run never calls legacy app_draw. */
    if(mode==0 || mode==6) assert(app.error.code==APP_ERROR_STATE && !polls && !state.draws);
    if(mode==1) assert(!app.error.code && state.draws==1 && state.closes==2 && close_window);
    if(mode==2 || mode==3) assert(app.error.code==APP_ERROR_STATE && app.ui && app.window && state.draws==1);
    if(mode==4) assert(!app.error.code && state.draws==2 && state.closes==1 && !table.draw);
    if(mode==5) assert(app.error.code==APP_ERROR_STATE && !state.draws && state.closes==1);
    if(mode==7) assert(!app.error.code && state.draws==1 && !state.closes && close_window);
    app_shutdown(&app);
    assert(!app.initialized && !app.ui && !app.window);
  }
  native_close=false;
  app_request_close(NULL);
  APP inactive={0}; app_request_close(&inactive);
}
int main(void) {
  APP lifecycle={0};
  fail_context=true;
  assert(!app_init(&lifecycle));
  assert(lifecycle.error.code==APP_ERROR_MEMORY && !lifecycle.ui && !lifecycle.window);
  assert(shutdowns==1 && registrations==0);
  fail_context=false;
  for(int cycle=0;cycle<3;++cycle) {
    assert(app_init(&lifecycle) && lifecycle.ui);
    reject_destroy=true;
    app_shutdown(&lifecycle);
    assert(lifecycle.ui && lifecycle.window && lifecycle.error.code==APP_ERROR_STATE);
    reject_destroy=false;
    app_shutdown(&lifecycle);
    assert(!lifecycle.ui && !lifecycle.window && !lifecycle.initialized);
  }
  assert(registrations==9 && destroyed==3);

  for(fail_at=1;fail_at<=7;++fail_at) {
    APP app={.initialized=true,.ui=(UIContext *)1,.window=(PlatformWindow *)1}; current=&app;
    waits=draws=callbacks=text_ends=ui_ends=popups=polls=shutdowns=0; close_window=false;
    if(fail_at==2) app.framebuffer_resized=true;
    selected=(UIContext *)2;
    app_loop(&app);
    assert(selected==(UIContext *)2);
    assert(polls==1 && app.error.code==(fail_at==7?APP_ERROR_NONE:APP_ERROR_GRAPHICS));
    if(fail_at<=2) assert(waits==0);
    if(fail_at<=3) assert(callbacks==0 && draws==0 && text_ends==0);
    if(fail_at==4) assert(callbacks==1 && ui_ends==1 && !text_ends && !draws && !popups);
    if(fail_at==5) assert(text_ends==1 && !draws);
    if(fail_at>=6) assert(draws==1);
    if(app.error.code) { app_loop(&app); assert(polls==1); }
    AppError saved=app.error;
    app_shutdown(&app);
    assert(shutdowns==1 && !app.ui && !app.window && app.error.code==saved.code && app.error.stage==saved.stage);
  }
  callback_tests();
  puts("Application loop: PASS (stop at poll/recreate/wait/UI/prepare/present, caller shutdown; callbacks/user data/close veto/reentrancy/legacy)");
}

void ui_text_input_reset(void) {}

void input_scroll_unregister(PlatformWindow *window) {}
