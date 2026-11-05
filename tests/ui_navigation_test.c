/* Real core with a deterministic renderer; CHECK remains active in release. */
#include "ui/ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"navigation:%d: %s\n",__LINE__,#x); exit(1); } } while(0)
static int clips, quads;
static void quad(void *u,float x,float y,float w,float h,float r,float g,float b) { ++quads; }
static void alpha(void *u,float x,float y,float w,float h,float r,float g,float b,float a) { ++quads; }
static void text(void *u,Font *f,const char *s,float x,float y,float z,float r,float g,float b) {}
static void image(void *u,float x,float y,float w,float h,int slot,float a,float b,float c,float d) {}
static float measure(void *u,Font *f,const char *s,float z) { return strlen(s)*z*.5f; }
static float ascent(void *u,Font *f,float z) { return z; }
static bool push(void *u,float x,float y,float w,float h) { ++clips; return true; }
static bool pop(void *u) { CHECK(clips>0); --clips; return true; }
static int depth(void *u) { return clips; }
static const UIRenderer renderer={.draw_quad=quad,.draw_quad_alpha=alpha,
  .draw_text_colored=text,.draw_textured_quad=image,.measure_text_width=measure,
  .font_ascent=ascent,.text_clip_push=push,.text_clip_pop=pop,.text_clip_depth=depth};
