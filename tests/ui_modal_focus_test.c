#define main navigation_suite_main
#include "ui_navigation_test.c"
#undef main
#include "../src/ui/ui_state_text_fields_internal.h"
#include "../src/ui/ui_state_numeric_internal.h"

static UITextEditorResult source(char *text) {
  UITextEditorOptions options={.scale=14,.line_numbers=true};
  return ui_text_editor("source",font,(UIRect){10,20,300,100},text,512,&options);
}
static void dialog(const char *id,bool *open,const char *field_id,char *text) {
  float x,y,w,h;
  if(*open && ui_modal_begin(id,id,font,400,260,open,&x,&y,&w,&h))
    field(field_id,y+40,text);
  ui_modal_end(id,*open);
}
static void automatic(void) {
  UIContext *c=context(); char text[512]="alpha\nèbeta\nz",draft[64]="";
  bool open=false;
  begin(NULL,0); source(text); ui_end_frame(); ui_text_input_focus("source");
  InputKeyEvent select[]={key(PLATFORM_KEY_RIGHT,0),key(PLATFORM_KEY_RIGHT,PLATFORM_MOD_SHIFT)};
  begin(select,2); UITextEditorResult before=source(text); ui_end_frame();
  CHECK(before.caret==2&&before.selection_anchor==1);
  open=true; begin(NULL,0); source(text); dialog("modal",&open,"draft",draft); ui_end_frame();
  CHECK(!ui_text_input_is_focused("source"));
  UIContext *other=context(); char independent[512]="other";
  begin(NULL,0); source(independent); ui_end_frame();
  CHECK(!ui_has_blocking_overlay()&&!ui_text_input_is_focused("source"));
  CHECK(ui_context_destroy(other)&&ui_context_set_current(c)&&ui_has_blocking_overlay());
  /* The background may be submitted for many frames without losing selection. */
  ui_text_input_focus("draft"); InputKeyEvent e=character('M');
  begin(&e,1); source(text); dialog("modal",&open,"draft",draft); ui_end_frame();
  CHECK(!strcmp(draft,"M")&&!strcmp(text,"alpha\nèbeta\nz"));
  e=key(PLATFORM_KEY_ESCAPE,0);
  begin(&e,1); source(text); dialog("modal",&open,"draft",draft); ui_end_frame();
  CHECK(!open&&ui_text_input_is_focused("source")&&ui_has_keyboard_capture());
  begin(NULL,0); UITextEditorResult restored=source(text); ui_end_frame();
  CHECK(restored.focused&&restored.caret==before.caret&&restored.selection_anchor==before.selection_anchor);
  e=character('Q'); begin(&e,1); source(text); ui_end_frame(); CHECK(!strcmp(text,"aQpha\nèbeta\nz"));
  e=key(PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL); begin(&e,1); source(text); ui_end_frame(); CHECK(!strcmp(text,"alpha\nèbeta\nz"));
  CHECK(ui_context_destroy(c));
}
static void single_line_and_nested(void) {
  UIContext *c=context(); char text[512],a[64]="outer",b[64]="inner";
  memset(text,'a',399); text[399]=0;
  begin(NULL,0); ui_text_input("long",font,ANCHOR_TOP_LEFT,0,14,10,20,300,text,sizeof(text),NULL); ui_end_frame();
  ui_text_input_focus("long"); InputKeyEvent e=character('!');
  begin(&e,1); ui_text_input("long",font,ANCHOR_TOP_LEFT,0,14,10,20,300,text,sizeof(text),NULL); ui_end_frame();
  CHECK(strlen(text)==400); UITextInputState *s=ui_text_field_state("long");
  s->caret=10; s->selection_anchor=4;
  bool outer=true,inner=false; float x,y,w,h;
#define FRAME() do { \
  ui_text_input("long",font,ANCHOR_TOP_LEFT,0,14,10,20,300,text,sizeof(text),NULL); \
  if(outer && ui_modal_begin("outer","Outer",font,450,320,&outer,&x,&y,&w,&h)) { \
    field("outer-text",y+30,a); dialog("inner",&inner,"inner-text",b); \
  } ui_modal_end("outer",outer); \
} while(0)
  begin(NULL,0); FRAME(); ui_end_frame(); CHECK(s->suspended&&!s->focused&&s->original_long);
  ui_text_input_focus("outer-text"); e=character('O'); begin(&e,1); FRAME(); ui_end_frame();
  inner=true; begin(NULL,0); FRAME(); ui_end_frame(); CHECK(!ui_text_input_is_focused("outer-text"));
  e=key(PLATFORM_KEY_ESCAPE,0); begin(&e,1); FRAME(); ui_end_frame();
  CHECK(!inner&&outer&&ui_text_input_is_focused("outer-text")&&!s->focused);
  begin(&e,1); FRAME(); ui_end_frame(); CHECK(!outer&&s->focused&&s->caret==10&&s->selection_anchor==4);
  /* Escape still restores the original long baseline from before the modal. */
  begin(&e,1); FRAME(); ui_end_frame(); CHECK(strlen(text)==399&&!s->focused&&!s->original_long);
  CHECK(ui_context_destroy(c));
