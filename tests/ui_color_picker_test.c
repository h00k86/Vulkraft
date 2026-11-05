#define main navigation_suite_main
#include "ui_navigation_test.c"
#undef main
#include "../examples/ui_reference_scene.c"
static UIReferenceScene scene;
static void draw_options(const InputKeyEvent *event) {
  begin(event,event?1:0); ui_reference_draw(&scene,font,640,480,"."); ui_end_frame();
}
static void activate(const char *id) {
  InputKeyEvent tab=key(PLATFORM_KEY_TAB,0),enter=key(PLATFORM_KEY_ENTER,0);
  for(int n=0;n<100&&strcmp(ui_keyboard_focus_id(),id);++n) draw_options(&tab);
  CHECK(!strcmp(ui_keyboard_focus_id(),id)); draw_options(&enter);
}
static bool same(UIColor a,UIColor b) { return fabsf(a.r-b.r)<.00001f&&fabsf(a.g-b.g)<.00001f&&fabsf(a.b-b.b)<.00001f; }
static bool extended_frame(UIColor *color,const UIColorPickerOptions *options,
                            bool *started,float x,float y,bool down,bool lost,
                            const InputKeyEvent *events,size_t count) {
  UIFrameInput f={.width=640,.height=480,.mouse_x=x,.mouse_y=y,.mouse_down=down,
                 .focus_lost=lost,.events=events,.event_count=count};
  CHECK(ui_begin_frame_input(&f)); label_count=0;
  UIPropertyLayout p=ui_property_layout_begin(20,20,500,1);
  bool changed=ui_property_color_ex(&p,"extended",font,"Colore",color,options,started);
  ui_end_frame(); return changed;
}
static void extended_picker(void) {
  UIContext *c=context();
  UIColorPickerOptions integer={.integer_channels=true};
  UIColor color={powf(137.006f/255,2.2f),powf(30.333f/255,2.2f),.123456f},original=color;
  bool started=true;
  CHECK(!extended_frame(&color,&integer,&started,-20,-20,false,false,NULL,0)&&!started);
  CHECK(color.r==original.r&&color.g==original.g&&color.b==original.b);
  CHECK(!strcmp(label_trace[9],"137")&&!strcmp(label_trace[11],"30"));
  CHECK(!extended_frame(&color,NULL,&started,-20,-20,false,false,NULL,0)&&!started);
  CHECK(strchr(label_trace[9],'.')&&fabsf(strtof(label_trace[9],NULL)-137.006f)<.001f);
  /* A numeric edit opens with exact text even when idle labels are rounded. */
  UITheme theme=ui_get_theme();
  float red_y=20+theme.field_height+theme.row_spacing;
  CHECK(!extended_frame(&color,&integer,&started,500,red_y+5,true,false,NULL,0)&&!started);
  CHECK(strchr(label_trace[9],'.')&&ui_has_keyboard_capture());
  extended_frame(&color,&integer,&started,500,red_y+5,false,false,NULL,0);
  InputKeyEvent enter=key(PLATFORM_KEY_ENTER,0);
  CHECK(!extended_frame(&color,&integer,&started,500,red_y+5,false,false,&enter,1)&&!started);
  CHECK(color.r==original.r&&color.g==original.g&&color.b==original.b);
  CHECK(!strcmp(label_trace[9],"137"));
  /* One start per gesture; no dependence on child widget IDs. */
  float track_y=red_y+theme.field_height+theme.slider_track_height*.5f;
  CHECK(extended_frame(&color,&integer,&started,100,track_y,true,false,NULL,0)&&started);
  CHECK(extended_frame(&color,&integer,&started,200,track_y,true,false,NULL,0)&&!started);
  extended_frame(&color,&integer,&started,200,track_y,false,false,NULL,0); CHECK(!started);
  extended_frame(&color,&integer,&started,200,track_y,true,false,NULL,0); CHECK(started);
  extended_frame(&color,&integer,&started,220,track_y,true,true,NULL,0); CHECK(!started&&!ui_has_input_capture());
  extended_frame(&color,&integer,&started,25,25,false,false,NULL,0);
  extended_frame(&color,&integer,&started,25,25,true,false,NULL,0); CHECK(!started);
  CHECK(extended_frame(&color,&integer,&started,25,25,false,false,NULL,0)&&!started);
  CHECK(same(color,ui_color_srgb(0xe0524f)));
  InputKeyEvent tab=key(PLATFORM_KEY_TAB,0),end=key(PLATFORM_KEY_END,0);
  for(int n=0;n<8;++n) {
    extended_frame(&color,&integer,&started,-20,-20,false,false,&tab,1); CHECK(!started);
  }
  CHECK(extended_frame(&color,&integer,&started,-20,-20,false,false,&end,1)&&!started);
  CHECK(same(color,ui_color_srgb(0xff524f)));
  for(int channel=1;channel<3;++channel) {
    float channel_y=track_y+channel*(theme.field_height+theme.row_spacing);
    extended_frame(&color,&integer,&started,100,channel_y,true,false,NULL,0); CHECK(started);
    extended_frame(&color,&integer,&started,120,channel_y,true,false,NULL,0); CHECK(!started);
    extended_frame(&color,&integer,&started,120,channel_y,false,false,NULL,0); CHECK(!started);
  }
  /* Invalid input always resets the output flag and leaves data untouched. */
  started=true; original=color;
  CHECK(!ui_property_color_ex(NULL,"bad",font,"Color",&color,&integer,&started)&&!started);
  CHECK(same(color,original));
  CHECK(ui_context_destroy(c));
}
int main(void) {
  extended_picker();
  UIContext *c=context();
  UIColor color=ui_color_srgb(0xab34eb),original=color;
  begin(NULL,0);
  UIPropertyLayout p=ui_property_layout_begin(20,20,500,1);
  CHECK(!ui_property_color(&p,"color",font,"Colore",&color)&&same(color,original));
  ui_end_frame();
  InputKeyEvent tab=key(PLATFORM_KEY_TAB,0),enter=key(PLATFORM_KEY_ENTER,0);
  begin(&tab,1); p=ui_property_layout_begin(20,20,500,1); ui_property_color(&p,"color",font,"Colore",&color); ui_end_frame();
  focus("color/preset/0");
  begin(&enter,1); p=ui_property_layout_begin(20,20,500,1);
  CHECK(ui_property_color(&p,"color",font,"Colore",&color)); ui_end_frame(); CHECK(same(color,ui_color_srgb(0xe0524f)));
  /* Reach the red slider after the eight preset buttons. */
  for(int n=0;n<8;++n) { begin(&tab,1); p=ui_property_layout_begin(20,20,500,1); ui_property_color(&p,"color",font,"Colore",&color); ui_end_frame(); }
  focus("color/red");
  InputKeyEvent end=key(PLATFORM_KEY_END,0);
  begin(&end,1); p=ui_property_layout_begin(20,20,500,1); CHECK(ui_property_color(&p,"color",font,"Colore",&color)); ui_end_frame();
  CHECK(same(color,ui_color_srgb(0xff524f)));
  begin(NULL,0); p=ui_property_layout_begin(20,20,500,1); color=(UIColor){NAN,2,-1};
  CHECK(ui_property_color(&p,"color",font,"Colore",&color)&&same(color,(UIColor){0,1,0})); ui_end_frame();
  CHECK(clips==0&&ui_context_destroy(c));
  c=context(); ui_reference_init(&scene); scene.options=true;
  draw_options(NULL);
  activate("reference/accent/preset/3");
  CHECK(scene.custom_accent&&same(scene.accent,ui_color_srgb(0x48bd78))&&same(ui_get_theme().accent,scene.accent));
  activate("reference/options-close"); CHECK(!scene.options);
  activate("reference/theme"); CHECK(scene.light&&same(ui_get_theme().accent,scene.accent));
  activate("reference/options"); CHECK(scene.options);
  activate("reference/reset-accent"); CHECK(!scene.custom_accent&&same(ui_get_theme().accent,ui_theme_light().accent));
  CHECK(same(ui_get_theme().selected,ui_theme_light().selected));
  CHECK(clips==0&&ui_context_destroy(c));
  puts("Color picker: PASS (sRGB, presets, public drag start, integer presentation, exact edits, keyboard RGB, normalization, live accent, theme switch, reset)");
}