static Font *font=(Font *)1;
static InputKeyEvent key(int k,int mods) {
  return (InputKeyEvent){.kind=INPUT_KEY,.key=k,.action=PLATFORM_PRESS,.mods=mods};
}
static InputKeyEvent character(unsigned cp) { return (InputKeyEvent){.kind=INPUT_CHARACTER,.codepoint=cp}; }
static void begin(const InputKeyEvent *events,size_t count) {
  UIFrameInput f={.width=640,.height=480,.mouse_x=-20,.mouse_y=-20,.delta_time=.016f,
                 .events=events,.event_count=count};
  CHECK(ui_begin_frame_input(&f));
}
static bool button(const char *id,float y,bool disabled) {
  UIButtonStyle style={.disabled=disabled};
  return ui_button_labeled(id,id,font,ANCHOR_TOP_LEFT,0,14,20,y,&style);
}
static void field(const char *id,float y,char *buffer) {
  ui_text_input(id,font,ANCHOR_TOP_LEFT,0,14,20,y,200,buffer,64,NULL);
}
static void focus(const char *id) { if(strcmp(ui_keyboard_focus_id(),id)) fprintf(stderr,"focus expected=%s actual=%s\n",id,ui_keyboard_focus_id()); CHECK(!strcmp(ui_keyboard_focus_id(),id)); }
static UIContext *context(void) {
  UIContext *c=ui_context_create(); CHECK(c&&ui_context_set_current(c));
  CHECK(ui_context_set_renderer(c,&renderer,NULL)); return c;
}
static void traversal(void) {
  UIContext *c=context(); bool checked=false; int clicks=0; char a[64]="",b[64]="";
  #define FORM() do { clicks+=button("first",20,false); button("disabled",50,true); \
    ui_checkbox("check","Check",font,20,90,14,&checked); field("a",120,a); field("b",160,b); } while(0)
  begin(NULL,0); FORM(); ui_end_frame(); focus("");
  InputKeyEvent e=key(PLATFORM_KEY_TAB,0);
  begin(&e,1); FORM(); ui_end_frame(); focus("first");
  CHECK(!e.consumed&&ui_has_keyboard_capture());
  e=key(PLATFORM_KEY_ENTER,0); begin(&e,1); FORM(); ui_end_frame(); CHECK(clicks==1);
  e.action=PLATFORM_REPEAT; begin(&e,1); FORM(); ui_end_frame(); CHECK(clicks==1);
  e=key(PLATFORM_KEY_TAB,0); begin(&e,1); FORM(); ui_end_frame(); focus("check");
  e=key(PLATFORM_KEY_SPACE,0); begin(&e,1); FORM(); ui_end_frame(); CHECK(checked);
  e=key(PLATFORM_KEY_TAB,0); begin(&e,1); FORM(); ui_end_frame(); focus("a");
  InputKeyEvent burst[]={character(0xe8),key(PLATFORM_KEY_TAB,0),character(0x20ac)};
  begin(burst,3); FORM(); ui_end_frame(); focus("b"); CHECK(!strcmp(a,"è")&&!strcmp(b,"€"));
  InputKeyEvent reverse[]={character('!'),key(PLATFORM_KEY_TAB,PLATFORM_MOD_SHIFT),character('?')};
  begin(reverse,3); FORM(); ui_end_frame(); focus("a"); CHECK(!strcmp(a,"è?")&&!strcmp(b,"€!"));
  e=key(PLATFORM_KEY_ESCAPE,0); begin(&e,1); FORM(); ui_end_frame(); CHECK(!strcmp(a,"è"));
  e=key(PLATFORM_KEY_TAB,PLATFORM_MOD_SHIFT); begin(&e,1); FORM(); ui_end_frame(); focus("check");
  begin(NULL,0); button("first",20,false); field("a",120,a); field("b",160,b); ui_end_frame(); focus("a");
  e=key(PLATFORM_KEY_TAB,PLATFORM_MOD_SHIFT); begin(&e,1); FORM(); ui_end_frame(); focus("first");
  begin(NULL,0); button("first",20,true); field("a",120,a); field("b",160,b); ui_end_frame(); focus("a");
  UIFrameInput lost={.width=640,.height=480,.focus_lost=true};
  CHECK(ui_begin_frame_input(&lost)); FORM(); ui_end_frame(); focus(""); CHECK(!ui_has_keyboard_capture());
  CHECK(ui_context_destroy(c));
  #undef FORM
}
static void choices(void) {
  UIContext *c=context(); float value=.5f; int selected=0;
  const char *options[]={"one","two","three"};
  #define CHOICES() do { ui_slider("slider",font,ANCHOR_TOP_LEFT,10,1,0,0,200,&value,0,1,NULL,NULL); \
    ui_dropdown("choice",font,ANCHOR_TOP_LEFT,0,14,20,100,200,options,3,&selected,NULL,NULL); } while(0)
  begin(NULL,0); CHOICES(); ui_end_frame();
  InputKeyEvent e=key(PLATFORM_KEY_TAB,0); begin(&e,1); CHOICES(); ui_end_frame(); focus("slider");
  e=key(PLATFORM_KEY_RIGHT,0); begin(&e,1); CHOICES(); ui_end_frame(); CHECK(value>.5f);
  e=key(PLATFORM_KEY_END,0); begin(&e,1); CHOICES(); ui_end_frame(); CHECK(value==1);
  InputKeyEvent tabs[]={key(PLATFORM_KEY_TAB,0),key(PLATFORM_KEY_TAB,0)};
  begin(tabs,2); CHOICES(); ui_end_frame(); focus("choice");
  e=key(PLATFORM_KEY_ENTER,0); begin(&e,1); CHOICES(); ui_end_frame(); CHECK(ui_dropdown_is_open("choice"));
  e=key(PLATFORM_KEY_DOWN,0); begin(&e,1); CHOICES(); ui_end_frame(); CHECK(selected==0);
  e=key(PLATFORM_KEY_ESCAPE,0); begin(&e,1); CHOICES(); ui_end_frame(); CHECK(!ui_dropdown_is_open("choice")&&selected==0);
  InputKeyEvent pick[]={key(PLATFORM_KEY_ENTER,0),key(PLATFORM_KEY_END,0),key(PLATFORM_KEY_ENTER,0)};
  begin(pick,3); CHOICES(); ui_end_frame(); CHECK(selected==2&&!ui_dropdown_is_open("choice"));
  CHECK(ui_context_destroy(c));
  #undef CHOICES
}
static void modals(void) {
  UIContext *c=context(); bool open=false; char buffer[64]="original"; int background=0;
  #define MODAL() do { if(button("open",20,false)) open=true; background+=button("background",60,false); \
    if(open) { float x,y,w,h; if(ui_modal_begin("dialog","Dialog",font,400,240,&open,&x,&y,&w,&h)) { \
      field("edit",y,buffer); if(button("close",y+50,false)) open=false; } ui_modal_end("dialog",open); } } while(0)
  begin(NULL,0); MODAL(); ui_end_frame();
  InputKeyEvent e=key(PLATFORM_KEY_TAB,0); begin(&e,1); MODAL(); ui_end_frame(); focus("open");
  e=key(PLATFORM_KEY_ENTER,0); begin(&e,1); MODAL(); ui_end_frame(); CHECK(open); focus("dialog_modal_close");
  e=key(PLATFORM_KEY_TAB,0); begin(&e,1); MODAL(); ui_end_frame(); focus("edit");
  e=character('!'); begin(&e,1); MODAL(); ui_end_frame(); CHECK(!strcmp(buffer,"original!"));
  e=key(PLATFORM_KEY_ESCAPE,0); begin(&e,1); MODAL(); ui_end_frame(); CHECK(open&&!strcmp(buffer,"original"));
  e=key(PLATFORM_KEY_TAB,0); begin(&e,1); MODAL(); ui_end_frame(); focus("close");
  begin(&e,1); MODAL(); ui_end_frame(); focus("dialog_modal_close");
  e=key(PLATFORM_KEY_ENTER,0); begin(&e,1); MODAL(); ui_end_frame(); CHECK(!open&&!background); focus("open");
  begin(&e,1); MODAL(); ui_end_frame(); CHECK(open);
  open=false; begin(NULL,0); MODAL(); ui_end_frame(); focus("open");
  CHECK(ui_context_destroy(c));
  #undef MODAL
}
static void scrolling(void) {
  UIContext *c=context();
  #define SCROLL() do { UIScrollScope scroll=ui_scroll_begin("scroll",(UIRect){10,10,300,80},1); \
    for(int n=0;n<8;++n) { char id[16]; snprintf(id,sizeof(id),"row%d",n); button(id,scroll.layout.cursor_y,false); scroll.layout.cursor_y+=40; } \
    ui_scroll_end(&scroll); } while(0)
  begin(NULL,0); SCROLL(); ui_end_frame();
  InputKeyEvent e=key(PLATFORM_KEY_TAB,PLATFORM_MOD_SHIFT);
  begin(&e,1); SCROLL(); ui_end_frame(); focus("row7");
  UIScrollDiagnostics d; CHECK(ui_scroll_get_diagnostics("scroll",&d)); CHECK(d.offset>0);
  UIContext *other=context(); begin(NULL,0); button("other",20,false); ui_end_frame(); focus("");
  CHECK(ui_context_destroy(other)&&ui_context_set_current(c)); focus("row7");
  CHECK(ui_context_destroy(c));
  #undef SCROLL
}
static void nested_modals(void) {
  UIContext *c=context(); bool outer=false,inner=false; float x,y,w,h;
  #define NESTED() do { if(button("launch",20,false)) outer=true; \
    if(outer) { if(ui_modal_begin("outer","Outer",font,500,350,&outer,&x,&y,&w,&h)) { \
      if(button("inner-launch",y,false)) inner=true; \
      if(inner) { if(ui_modal_begin("inner","Inner",font,300,200,&inner,&x,&y,&w,&h)) button("inner-action",y,false); \
        ui_modal_end("inner",inner); } } ui_modal_end("outer",outer); } } while(0)
  begin(NULL,0); NESTED(); ui_end_frame();
  InputKeyEvent e=key(PLATFORM_KEY_TAB,0); begin(&e,1); NESTED(); ui_end_frame();
  e=key(PLATFORM_KEY_ENTER,0); begin(&e,1); NESTED(); ui_end_frame(); focus("outer_modal_close");
  e=key(PLATFORM_KEY_TAB,0); begin(&e,1); NESTED(); ui_end_frame(); focus("inner-launch");
  e=key(PLATFORM_KEY_ENTER,0); begin(&e,1); NESTED(); ui_end_frame(); focus("inner_modal_close");
  e=key(PLATFORM_KEY_TAB,0); begin(&e,1); NESTED(); ui_end_frame(); focus("inner-action");
  begin(&e,1); NESTED(); ui_end_frame(); focus("inner_modal_close");
  e=key(PLATFORM_KEY_ESCAPE,0); begin(&e,1); NESTED(); ui_end_frame();
  CHECK(outer&&!inner); focus("inner-launch");
  begin(&e,1); NESTED(); ui_end_frame(); CHECK(!outer); focus("launch");
  CHECK(ui_context_destroy(c));
  #undef NESTED
}
int main(void) {
  traversal(); choices(); modals(); nested_modals(); scrolling();
  CHECK(clips==0&&quads>0);
  puts("UI navigation: PASS (ordered UTF-8/Tab, reverse traversal, disabled/removed widgets, activation, modal restore, choices, scroll, contexts)");
  return 0;
}
