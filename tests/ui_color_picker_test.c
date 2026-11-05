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
int main(void) {
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
  puts("Color picker: PASS (sRGB, presets, keyboard RGB, normalization, live accent, theme switch, reset)");
}
