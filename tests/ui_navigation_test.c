/* Real core with a deterministic renderer; CHECK remains active in release. */
#include "ui/ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"navigation:%d: %s\n",__LINE__,#x); exit(1); } } while(0)
static int clips, quads, ring_quads;
static bool reject_clip, expect_numeric_clip;
static int label_count;
static char label_trace[32][96];
static void quad(void *u,float x,float y,float w,float h,float r,float g,float b) { ++quads;
  UIColor accent=ui_get_theme().accent;
  if((w==2 || h==2) && r==accent.r && g==accent.g && b==accent.b) ++ring_quads;
}
static void alpha(void *u,float x,float y,float w,float h,float r,float g,float b,float a) { ++quads; }
static void text(void *u,Font *f,const char *s,float x,float y,float z,float r,float g,float b) {
  if(expect_numeric_clip) CHECK(clips>0);
  if(label_count<32) snprintf(label_trace[label_count++],96,"%s",s);
}
static void image(void *u,float x,float y,float w,float h,int slot,float a,float b,float c,float d) {}
static float measure(void *u,Font *f,const char *s,float z) { return strlen(s)*z*.5f; }
static float ascent(void *u,Font *f,float z) { return z; }
static bool push(void *u,float x,float y,float w,float h) { if(reject_clip) return false; ++clips; return true; }
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
  e=key(PLATFORM_KEY_ESCAPE,0); begin(&e,1); MODAL(); ui_end_frame(); CHECK(!open&&!strcmp(buffer,"original!"));
  UIInputDiagnostics diagnostics; CHECK(ui_get_input_diagnostics(&diagnostics));
  CHECK(diagnostics.escape_consumed); focus("open");
  e=key(PLATFORM_KEY_ENTER,0); begin(&e,1); MODAL(); ui_end_frame(); CHECK(open);
  e=key(PLATFORM_KEY_TAB,0); begin(&e,1); MODAL(); ui_end_frame(); focus("edit");
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
static void label_policy(void) {
  UIContext *a=context(); char output[96];
  const char *labels[]={"3D","FX","Nuovo layer","NUOVO LAYER","Caffè €"};
  begin(NULL,0); label_count=0;
  for(int i=0;i<5;++i) {
    ui_humanize_label(labels[i],output,sizeof(output)); CHECK(!strcmp(output,labels[i]));
    char id[32]; snprintf(id,sizeof(id),"label%d",i);
    ui_button_labeled(id,labels[i],font,ANCHOR_TOP_LEFT,0,14,20,20+i*40,NULL);
  }
  CHECK(label_count==5);
  for(int i=0;i<5;++i) CHECK(!strcmp(label_trace[i],labels[i]));
  ui_end_frame();
  ui_set_sentence_case(true);
  ui_humanize_label("NUOVO LAYER",output,sizeof(output)); CHECK(!strcmp(output,"Nuovo layer"));
  ui_humanize_label("Nuovo layer",output,sizeof(output)); CHECK(!strcmp(output,"Nuovo layer"));
  ui_humanize_label("FX (px)",output,sizeof(output)); CHECK(!strcmp(output,"Fx (px)"));
  ui_humanize_label("3D",output,sizeof(output)); CHECK(!strcmp(output,"3D"));
  char small[3]; ui_humanize_label("A€",small,sizeof(small)); CHECK(!strcmp(small,"A"));
  static const char *const acronyms[]={"FX","VFX","GLB","FPS","HUD","UI","NPC","3D"};
  CHECK(ui_set_sentence_case_exceptions(acronyms,sizeof(acronyms)/sizeof(acronyms[0])));
  const char *inputs[]={"FX", "FX ATTIVO", "MOSTRA FX/VFX, GLB: FPS HUD UI NPC",
                        "EFFETTO AFX FX2 FX_TEST FX", "VISTA 3D", "FX (px)",
                        "FX già scritto", "ÉFX FXÉ FX"};
  const char *expected[]={"FX", "FX attivo", "Mostra FX/VFX, GLB: FPS HUD UI NPC",
                          "Effetto afx fx2 fx_test FX", "Vista 3D", "FX (px)",
                          "FX già scritto", "ÉFx fxÉ FX"};
  begin(NULL,0); label_count=0;
  for(size_t i=0;i<sizeof(inputs)/sizeof(inputs[0]);++i) {
    ui_humanize_label(inputs[i],output,sizeof(output)); CHECK(!strcmp(output,expected[i]));
    char id[32]; snprintf(id,sizeof(id),"acronym%zu",i);
    ui_button_labeled(id,inputs[i],font,ANCHOR_TOP_LEFT,0,14,20,20+i*40,NULL);
  }
  CHECK(label_count==8);
  for(int i=0;i<8;++i) CHECK(!strcmp(label_trace[i],expected[i]));
  ui_end_frame();
  char clipped[9]; ui_humanize_label("MOSTRA FX2",clipped,sizeof(clipped)); CHECK(!strcmp(clipped,"Mostra f"));
  ui_humanize_label("MOSTRA FX",clipped,sizeof(clipped)); CHECK(!strcmp(clipped,"Mostra F"));
  strcpy(output,"MOSTRA FX"); ui_humanize_label(output,output,sizeof(output)); CHECK(!strcmp(output,"Mostra FX"));
  const char *bad_empty[]={""}, *bad_null[]={NULL}, *bad_phrase[]={"FX VFX"};
  CHECK(!ui_set_sentence_case_exceptions(NULL,1));
  CHECK(!ui_set_sentence_case_exceptions(bad_empty,1));
  CHECK(!ui_set_sentence_case_exceptions(bad_null,1));
  CHECK(!ui_set_sentence_case_exceptions(bad_phrase,1));
  ui_humanize_label("FX",output,sizeof(output)); CHECK(!strcmp(output,"FX"));
  CHECK(ui_set_sentence_case_exceptions(NULL,0));
  ui_humanize_label("FX",output,sizeof(output)); CHECK(!strcmp(output,"Fx"));
  static const char *const lowercase[]={"fx"};
  CHECK(ui_set_sentence_case_exceptions(lowercase,1));
  ui_humanize_label("FX",output,sizeof(output)); CHECK(!strcmp(output,"Fx"));
  CHECK(ui_set_sentence_case_exceptions(acronyms,8));
  UIContext *b=context();
  ui_humanize_label("FX",output,sizeof(output)); CHECK(!strcmp(output,"FX"));
  ui_set_sentence_case(true);
  ui_humanize_label("FX",output,sizeof(output)); CHECK(!strcmp(output,"Fx"));
  CHECK(ui_context_set_current(a));
  ui_humanize_label("FX",output,sizeof(output)); CHECK(!strcmp(output,"FX"));
  CHECK(ui_context_reset(a));
  ui_humanize_label("FX",output,sizeof(output)); CHECK(!strcmp(output,"FX"));
  ui_set_sentence_case(true);
  ui_humanize_label("FX",output,sizeof(output)); CHECK(!strcmp(output,"Fx"));
  CHECK(ui_context_destroy(a)&&ui_context_set_current(b)&&ui_context_destroy(b));
}
static void modality_frame(bool *open,const UIFrameInput *input) {
  ring_quads=0; CHECK(ui_begin_frame_input(input));
  if(button("launch",20,false)) *open=true;
  if(*open) {
    float x,y,w,h;
    if(ui_modal_begin("mode","Rename",font,400,240,open,&x,&y,&w,&h))
      button("action",y,false);
    ui_modal_end("mode",*open);
  }
  ui_end_frame();
}
static void input_modality(void) {
  UIContext *c=context(); bool open=false;
  UIFrameInput f={.width=640,.height=480,.mouse_x=30,.mouse_y=30,.delta_time=.016f};
  modality_frame(&open,&f); CHECK(!open&&!ring_quads);
  f.mouse_down=true; modality_frame(&open,&f); CHECK(!open&&!ring_quads);
  f.mouse_down=false; modality_frame(&open,&f); CHECK(open&&!ring_quads); focus("mode_modal_close");
  InputKeyEvent e=key(PLATFORM_KEY_TAB,0); f.events=&e; f.event_count=1;
  modality_frame(&open,&f); CHECK(open&&ring_quads==4); focus("action");
  f.events=NULL; f.event_count=0; f.mouse_x=31;
  modality_frame(&open,&f); CHECK(open&&!ring_quads); focus("action");
  e=key(PLATFORM_KEY_TAB,0); f.events=&e; f.event_count=1;
  modality_frame(&open,&f); CHECK(ring_quads==4); focus("mode_modal_close");
  e=key(PLATFORM_KEY_ESCAPE,0); modality_frame(&open,&f); CHECK(!open); focus("launch");
  f.events=NULL; f.event_count=0;
  modality_frame(&open,&f); CHECK(ring_quads==4);
  /* A pointer-opened modal must not show rings on programmatic text focus. */
  f.mouse_x=32; modality_frame(&open,&f); CHECK(!ring_quads);
  ring_quads=0; CHECK(ui_begin_frame_input(&f)); ui_text_input_focus("auto");
  char buffer[64]=""; field("auto",100,buffer); ui_end_frame();
  CHECK(ui_text_input_is_focused("auto")&&!ring_quads);
  open=true; modality_frame(&open,&f); CHECK(open&&!ring_quads);
  UITheme theme=ui_get_theme();
  f.mouse_x=120+400-theme.compact_button_height*.5f-theme.row_spacing;
  f.mouse_y=120+(theme.panel_header_height+theme.row_spacing)*.5f;
  f.mouse_down=true; modality_frame(&open,&f); CHECK(open&&!ring_quads);
  f.mouse_down=false; modality_frame(&open,&f); CHECK(!open&&!ring_quads); focus("launch");
  modality_frame(&open,&f); CHECK(!ring_quads);
  CHECK(ui_context_destroy(c));
}
static void editor_modal_frame(int kind,bool *open,char *buffer,float *number,bool *revealed,const InputKeyEvent *event) {
  begin(event,event?1:0);
  CHECK(!button("background",20,false));
  float x,y,w,h;
  if(ui_modal_begin("editor-dialog","Edit",font,400,240,open,&x,&y,&w,&h)) {
    if(kind==0) ui_text_input("modal-edit",font,ANCHOR_TOP_LEFT,0,14,x,y,200,buffer,64,NULL);
    else if(kind==1) ui_float_field("modal-number",font,x,y,200,30,14,number,0,100);
    else {
      UIPropertyLayout layout=ui_property_layout_begin(x,y,w,1);
      ui_property_password(&layout,"secret",font,"Password",buffer,64,revealed);
    }
  }
  ui_modal_end("editor-dialog",*open); ui_end_frame();
}
static void editors_cancel_dialog(void) {
  for(int kind=0;kind<3;++kind) {
    UIContext *c=context(); bool open=true,revealed=false; char buffer[64]="draft"; float number=1;
    editor_modal_frame(kind,&open,buffer,&number,&revealed,NULL);
    InputKeyEvent event=key(PLATFORM_KEY_TAB,0);
    editor_modal_frame(kind,&open,buffer,&number,&revealed,&event);
    focus(kind==0?"modal-edit":kind==1?"modal-number":"secret/text");
    event=character('9'); editor_modal_frame(kind,&open,buffer,&number,&revealed,&event);
    if(kind!=1) CHECK(!strcmp(buffer,"draft9")); else CHECK(number==1);
    event=key(PLATFORM_KEY_ESCAPE,0);
    editor_modal_frame(kind,&open,buffer,&number,&revealed,&event);
    UIInputDiagnostics d; CHECK(ui_get_input_diagnostics(&d));
    CHECK(!open&&d.escape_consumed&&!event.consumed&&!ui_any_text_input_focused());
    CHECK(number==1); /* Uncommitted numeric text is never applied. */
    CHECK(ui_context_destroy(c));
  }
}
static void capture_form(int kind, float *number, char *buffer) {
  if(kind==0) button("capture-button",20,false);
  else if(kind==1) field("capture-text",20,buffer);
  else ui_float_field("capture-number",font,20,20,100,30,14,number,-100,100);
}
static void capture_frame(int kind,float *number,char *buffer, bool down,
                          const InputKeyEvent *events,size_t count) {
  UIFrameInput f={.width=640,.height=480,.mouse_x=30,.mouse_y=30,
                 .mouse_down=down,.delta_time=.016f,.events=events,.event_count=count};
  CHECK(ui_begin_frame_input(&f)); capture_form(kind,number,buffer); ui_end_frame();
}
static void capture_release(void) {
  for(int kind=0;kind<3;++kind) for(int escape=0;escape<2;++escape) {
    UIContext *c=context(); float number=2.2f; char buffer[64]="text";
    capture_frame(kind,&number,buffer,false,NULL,0);
    capture_frame(kind,&number,buffer,true,NULL,0);
    capture_frame(kind,&number,buffer,false,NULL,0);
    CHECK(ui_has_keyboard_capture()==(kind!=0));
    CHECK(!ui_has_pointer_capture());
    if(kind) {
      InputKeyEvent end=key(escape?PLATFORM_KEY_ESCAPE:PLATFORM_KEY_ENTER,0);
      capture_frame(kind,&number,buffer,false,&end,1);
      CHECK(!ui_has_input_capture());
      focus(kind==1?"capture-text":"capture-number");
    }
    InputKeyEvent shortcut[]={key(PLATFORM_KEY_G,0),character('g')};
    capture_frame(kind,&number,buffer,false,shortcut,2);
    UIInputDiagnostics d; CHECK(ui_get_input_diagnostics(&d));
    CHECK(!d.keyboard_capture&&!d.focused_widget_id&&!ui_has_input_capture());
    CHECK(number==2.2f&&!strcmp(buffer,"text"));
    capture_frame(kind,&number,buffer,false,NULL,0); CHECK(!ui_has_keyboard_capture());
    if(kind) {
      /* Enter explicitly resumes editing, while Tab can resume traversal. */
      InputKeyEvent enter=key(PLATFORM_KEY_ENTER,0);
      capture_frame(kind,&number,buffer,false,&enter,1); CHECK(ui_has_keyboard_capture());
      capture_frame(kind,&number,buffer,false,&enter,1); CHECK(!ui_has_keyboard_capture());
      InputKeyEvent tab=key(PLATFORM_KEY_TAB,0);
      capture_frame(kind,&number,buffer,false,&tab,1); CHECK(ui_has_keyboard_capture());
      capture_frame(kind,&number,buffer,false,&enter,1); CHECK(!ui_has_keyboard_capture());
      capture_frame(kind,&number,buffer,false,shortcut,2); CHECK(!ui_has_keyboard_capture());
      if(kind==2) {
        capture_frame(kind,&number,buffer,false,&enter,1); CHECK(ui_has_keyboard_capture());
        InputKeyEvent invalid[]={key(PLATFORM_KEY_A,PLATFORM_MOD_CONTROL),key(PLATFORM_KEY_ENTER,0)};
        capture_frame(kind,&number,buffer,false,invalid,2); CHECK(ui_has_keyboard_capture());
        InputKeyEvent exponent[]={character('1'),character('e'),character('+'),character('1'),key(PLATFORM_KEY_ENTER,0)};
        capture_frame(kind,&number,buffer,false,exponent,5);
        CHECK(number==10&&!ui_has_keyboard_capture());
      }
    }
    CHECK(ui_context_destroy(c));
  }
}
static void numeric_presentation(void) {
  UIContext *c=context();
  float values[]={2.2f,-2.2f,100,1.2345678f,1e20f,1e-20f,-0.0f};
  for(size_t i=0;i<sizeof(values)/sizeof(values[0]);++i) {
    begin(NULL,0); label_count=0; expect_numeric_clip=true;
    float original=values[i];
    ui_float_field("presentation",font,20,20,30,30,14,&values[i],-1e30f,1e30f);
    expect_numeric_clip=false;
    CHECK(label_count==1&&clips==0);
    CHECK(strtof(label_trace[0],NULL)==original&&values[i]==original);
    if(i==0) CHECK(!strcmp(label_trace[0],"2.2"));
    if(i==2) CHECK(!strcmp(label_trace[0],"100"));
    if(i==6) CHECK(signbit(strtof(label_trace[0],NULL)));
    ui_end_frame();
  }
  begin(NULL,0); label_count=0; reject_clip=true;
  ui_float_field("presentation",font,20,20,30,30,14,&values[0],-100,100);
  reject_clip=false; CHECK(!label_count&&clips==0); ui_end_frame();
  CHECK(ui_context_destroy(c));
}
int main(void) {
  capture_release(); numeric_presentation(); traversal(); choices(); modals(); nested_modals(); scrolling(); label_policy(); input_modality(); editors_cancel_dialog();
  CHECK(clips==0&&quads>0);
  puts("UI navigation: PASS (ordered UTF-8/Tab, reverse traversal, disabled/removed widgets, activation, modal one-Esc/editors, keyboard-only rings, label policy, choices, scroll, contexts)");
  return 0;
}
