#define main navigation_suite_main
#include "ui_navigation_test.c"
#undef main
#include "../src/ui/ui_state_text_fields_internal.h"

static UITextEditorResult draw(char *buffer,int capacity,const UITextEditorOptions *options) {
  UITextEditorOptions opt=options?*options:(UITextEditorOptions){0};
  if(opt.scale==0) opt.scale=14;
  return ui_text_editor("source",font,(UIRect){20,20,260,100},buffer,capacity,&opt);
}
static UITextEditorResult event(char *buffer,int capacity,InputKeyEvent e) {
  begin(&e,1); UITextEditorResult r=draw(buffer,capacity,NULL); ui_end_frame(); return r;
}
static char copied[256];
static const char *paste_text;
static int released;
static bool clip_read(void *user,const char **text,size_t *bytes) {
  *text=paste_text; *bytes=strlen(paste_text); return true;
}
static void clip_release(void *user,const char *text) { CHECK(text==paste_text); ++released; }
static bool clip_write(void *user,const char *text) { snprintf(copied,sizeof(copied),"%s",text); return true; }
static void editing(void) {
  UIContext *c=context(); char text[256]="";
  UIClipboard clipboard={clip_read,clip_release,clip_write};
  CHECK(ui_context_set_clipboard(c,&clipboard,NULL));
  begin(NULL,0); UITextEditorResult r=draw(text,sizeof(text),NULL); ui_end_frame();
  CHECK(r.line_count==1&&!r.changed&&!r.focused);
  ui_text_input_focus("source");
  r=event(text,sizeof(text),character(0xe8)); CHECK(r.changed&&r.focused&&r.line==1&&r.column==2);
  r=event(text,sizeof(text),key(PLATFORM_KEY_ENTER,0)); CHECK(r.changed&&r.focused&&r.line_count==2&&r.column==1);
  r=event(text,sizeof(text),key(PLATFORM_KEY_TAB,0)); CHECK(r.changed&&r.focused&&!strcmp(text,"è\n\t"));
  r=event(text,sizeof(text),character('x')); CHECK(!strcmp(text,"è\n\tx")&&r.column==3);
  r=event(text,sizeof(text),key(PLATFORM_KEY_S,PLATFORM_MOD_CONTROL)); CHECK(r.save_requested&&!r.changed&&r.focused);
  r=event(text,sizeof(text),key(PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL)); CHECK(r.changed&&!strcmp(text,"è\n\t"));
  r=event(text,sizeof(text),key(PLATFORM_KEY_Y,PLATFORM_MOD_CONTROL)); CHECK(r.changed&&!strcmp(text,"è\n\tx"));
  r=event(text,sizeof(text),key(PLATFORM_KEY_HOME,0)); CHECK(r.caret==3&&r.column==1);
  r=event(text,sizeof(text),key(PLATFORM_KEY_UP,PLATFORM_MOD_SHIFT)); CHECK(r.caret==0&&r.selection_anchor==3);
  r=event(text,sizeof(text),key(PLATFORM_KEY_C,PLATFORM_MOD_CONTROL)); CHECK(!strcmp(copied,"è\n"));
  r=event(text,sizeof(text),key(PLATFORM_KEY_ESCAPE,0)); CHECK(!r.focused&&!r.changed&&!strcmp(text,"è\n\tx"));
  CHECK(!ui_has_keyboard_capture()&&!ui_any_text_input_focused());
  ui_text_input_focus("source");
  event(text,sizeof(text),key(PLATFORM_KEY_A,PLATFORM_MOD_CONTROL));
  paste_text="a\r\nb\rc\n🙂\tè";
  r=event(text,sizeof(text),key(PLATFORM_KEY_V,PLATFORM_MOD_CONTROL));
  CHECK(r.changed&&r.line_count==4&&!strcmp(text,"a\nb\nc\n🙂\tè")&&released==1);
  r=event(text,sizeof(text),key(PLATFORM_KEY_HOME,PLATFORM_MOD_CONTROL)); CHECK(r.caret==0);
  r=event(text,sizeof(text),key(PLATFORM_KEY_END,PLATFORM_MOD_CONTROL)); CHECK(r.caret==(int)strlen(text));
  r=event(text,sizeof(text),key(PLATFORM_KEY_BACKSPACE,0)); CHECK(r.changed&&!strcmp(text,"a\nb\nc\n🙂\t"));
  CHECK(ui_context_destroy(c));
}
static void movement_and_limits(void) {
  UIContext *c=context(); char text[128]="abcd\nx\nè🙂zz\n";
  begin(NULL,0); draw(text,sizeof(text),NULL); ui_end_frame(); ui_text_input_focus("source");
  event(text,sizeof(text),key(PLATFORM_KEY_HOME,PLATFORM_MOD_CONTROL));
  for(int i=0;i<3;++i) event(text,sizeof(text),key(PLATFORM_KEY_RIGHT,0));
  UITextEditorResult r=event(text,sizeof(text),key(PLATFORM_KEY_DOWN,0)); CHECK(r.line==2&&r.column==2);
  r=event(text,sizeof(text),key(PLATFORM_KEY_DOWN,0)); CHECK(r.line==3&&r.column==4&&r.caret==14);
  r=event(text,sizeof(text),key(PLATFORM_KEY_END,0)); CHECK(r.line==3&&r.column==5);
  r=event(text,sizeof(text),key(PLATFORM_KEY_DOWN,0)); CHECK(r.line==4&&r.column==1);
  r=event(text,sizeof(text),key(PLATFORM_KEY_UP,0)); CHECK(r.line==3&&r.column==5);
  /* Host replacements invalidate old undo history. */
  strcpy(text,"new"); r=event(text,sizeof(text),key(PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL)); CHECK(!strcmp(text,"new")&&!r.changed);
  /* Reveal is a host command, does not steal focus or mark the document dirty. */
  strcpy(text,"uno\ndue\ntre"); UITextEditorOptions options={.line_numbers=true,.reveal_line=3,.highlight_line=3};
  begin(NULL,0); r=draw(text,sizeof(text),&options); ui_end_frame(); CHECK(r.line==3&&r.column==1&&!r.changed);
  char small[4]="è"; CHECK(ui_context_reset(c));
  begin(NULL,0); draw(small,sizeof(small),NULL); ui_end_frame(); ui_text_input_focus("source");
  event(small,sizeof(small),key(PLATFORM_KEY_END,PLATFORM_MOD_CONTROL));
  r=event(small,sizeof(small),character(0x1f642)); CHECK(!r.changed&&!strcmp(small,"è"));
  r=event(small,sizeof(small),key(PLATFORM_KEY_ENTER,0)); CHECK(r.changed&&!strcmp(small,"è\n"));
  begin(NULL,0); ui_context_clear_error(c);
  char bad[]={'x','x'}; r=draw(bad,sizeof(bad),NULL); CHECK(!r.changed&&ui_context_error(c).code==UI_ERROR_ARGUMENT);
  ui_context_clear_error(c); char cr[]="a\rb"; draw(cr,sizeof(cr),NULL); CHECK(ui_context_error(c).code==UI_ERROR_ARGUMENT);
  ui_context_clear_error(c); draw(small,UI_TEXT_EDITOR_MAX_CAPACITY+1,NULL); CHECK(ui_context_error(c).code==UI_ERROR_ARGUMENT);
  ui_end_frame(); CHECK(ui_context_destroy(c));
}
static void readonly_and_navigation(void) {
  UIContext *c=context(); char text[64]="read\nonly"; UITextEditorOptions options={.read_only=true};
  begin(NULL,0); draw(text,sizeof(text),&options); button("next",140,false); ui_end_frame();
  ui_text_input_focus("source"); InputKeyEvent events[]={key(PLATFORM_KEY_A,PLATFORM_MOD_CONTROL),character('!'),
    key(PLATFORM_KEY_BACKSPACE,0),key(PLATFORM_KEY_ENTER,0),key(PLATFORM_KEY_TAB,0),key(PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL)};
  begin(events,6); UITextEditorResult r=draw(text,sizeof(text),&options); button("next",140,false); ui_end_frame();
  CHECK(!r.changed&&!strcmp(text,"read\nonly")&&r.focused);
  InputKeyEvent e=key(PLATFORM_KEY_TAB,PLATFORM_MOD_SHIFT);
  begin(&e,1); draw(text,sizeof(text),&options); button("next",140,false); ui_end_frame(); focus("next");
  CHECK(!ui_any_text_input_focused());
  /* Ordered Tab -> character -> Tab -> character routes indentation to editor. */
  e=key(PLATFORM_KEY_TAB,0); begin(&e,1); draw(text,sizeof(text),NULL); button("next",140,false); ui_end_frame();
  focus("source");
  InputKeyEvent burst[]={character('A'),key(PLATFORM_KEY_TAB,0),character('B')};
  begin(burst,3); r=draw(text,sizeof(text),NULL); button("next",140,false); ui_end_frame();
  CHECK(r.changed&&strstr(text,"A\tB"));
  UIContext *other=context(); char second[64]="other";
  begin(NULL,0); r=draw(second,sizeof(second),NULL); ui_end_frame(); CHECK(!r.focused&&!r.changed);
  CHECK(ui_context_destroy(other)&&ui_context_set_current(c)); CHECK(ui_text_input_is_focused("source"));
  UIFrameInput lost={.width=640,.height=480,.focus_lost=true};
  CHECK(ui_begin_frame_input(&lost)); r=draw(text,sizeof(text),NULL); ui_end_frame(); CHECK(!r.focused&&!ui_has_keyboard_capture());
  CHECK(ui_context_destroy(c));
}
static UITextEditorResult pointer(char *text,float x,float y,bool down,float wheel) {
  UIFrameInput f={.width=640,.height=480,.mouse_x=x,.mouse_y=y,.mouse_down=down,.scroll_delta=wheel};
  CHECK(ui_begin_frame_input(&f)); UITextEditorResult r=draw(text,1024,NULL); ui_end_frame(); return r;
}
static void overlays_and_lifetime(void) {
  UIContext *c=context(); char source[128]="original",modal_text[64]="";
  bool open=false; float x,y,w,h;
  begin(NULL,0); draw(source,sizeof(source),NULL); ui_end_frame();
  ui_text_input_focus("source"); event(source,sizeof(source),character('!'));
  CHECK(!strcmp(source,"!original"));
  open=true;
  begin(NULL,0); draw(source,sizeof(source),NULL);
  if(ui_modal_begin("overlay","Overlay",font,400,250,&open,&x,&y,&w,&h)) field("modal-text",y,modal_text);
  ui_modal_end("overlay",open); ui_end_frame();
  ui_text_input_focus("modal-text"); InputKeyEvent e=character('M');
  begin(&e,1); draw(source,sizeof(source),NULL);
  if(ui_modal_begin("overlay","Overlay",font,400,250,&open,&x,&y,&w,&h)) field("modal-text",y,modal_text);
  ui_modal_end("overlay",open); ui_end_frame();
  CHECK(!strcmp(source,"!original")&&!strcmp(modal_text,"M"));
  open=false; begin(NULL,0); draw(source,sizeof(source),NULL); ui_modal_end("overlay",open); ui_end_frame();
  ui_release_input_capture(); CHECK(!ui_any_text_input_focused());
  ui_text_input_focus("source");
  event(source,sizeof(source),key(PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL)); CHECK(!strcmp(source,"original"));
  /* A widget omitted for a frame releases capture through the shared lifecycle. */
  begin(NULL,0); button("only",150,false); ui_end_frame(); CHECK(!ui_any_text_input_focused());
  CHECK(ui_context_reset(c));
  begin(NULL,0); UITextEditorResult r=draw(source,sizeof(source),NULL); ui_end_frame();
  CHECK(!r.focused&&r.caret==0); CHECK(ui_context_destroy(c));
}
static void mouse_and_clipping(void) {
  UIContext *c=context(); char text[1024]="abcd\nefgh\nijkl\nmnop\nqrst\nuvwx\nyz";
  pointer(text,30,30,false,0);
  UITextEditorResult r=pointer(text,30,30,true,0); CHECK(r.focused&&r.line==1&&r.column==2);
  r=pointer(text,46,50,true,0); CHECK(r.line==2&&r.caret!=r.selection_anchor);
  pointer(text,46,50,false,0);
  r=event(text,sizeof(text),character('!')); CHECK(r.changed&&strchr(text,'!'));
  UITextInputState *state=ui_text_field_state("source");
  pointer(text,60,60,false,-1); CHECK(state->editor_scroll_y>0);
  pointer(text,60,60,false,100); CHECK(state->editor_scroll_y==0);
  reject_clip=true; begin(NULL,0); draw(text,sizeof(text),NULL); ui_end_frame(); reject_clip=false; CHECK(clips==0);
  /* Wide line can be explored with the horizontal scrollbar and caret reveal. */
  memset(text,'w',200); text[200]=0;
  event(text,sizeof(text),key(PLATFORM_KEY_END,PLATFORM_MOD_CONTROL)); CHECK(state->editor_scroll_x>0);
  event(text,sizeof(text),key(PLATFORM_KEY_HOME,PLATFORM_MOD_CONTROL)); CHECK(state->editor_scroll_x==0);
  pointer(text,180,114,true,0); CHECK(state->editor_scroll_x>0);
  pointer(text,180,114,false,0);
  /* Press and text in the same frame edits at the newly clicked position. */
  pointer(text,30,30,false,0); strcpy(text,"abcd");
  InputKeyEvent e=character('X');
  UIFrameInput f={.width=640,.height=480,.mouse_x=30,.mouse_y=30,.mouse_down=true,.events=&e,.event_count=1};
  CHECK(ui_begin_frame_input(&f)); r=draw(text,sizeof(text),NULL); ui_end_frame();
  CHECK(r.changed&&!strcmp(text,"aXbcd"));
  CHECK(ui_context_destroy(c));
}
int main(void) {
  editing(); movement_and_limits(); readonly_and_navigation(); overlays_and_lifetime(); mouse_and_clipping();
  puts("Multiline editor: PASS (UTF-8, LF/tab, clipboard, undo/redo, navigation, focus, mouse selection, scroll, limits)");
  return 0;
}
