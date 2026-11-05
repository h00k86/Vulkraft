/* Public API only; link the independently compiled overlay/capture modules. */
#include "ui/ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"overlay modules:%d: %s\n",__LINE__,#x); exit(1); } } while(0)
static int clips;
static Font *font=(Font *)1;
static void quad(void *u,float x,float y,float w,float h,float r,float g,float b) {}
static void alpha(void *u,float x,float y,float w,float h,float r,float g,float b,float a) {}
static void text(void *u,Font *f,const char *s,float x,float y,float z,float r,float g,float b) {}
static void image(void *u,float x,float y,float w,float h,int slot,float a,float b,float c,float d) {}
static float measure(void *u,Font *f,const char *s,float z) { return strlen(s)*z*.5f; }
static float ascent(void *u,Font *f,float z) { return z; }
static bool push(void *u,float x,float y,float w,float h) { ++clips; return true; }
static bool pop(void *u) { CHECK(clips>0); --clips; return true; }
static int depth(void *u) { return clips; }
static const UIRenderer renderer={.draw_quad=quad,.draw_quad_alpha=alpha,.draw_text_colored=text,
  .draw_textured_quad=image,.measure_text_width=measure,.font_ascent=ascent,
  .text_clip_push=push,.text_clip_pop=pop,.text_clip_depth=depth};
