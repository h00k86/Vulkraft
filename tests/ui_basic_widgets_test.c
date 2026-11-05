#define main navigation_suite_main
#include "ui_navigation_test.c"
#undef main
static bool leaked;
static int tooltip_draws;
static void observed_text(void *u,Font *f,const char *s,float x,float y,float z,float r,float g,float b) {
  if(strstr(s,"è")||strstr(s,"€")) leaked=true;
  if(!strcmp(s,"Helpful")) ++tooltip_draws;
}
static float observed_measure(void *u,Font *f,const char *s,float z) {
  if(strstr(s,"è")||strstr(s,"€")) leaked=true;
  return strlen(s)*z*.5f;
}
static float widths[4]; static int rectangle_count;
static void observed_quad(void *u,float x,float y,float w,float h,float r,float g,float b) {
  CHECK(isfinite(x)&&isfinite(y)&&isfinite(w)&&isfinite(h)&&w>=0&&h>=0);
  if(rectangle_count<4) widths[rectangle_count]=w;
  ++rectangle_count;
}
static void password(char *buffer,bool *revealed) {
  UIPropertyLayout p=ui_property_layout_begin(10,20,500,1);
  ui_property_password(&p,"password",font,"Password",buffer,128,revealed);
}
static int copies;
static bool write_clip(void *u,const char *text) { ++copies; return true; }
int main(void) {
  UIContext *c=context(); UIRenderer observed=renderer;
  observed.draw_text_colored=observed_text; observed.measure_text_width=observed_measure;
  observed.draw_quad=observed_quad; CHECK(ui_context_set_renderer(c,&observed,NULL));
  int choice=0;
  #define RADIO() do { ui_radio_button("one",font,(UIRect){20,20,120,30},"One",0,&choice,false); \
    ui_radio_button("disabled",font,(UIRect){20,55,120,30},"Disabled",2,&choice,true); \
    ui_radio_button("two",font,(UIRect){20,90,120,30},"Two",1,&choice,false); } while(0)
  begin(NULL,0); RADIO(); ui_end_frame();
  InputKeyEvent tab=key(PLATFORM_KEY_TAB,0),space=key(PLATFORM_KEY_SPACE,0);
  begin(&tab,1); RADIO(); ui_end_frame(); focus("one");
  begin(&tab,1); RADIO(); ui_end_frame(); focus("two");
  begin(&space,1); RADIO(); ui_end_frame(); CHECK(choice==1);
  begin(NULL,0); rectangle_count=0;
  ui_progress_bar(font,(UIRect){0,0,200,20},.25f,false,0,NULL); CHECK(widths[1]==50);
  rectangle_count=0; ui_progress_bar(font,(UIRect){0,0,200,20},2,false,0,NULL); CHECK(widths[1]==200);
  ui_progress_bar(font,(UIRect){0,0,200,20},NAN,true,3e38f,NULL); RADIO(); ui_end_frame();
  /* Tooltip delay, keyboard focus, disappearance and per-context ownership. */
  for(int n=0;n<5;++n) { begin(NULL,0); RADIO(); ui_tooltip("two",font,(UIRect){20,90,120,30},"Helpful",.05f); ui_end_frame(); }
  CHECK(tooltip_draws==1);
  begin(NULL,0); RADIO(); ui_end_frame(); CHECK(tooltip_draws==1);
  CHECK(ui_context_destroy(c));
  c=context(); CHECK(ui_context_set_renderer(c,&observed,NULL));
  UIClipboard clipboard={.write=write_clip}; CHECK(ui_context_set_clipboard(c,&clipboard,NULL));
  char value[128]="sè€cret"; bool revealed=false;
  begin(NULL,0); password(value,&revealed); ui_end_frame(); CHECK(!leaked);
  begin(&tab,1); password(value,&revealed); ui_end_frame(); focus("password/text");
  InputKeyEvent all=key(PLATFORM_KEY_A,PLATFORM_MOD_CONTROL),copy=key(PLATFORM_KEY_C,PLATFORM_MOD_CONTROL),cut=key(PLATFORM_KEY_X,PLATFORM_MOD_CONTROL);
  begin(&all,1); password(value,&revealed); ui_end_frame();
  begin(&copy,1); password(value,&revealed); ui_end_frame();
  begin(&cut,1); password(value,&revealed); ui_end_frame(); CHECK(!copies&&!strcmp(value,"sè€cret")&&!leaked);
  InputKeyEvent cp=character(0x1f642);
  begin(&cp,1); password(value,&revealed); ui_end_frame(); CHECK(!strcmp(value,"🙂")&&!leaked);
  InputKeyEvent undo=key(PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL);
  begin(&undo,1); password(value,&revealed); ui_end_frame(); CHECK(!strcmp(value,"sè€cret")&&!leaked);
  begin(&tab,1); password(value,&revealed); ui_end_frame(); focus("password/reveal");
  begin(&space,1); password(value,&revealed); ui_end_frame(); CHECK(revealed);
  begin(NULL,0); password(value,&revealed); ui_end_frame(); CHECK(leaked);
  revealed=false; leaked=false;
  begin(NULL,0); password(value,&revealed); ui_end_frame(); CHECK(!leaked&&clips==0);
  CHECK(ui_context_destroy(c));
  puts("Basic widgets: PASS (radio exclusivity/disabled/focus, progress bounds, tooltip delay, UTF-8 password masking/reveal/undo, no copy/cut)");
}