#undef FRAME
}
static void override_and_cancel(void) {
  UIContext *c=context(); char text[512]="abcdef",draft[64]=""; bool open=false;
  begin(NULL,0); source(text); button("launch",160,false); ui_end_frame(); ui_text_input_focus("source");
  InputKeyEvent e=key(PLATFORM_KEY_RIGHT,0); begin(&e,1); source(text); button("launch",160,false); ui_end_frame();
  e=key(PLATFORM_KEY_TAB,PLATFORM_MOD_SHIFT); begin(&e,1); source(text); button("launch",160,false); ui_end_frame(); focus("launch");
  open=true; begin(NULL,0); source(text); button("launch",160,false); dialog("modal",&open,"draft",draft);
  CHECK(!ui_modal_set_return_focus("missing","source",true));
  CHECK(!ui_modal_set_return_focus("modal","draft",true)); /* Child is not a parent target. */
  CHECK(ui_modal_set_return_focus("modal","source",true)); ui_end_frame();
  e=key(PLATFORM_KEY_ESCAPE,0); begin(&e,1); source(text); button("launch",160,false); dialog("modal",&open,"draft",draft); ui_end_frame();
  CHECK(ui_text_input_is_focused("source"));
  e=character('X'); begin(&e,1); source(text); button("launch",160,false); ui_end_frame(); CHECK(!strcmp(text,"aXbcdef"));
  /* Explicit capture release cancels pending resumption, including after regain. */
  open=true; begin(NULL,0); source(text); dialog("modal",&open,"draft",draft); ui_end_frame();
  ui_release_input_capture(); open=false;
  begin(NULL,0); source(text); dialog("modal",&open,"draft",draft); ui_end_frame();
  CHECK(!ui_text_input_is_focused("source"));
  ui_text_input_focus("source"); begin(NULL,0); source(text); ui_end_frame();
  open=true; begin(NULL,0); source(text); dialog("modal",&open,"draft",draft); ui_end_frame();
  UIFrameInput lost={.width=640,.height=480,.focus_lost=true};
  CHECK(ui_begin_frame_input(&lost)); source(text); dialog("modal",&open,"draft",draft); ui_end_frame();
  open=false; begin(NULL,0); source(text); dialog("modal",&open,"draft",draft); ui_end_frame();
  CHECK(!ui_text_input_is_focused("source")&&!ui_has_keyboard_capture());
  e=character('G'); begin(&e,1); source(text); ui_end_frame(); CHECK(!strcmp(text,"aXbcdef"));
  CHECK(ui_context_destroy(c));
}
static void numeric_draft(void) {
  UIContext *c=context(); float number=12; char draft[64]=""; bool open=false;
#define NUMBER() ui_float_field("number",font,20,20,120,30,14,&number,-1000,1000)
  begin(NULL,0); NUMBER(); ui_end_frame();
  InputKeyEvent e=key(PLATFORM_KEY_TAB,0); begin(&e,1); NUMBER(); ui_end_frame();
  InputKeyEvent type[]={key(PLATFORM_KEY_A,PLATFORM_MOD_CONTROL),character('4'),character('2')};
  begin(type,3); NUMBER(); ui_end_frame(); CHECK(number==12);
  open=true; begin(NULL,0); NUMBER(); dialog("modal",&open,"draft",draft); ui_end_frame();
  begin(NULL,0); NUMBER(); dialog("modal",&open,"draft",draft); ui_end_frame();
  e=key(PLATFORM_KEY_ESCAPE,0); begin(&e,1); NUMBER(); dialog("modal",&open,"draft",draft); ui_end_frame();
  CHECK(!open&&ui_numeric_state("number")->focused);
  e=key(PLATFORM_KEY_ENTER,0); begin(&e,1); NUMBER(); ui_end_frame(); CHECK(number==42);
  CHECK(ui_context_destroy(c));
#undef NUMBER
}
int main(void) {
  automatic(); single_line_and_nested(); override_and_cancel(); numeric_draft();
  puts("Modal return focus: PASS (editing/caret/selection/history/baseline, nested modals, explicit target, cancel/focus loss, numeric draft)");
  return 0;
}
