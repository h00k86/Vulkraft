#define main navigation_suite_main
#include "ui_navigation_test.c"
#undef main
static void pointer(float x,float y,bool down,bool lost) {
  UIFrameInput frame={.width=640,.height=480,.mouse_x=x,.mouse_y=y,.mouse_down=down,.focus_lost=lost};
  CHECK(ui_begin_frame_input(&frame));
}
int main(void) {
  UIContext *c=context();
  const UITab tabs[]={{"Prima",false},{"Disabilitata",true},{"Terza",false}};
  int selected=-1; UIRect area={20,20,300,32},a,b;
  begin(NULL,0); CHECK(ui_tabs("tabs",font,area,tabs,3,&selected)); CHECK(selected==0); ui_end_frame();
  InputKeyEvent e=key(PLATFORM_KEY_TAB,0);
  begin(&e,1); ui_tabs("tabs",font,area,tabs,3,&selected); ui_end_frame(); focus("tabs");
  e=key(PLATFORM_KEY_RIGHT,0);
  begin(&e,1); CHECK(ui_tabs("tabs",font,area,tabs,3,&selected)); ui_end_frame(); CHECK(selected==2);
  begin(&e,1); ui_tabs("tabs",font,area,tabs,3,&selected); ui_end_frame(); CHECK(selected==0);
  e=key(PLATFORM_KEY_END,0);
  begin(&e,1); ui_tabs("tabs",font,area,tabs,3,&selected); ui_end_frame(); CHECK(selected==2);
  pointer(150,30,true,false); CHECK(!ui_tabs("tabs",font,area,tabs,3,&selected)); ui_end_frame();
  pointer(30,30,false,false); ui_tabs("tabs",font,area,tabs,3,&selected); ui_end_frame();
  pointer(30,30,true,false); CHECK(ui_tabs("tabs",font,area,tabs,3,&selected)); ui_end_frame(); CHECK(selected==0);
  const UITab disabled[]={{"No",true}};
  begin(NULL,0); CHECK(ui_tabs("tabs",font,area,disabled,1,&selected)); CHECK(selected==-1);
  CHECK(!ui_tabs("tabs",font,area,tabs,65,&selected));
  CHECK(ui_clip_push((UIRect){0,0,10,10}));
  ui_tabs("clipped",font,area,tabs,3,&selected); ui_clip_pop(); ui_end_frame();
  float ratio=.5f; UIRect bounds={20,100,300,160};
  #define SPLIT() ui_splitter("split",bounds,UI_SPLIT_COLUMNS,8,40,60,&ratio,&a,&b)
  pointer(169,120,true,false); SPLIT(); ui_end_frame(); CHECK(ratio==.5f&&ui_slider_is_dragging("split"));
  pointer(219,120,true,false); CHECK(SPLIT()); ui_end_frame(); CHECK(fabsf(ratio-(.5f+50.0f/292))<.0001f);
  CHECK(fabsf(a.w+b.w+8-300)<.001f && b.x==a.x+a.w+8);
  pointer(1000,120,true,false); SPLIT(); ui_end_frame(); CHECK(fabsf(b.w-60)<.001f);
  float held=ratio;
  pointer(30,120,true,true); SPLIT(); ui_end_frame(); CHECK(ratio==held&&!ui_slider_is_dragging("split"));
  pointer(169,120,false,false); SPLIT(); ui_end_frame();
  e=key(PLATFORM_KEY_TAB,0); begin(&e,1); SPLIT(); ui_end_frame(); focus("split");
  e=key(PLATFORM_KEY_HOME,0); begin(&e,1); SPLIT(); ui_end_frame(); CHECK(fabsf(a.w-40)<.001f);
  e=key(PLATFORM_KEY_END,0); begin(&e,1); SPLIT(); ui_end_frame(); CHECK(fabsf(b.w-60)<.001f);
  begin(NULL,0);
  ratio=NAN; CHECK(ui_splitter("rows",bounds,UI_SPLIT_ROWS,8,40,60,&ratio,&a,&b));
  CHECK(isfinite(ratio)&&a.h>=40&&b.h>=60&&b.y==a.y+a.h+8);
  ratio=.5f; ui_splitter("tiny",(UIRect){0,0,38,20},UI_SPLIT_COLUMNS,8,40,60,&ratio,&a,&b);
  CHECK(fabsf(a.w-12)<.001f&&fabsf(b.w-18)<.001f);
  CHECK(!ui_splitter("bad",bounds,UI_SPLIT_COLUMNS,NAN,0,0,&ratio,&a,&b)&&a.w==0&&b.w==0);
  ui_end_frame();
  /* A clipped handle cannot acquire capture. */
  ratio=.5f; pointer(169,120,true,false);
  CHECK(ui_clip_push((UIRect){0,0,10,10})); SPLIT(); ui_clip_pop(); ui_end_frame();
  CHECK(!ui_slider_is_dragging("split"));
  CHECK(clips==0&&ui_context_destroy(c));
  puts("Tabs/splitter: PASS (selection, disabled tabs, keyboard, drag, bounds, focus loss, clips, small areas)");
  return 0;
}