static InputKeyEvent key(int code) { return (InputKeyEvent){.kind=INPUT_KEY,.key=code,.action=PLATFORM_PRESS}; }
static void begin(float x,float y,bool down,const InputKeyEvent *event) {
  UIFrameInput f={.width=640,.height=480,.mouse_x=x,.mouse_y=y,.mouse_down=down,.delta_time=.016f,
    .events=event,.event_count=event?1:0};
  CHECK(ui_begin_frame_input(&f));
}
static bool button(const char *id,float x,float y) {
  UIButtonStyle style={.min_width=160,.max_width=160,.min_height=30};
  return ui_button_labeled(id,id,font,ANCHOR_TOP_LEFT,0,14,x,y,&style);
}
static void field(char *buffer) { ui_text_input("edit",font,ANCHOR_TOP_LEFT,0,14,20,20,200,buffer,64,NULL); }
static const char *options[]={"First","Second"};
static bool dropdown(const char *id,float x,float y,int *selected) {
  UIButtonStyle style={.min_height=30};
  return ui_dropdown(id,font,ANCHOR_TOP_LEFT,0,14,x,y,160,options,2,selected,NULL,&style);
}
static const UIMenuItem action[]={ {.id="action",.label="Action",.kind=UI_MENU_ITEM_ACTION} };
static void menu_capture(void) {
  char buffer[64]="base";
  ui_text_input_focus("edit"); begin(-20,-20,false,NULL); field(buffer); ui_end_frame();
  InputKeyEvent e={.kind=INPUT_CHARACTER,.codepoint='#'};
  begin(-20,-20,false,&e); field(buffer); ui_end_frame(); CHECK(!strcmp(buffer,"base#"));
  ui_menu_open_context("root",250,100,180,action,1);
  e=(InputKeyEvent){.kind=INPUT_CHARACTER,.codepoint='!'};
  begin(-20,-20,false,&e); field(buffer); ui_menu_render_overlay(font); ui_end_frame();
  CHECK(!strcmp(buffer,"base#"));
  UIInputDiagnostics d; CHECK(ui_get_input_diagnostics(&d));
  CHECK(d.keyboard_capture&&!strcmp(d.highest_priority_owner,"root"));
  /* Escape belongs to the menu; the editor must keep its current text. */
  e=key(PLATFORM_KEY_ESCAPE);
  begin(-20,-20,false,&e); field(buffer); ui_menu_render_overlay(font); ui_end_frame();
  CHECK(!ui_menu_hierarchy_is_open()&&!strcmp(buffer,"base#"));
  CHECK(ui_get_input_diagnostics(&d)&&d.escape_consumed);
  e=(InputKeyEvent){.kind=INPUT_CHARACTER,.codepoint='?'};
  begin(-20,-20,false,&e); field(buffer); ui_end_frame(); CHECK(!strcmp(buffer,"base#?"));


  ui_menu_open_context("root",250,100,180,action,1);
  begin(40,30,true,NULL);
  CHECK(!button("before",20,20));
  ui_menu_render_overlay(font); CHECK(!ui_menu_hierarchy_is_open());
  CHECK(!button("after",20,20)); ui_end_frame();
  begin(40,30,false,NULL);
  CHECK(!button("before",20,20)&&!button("after",20,20)); ui_end_frame();
}
static void dropdown_capture(void) {
  int selected=0;
  begin(-20,-20,false,NULL); dropdown("choice",20,100,&selected); ui_end_frame();
  ui_dropdown_set_open("choice",true);
  begin(-20,-20,false,NULL); dropdown("choice",20,100,&selected); ui_draw_dropdown_popups(); ui_end_frame();
  CHECK(ui_dropdown_is_open("choice"));
  begin(320,30,true,NULL); CHECK(!button("before",300,20));
  CHECK(!dropdown("choice",20,100,&selected));
  CHECK(!button("after",300,20)); ui_draw_dropdown_popups(); ui_end_frame();
  CHECK(!ui_dropdown_is_open("choice")&&selected==0);
  begin(320,30,false,NULL);
  CHECK(!button("before",300,20)&&!button("after",300,20)); ui_end_frame();

  /* Overlapping popups: only the last submitted popup selects an option. */
  int a=0,b=0;
  begin(-20,-20,false,NULL); dropdown("a",20,100,&a); dropdown("b",20,100,&b); ui_end_frame();
  ui_dropdown_set_open("a",true); ui_dropdown_set_open("b",true);
  begin(-20,-20,false,NULL); dropdown("a",20,100,&a); dropdown("b",20,100,&b);
  ui_draw_dropdown_popups(); ui_end_frame();
  begin(40,175,true,NULL); CHECK(!button("under",20,160));
  CHECK(!dropdown("a",20,100,&a)); CHECK(dropdown("b",20,100,&b));
  CHECK(!button("after",20,160)); ui_draw_dropdown_popups(); ui_end_frame();
  CHECK(a==0&&b==1&&ui_dropdown_is_open("a")&&!ui_dropdown_is_open("b"));
  InputKeyEvent e=key(PLATFORM_KEY_ESCAPE);
  begin(-20,-20,false,&e); dropdown("a",20,100,&a); ui_draw_dropdown_popups(); ui_end_frame();
  CHECK(!ui_dropdown_is_open("a")&&a==0);
}
static void modal_frame(bool *open,int *selected,char *origin,const InputKeyEvent *e) {
  begin(-20,-20,false,e); field(origin);
  float x,y,w,h;
  if(ui_modal_begin("dialog","Dialog",font,400,240,open,&x,&y,&w,&h))
    dropdown("inside",x,y,selected);
  ui_modal_end("dialog",*open); ui_draw_dropdown_popups(); ui_end_frame();
}
static void modal_escape(void) {
  char origin[64]="origin"; int selected=0; bool open=false;
  ui_text_input_focus("edit"); begin(-20,-20,false,NULL); field(origin); ui_end_frame();
  open=true; modal_frame(&open,&selected,origin,NULL); CHECK(open);
  InputKeyEvent e=key(PLATFORM_KEY_TAB); modal_frame(&open,&selected,origin,&e);
  CHECK(!strcmp(ui_keyboard_focus_id(),"inside"));
  e=key(PLATFORM_KEY_ENTER); modal_frame(&open,&selected,origin,&e);
  CHECK(open&&ui_dropdown_is_open("inside"));
  e=key(PLATFORM_KEY_ESCAPE); modal_frame(&open,&selected,origin,&e);
  CHECK(open&&!ui_dropdown_is_open("inside")&&selected==0);
  modal_frame(&open,&selected,origin,&e);
  CHECK(!open&&!strcmp(ui_keyboard_focus_id(),"edit"));
}
static void submenu_escape(void) {
  const UIMenuItem root[]={ {.id="parent",.label="Parent",.submenu_id="child",.kind=UI_MENU_ITEM_SUBMENU} };
  CHECK(ui_menu_submit_submenu("root","parent","child",action,1));
  ui_menu_open_context("root",150,100,180,root,1);
  begin(-20,-20,false,NULL); ui_menu_render_overlay(font); ui_end_frame();
  UIMenuDiagnostics d; CHECK(ui_menu_get_diagnostics(&d)&&d.active_depth==1);
  begin(d.popup[0].x+20,d.popup[0].y+10,false,NULL); ui_menu_render_overlay(font); ui_end_frame();
  CHECK(ui_menu_get_diagnostics(&d)&&d.active_depth==2);
  InputKeyEvent e=key(PLATFORM_KEY_ESCAPE);
  begin(-20,-20,false,&e); ui_menu_render_overlay(font); ui_end_frame();
  CHECK(ui_menu_get_diagnostics(&d)&&d.active_depth==1&&d.escape_consumed);
  begin(-20,-20,false,&e); ui_menu_render_overlay(font); ui_end_frame();
  CHECK(!ui_menu_hierarchy_is_open());
}
int main(void) {
  UIContext *ctx=ui_context_create();
  CHECK(ctx&&ui_context_set_current(ctx)&&ui_context_set_renderer(ctx,&renderer,NULL));
  menu_capture(); CHECK(ui_context_reset(ctx));
  dropdown_capture(); CHECK(ui_context_reset(ctx));
  modal_escape(); CHECK(ui_context_reset(ctx));
  submenu_escape(); CHECK(clips==0&&ui_context_destroy(ctx));
  puts("Overlay modules: PASS (menu keyboard capture, click dismissal, top dropdown ownership, modal/dropdown Escape, focus restore, submenu Escape)");
  return 0;
}
