#include "../src/ui/ui_state_frame_internal.h"
#include "../src/ui/ui_state_buttons_internal.h"
/* No application.h, window, collector or platform implementation: only UI core. */
#include "ui/ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ui_test_renderer.h"
#include <assert.h>
void texture_manager_preview_clear(APP *app) {}
void draw_quad(APP *a,float x,float y,float w,float h,float r,float g,float b) {}
void draw_hud_paper_panel(APP *a,float x,float y,float w,float h,const float f[4],const float c[3],bool focus,uint32_t seed) {}
float measure_text_width(Font *f,const char *s,float size) { return 0; }
void draw_text_colored(APP *a,Font *f,const char *s,float x,float y,float size,float r,float g,float b) {}

static void field(char *value, size_t capacity) {
  Font font={.ascender=1};
  ui_text_input("field",&font,ANCHOR_CENTER,0,14,0,0,100,value,(int)capacity,NULL);
}
static bool button(void) {
  Font font={.ascender=1};
  return ui_button_labeled("focus/button","Action",&font,ANCHOR_TOP_LEFT,
                           10,1,0,0,NULL);
}
int main(void) {
  UIFrameInput frame={.width=640,.height=400,.delta_time=0.016f,
                      .mouse_x=12,.mouse_y=18,.scroll_delta=2};
  assert(!ui_begin_frame_input(&frame));
  UIContext *a=ui_context_create(), *b=ui_context_create();
  bind_test_renderer(a); bind_test_renderer(b);
  assert(a && b && ui_context_set_current(a));
  UIFrameInput bad=frame;
  unsigned long long serial=ui_frame_serial_hint();
  assert(!ui_begin_frame_input(NULL));
  bad.width=0; assert(!ui_begin_frame_input(&bad));
  bad=frame; bad.height=NAN; assert(!ui_begin_frame_input(&bad));
  bad=frame; bad.delta_time=-1; assert(!ui_begin_frame_input(&bad));
  bad=frame; bad.scroll_delta=INFINITY; assert(!ui_begin_frame_input(&bad));
  bad=frame; bad.event_count=1; assert(!ui_begin_frame_input(&bad));
  bad.event_count=INPUT_KEYBOARD_CAPACITY+1; assert(!ui_begin_frame_input(&bad));
  assert(ui_frame_serial_hint()==serial && !ui_state_frame(a)->frame_active);
  const InputKeyEvent events[]={
    {.kind=INPUT_CHARACTER,.codepoint=0xe8},
    {.kind=INPUT_KEY,.key=PLATFORM_KEY_LEFT,.action=PLATFORM_PRESS},
    {.kind=INPUT_CHARACTER,.codepoint=0x20ac}
  };
  frame.events=events; frame.event_count=3;
  ui_text_input_focus("field");
  char first[32]="", second[32]="";
  assert(ui_begin_frame_input(&frame));
  assert(!ui_begin_frame_input(&frame));
  assert(ui_frame_serial_hint()==serial+1);
  assert(ui_scroll_delta()==2 && ui_consume_scroll_delta() && !ui_consume_scroll_delta());
  assert(ui_mouse_position().x==12 && ui_mouse_position().y==18);
  field(first,sizeof(first));
  assert(!strcmp(first,"\xe2\x82\xac\xc3\xa8"));
  for(size_t i=0;i<3;++i) assert(!events[i].consumed);
  ui_end_frame();
  assert(ui_context_set_current(b));
  frame.width=900; frame.height=700; frame.scroll_delta=-3;
  ui_text_input_focus("field");
  assert(ui_begin_frame_input(&frame));
  field(second,sizeof(second));
  assert(!strcmp(first,second) && ui_state_frame(b)->canvas_width==900 && ui_state_frame(b)->canvas_height==700);
  assert(ui_state_frame(a)->canvas_width==640 && ui_state_frame(a)->canvas_height==400);
  ui_end_frame();
  assert(ui_context_set_current(a));
  frame.events=NULL; frame.event_count=0; frame.focus_lost=true;
  assert(ui_begin_frame_input(&frame));
  assert(!ui_any_text_input_focused() && ui_state_frame(a)->key_event_count==0);
  field(first,sizeof(first));
  assert(!strcmp(first,second)); /* No replay in an empty batch. */
  ui_end_frame();
  frame=(UIFrameInput){.width=640,.height=400,.delta_time=.016f,
                       .mouse_x=11,.mouse_y=11,.mouse_down=true};
  assert(ui_begin_frame_input(&frame));
  assert(!button() && ui_has_pointer_capture());
  ui_end_frame();
  frame.mouse_down=false; frame.focus_lost=true;
  assert(ui_begin_frame_input(&frame));
  assert(!button()); /* Losing window focus must cancel, not click. */
  assert(!ui_state_frame(a)->g_mouse_clicked && !ui_state_frame(a)->g_mouse_released);
  for(int i=0;i<ui_state_buttons(a)->ui_state_count;++i) assert(!ui_state_buttons(a)->ui_states[i].is_pressed);
  ui_end_frame();
  /* Even an inconsistent host snapshot cannot inject input on focus loss. */
  frame.mouse_down=true; frame.scroll_delta=3; frame.events=events; frame.event_count=3;
  assert(ui_begin_frame_input(&frame));
  assert(!button()&&!ui_state_frame(a)->g_mouse_down&&!ui_state_frame(a)->key_event_count&&ui_scroll_delta()==0);
  ui_end_frame();
  frame.events=NULL; frame.event_count=0; frame.scroll_delta=0;
  frame.focus_lost=false; frame.mouse_down=true;
  assert(ui_begin_frame_input(&frame)); assert(!button()); ui_end_frame();
  frame.mouse_down=false;
  assert(ui_begin_frame_input(&frame)); assert(button()); ui_end_frame();
  Font slider_font={.ascender=1}; float value=.5f;
  frame.mouse_down=true; frame.mouse_x=20;
  frame.mouse_y=10+ui_get_theme().field_height+1;
  assert(ui_begin_frame_input(&frame));
  ui_slider("focus/slider",&slider_font,ANCHOR_TOP_LEFT,10,1,0,0,200,&value,0,1,NULL,NULL);
  assert(ui_slider_is_dragging("focus/slider"));
  ui_end_frame();
  float before_loss=value;
  frame.focus_lost=true; frame.mouse_x=100; /* Host still reports a held button. */
  assert(ui_begin_frame_input(&frame));
  ui_slider("focus/slider",&slider_font,ANCHOR_TOP_LEFT,10,1,0,0,200,&value,0,1,NULL,NULL);
  assert(!ui_slider_is_dragging("focus/slider")&&value==before_loss);
  ui_end_frame();
  assert(ui_context_destroy(a) && ui_context_destroy(b));
  puts("UI explicit frame: PASS (no APP/window/collector, UTF-8 ordering, immutable batch, context canvas, validation, focus)");
  return 0;
}
