#define main navigation_suite_main
#include "ui_navigation_test.c"
#undef main
static int kind,selected;
static bool checked,disabled;
static bool control(void) {
  if(kind==0) return button("target",20,disabled);
  if(kind==1) return ui_checkbox("target","Choice",font,20,20,14,&checked);
  return ui_radio_button("target",font,(UIRect){20,20,180,32},"Choice",1,&selected,disabled);
}
static bool frame(float x,float y,bool down,bool clip,bool lost,bool submit) {
  UIFrameInput input={.width=640,.height=480,.mouse_x=x,.mouse_y=y,.mouse_down=down,.focus_lost=lost,.delta_time=.016f};
  CHECK(ui_begin_frame_input(&input));
  if(clip) CHECK(ui_clip_push((UIRect){0,0,10,10}));
  bool activated=submit&&control();
  if(clip) CHECK(ui_clip_pop());
  ui_end_frame(); return activated;
}
int main(void) {
  for(kind=0;kind<3;++kind) {
    UIContext *c=context(); selected=0; checked=disabled=false;
    CHECK(!frame(300,300,true,false,false,true));
    CHECK(!frame(40,30,true,false,false,true));
    CHECK(!frame(40,30,false,false,false,true)); /* Cannot arm by entering held. */
    CHECK(!frame(40,30,true,true,false,true));
    CHECK(!frame(40,30,false,false,false,true)); /* Hidden press must not arm. */
    CHECK(!frame(40,30,true,false,false,true));
    CHECK(frame(40,30,false,false,false,true));
    selected=0; checked=false;
    CHECK(!frame(40,30,true,false,false,true));
    CHECK(!frame(40,30,false,false,true,true)); /* Focus loss is cancellation. */
    CHECK(!frame(40,30,false,false,false,true));
    CHECK(!frame(40,30,true,false,false,true));
    CHECK(!frame(40,30,true,false,false,false)); /* Temporarily absent control. */
    CHECK(!frame(40,30,true,false,false,true));
    CHECK(!frame(40,30,false,false,false,true));
    if(kind!=1) {
      CHECK(!frame(40,30,true,false,false,true)); disabled=true;
      CHECK(!frame(40,30,true,false,false,true)); disabled=false;
      CHECK(!frame(40,30,false,false,false,true));
    }
    /* A real modal blocks presses on controls outside its scope. */
    begin(NULL,0); control(); bool open=true; float x,y,w,h;
    ui_modal_begin("modal","Modal",font,300,180,&open,&x,&y,&w,&h); ui_modal_end("modal",true); ui_end_frame();
    CHECK(!frame(40,30,true,false,false,true));
    CHECK(!frame(40,30,false,false,false,true));
    CHECK(ui_context_destroy(c));
  }
  UIContext *c=context(); float value=.5f;
  /* Legacy slider now respects the same clips and press-edge rule. */
  float y=10+ui_get_theme().field_height+1;
  for(int n=0;n<3;++n) {
    UIFrameInput input={.width=640,.height=480,.mouse_x=n?20:600,.mouse_y=n?y:400,.mouse_down=n<2};
    CHECK(ui_begin_frame_input(&input));
    ui_slider("slider",font,ANCHOR_TOP_LEFT,10,1,0,0,200,&value,0,1,NULL,NULL);
    CHECK(!ui_slider_is_dragging("slider")); ui_end_frame();
  }
  UIFrameInput input={.width=640,.height=480,.mouse_x=20,.mouse_y=y,.mouse_down=true};
  CHECK(ui_begin_frame_input(&input)); CHECK(ui_clip_push((UIRect){0,0,5,5}));
  ui_slider("slider",font,ANCHOR_TOP_LEFT,10,1,0,0,200,&value,0,1,NULL,NULL);
  CHECK(!ui_slider_is_dragging("slider")&&value==.5f); ui_clip_pop(); ui_end_frame();
  CHECK(clips==0&&ui_context_destroy(c));
  puts("Shared interaction: PASS (clipping, press origin, cancellation, disappearance, disabled/modal blocking, legacy slider)");
}
