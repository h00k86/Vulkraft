#include "../src/ui/ui_state_text_fields_internal.h"
#include "../src/ui/ui_state_clipboard_internal.h"
/* Reuse the deterministic core renderer and event helpers. */
#define main navigation_suite_main
#include "ui_navigation_test.c"
#undef main

typedef struct {
  const char *source; size_t bytes;
  char copied[256]; int reads,releases,writes; bool fail_read,fail_write;
} ClipboardFixture;
static bool fixture_read(void *user,const char **text,size_t *bytes) {
  ClipboardFixture *f=user; ++f->reads;
  if(f->fail_read) return false;
  *text=f->source; *bytes=f->bytes; return true;
}
static void fixture_release(void *user,const char *text) { ++((ClipboardFixture *)user)->releases; }
static bool fixture_write(void *user,const char *text) {
  ClipboardFixture *f=user; ++f->writes;
  if(f->fail_write) return false;
  snprintf(f->copied,sizeof(f->copied),"%s",text); return true;
}
static const UIClipboard fixture_clipboard={fixture_read,fixture_release,fixture_write};
static void edit(char *s,int capacity,const InputKeyEvent *events,size_t count) {
  begin(events,count);
  ui_text_input("edit",font,ANCHOR_TOP_LEFT,0,14,20,20,100,s,capacity,NULL);
  ui_end_frame();
}
static void press(char *s,int capacity,int k,int mods) {
  InputKeyEvent e=key(k,mods); edit(s,capacity,&e,1); CHECK(!e.consumed);
}
static void type(char *s,int capacity,unsigned cp) {
  InputKeyEvent e=character(cp); edit(s,capacity,&e,1);
}
static void edits(void) {
  UIContext *c=context(); char s[128]="aè🙂z";
  ClipboardFixture f={0}; CHECK(ui_context_set_clipboard(c,&fixture_clipboard,&f));
  ui_text_input_focus("edit"); edit(s,sizeof(s),NULL,0);
  UITextInputState *state=ui_text_field_state("edit");
  press(s,sizeof(s),PLATFORM_KEY_LEFT,PLATFORM_MOD_SHIFT);
  press(s,sizeof(s),PLATFORM_KEY_LEFT,PLATFORM_MOD_SHIFT);
  CHECK(state->caret==3 && state->selection_anchor==8);
  press(s,sizeof(s),PLATFORM_KEY_C,PLATFORM_MOD_CONTROL); CHECK(!strcmp(f.copied,"🙂z"));
  type(s,sizeof(s),0x20ac); CHECK(!strcmp(s,"aè€"));
  press(s,sizeof(s),PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL); CHECK(!strcmp(s,"aè🙂z"));
  CHECK(state->caret==3 && state->selection_anchor==8);
  press(s,sizeof(s),PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL|PLATFORM_MOD_SHIFT); CHECK(!strcmp(s,"aè€"));
  press(s,sizeof(s),PLATFORM_KEY_A,PLATFORM_MOD_CONTROL); CHECK(!strcmp(s,"aè€"));
  f.fail_write=true; press(s,sizeof(s),PLATFORM_KEY_X,PLATFORM_MOD_CONTROL); CHECK(!strcmp(s,"aè€"));
  f.fail_write=false; press(s,sizeof(s),PLATFORM_KEY_X,PLATFORM_MOD_CONTROL); CHECK(!*s&&!strcmp(f.copied,"aè€"));
  press(s,sizeof(s),PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL); CHECK(!strcmp(s,"aè€"));
  f.source="中\n\t🙂\x7f"; f.bytes=strlen(f.source);
  press(s,sizeof(s),PLATFORM_KEY_V,PLATFORM_MOD_CONTROL); CHECK(!strcmp(s,"中🙂")&&f.releases==1);
  press(s,sizeof(s),PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL); CHECK(!strcmp(s,"aè€"));
  press(s,sizeof(s),PLATFORM_KEY_Y,PLATFORM_MOD_CONTROL); CHECK(!strcmp(s,"中🙂"));
  press(s,sizeof(s),PLATFORM_KEY_HOME,0); press(s,sizeof(s),PLATFORM_KEY_END,PLATFORM_MOD_SHIFT);
  f.source="\xc0\xaf"; f.bytes=2; press(s,sizeof(s),PLATFORM_KEY_V,PLATFORM_MOD_CONTROL);
  CHECK(!strcmp(s,"中🙂")&&f.releases==2);
  f.source="\n\t"; f.bytes=2; press(s,sizeof(s),PLATFORM_KEY_V,PLATFORM_MOD_CONTROL); CHECK(!strcmp(s,"中🙂"));
  f.fail_read=true; press(s,sizeof(s),PLATFORM_KEY_V,PLATFORM_MOD_CONTROL); CHECK(f.releases==3&&!strcmp(s,"中🙂"));
  CHECK(ui_context_set_clipboard(c,NULL,NULL));
  press(s,sizeof(s),PLATFORM_KEY_X,PLATFORM_MOD_CONTROL); press(s,sizeof(s),PLATFORM_KEY_V,PLATFORM_MOD_CONTROL); CHECK(!strcmp(s,"中🙂"));
  /* External replacement invalidates undo and the old Escape snapshot. */
  strcpy(s,"external"); edit(s,sizeof(s),NULL,0);
  press(s,sizeof(s),PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL); CHECK(!strcmp(s,"external"));
  press(s,sizeof(s),PLATFORM_KEY_END,0);
  press(s,sizeof(s),PLATFORM_KEY_A,PLATFORM_MOD_CONTROL|PLATFORM_MOD_ALT);
  CHECK(state->caret==state->selection_anchor); /* AltGr is not select-all. */
  type(s,sizeof(s),'!'); press(s,sizeof(s),PLATFORM_KEY_ESCAPE,0); CHECK(!strcmp(s,"external"));
  CHECK(ui_context_destroy(c));
}
static void capacity_history(void) {
  UIContext *c=context(); char s[128]="ab";
  ClipboardFixture f={.source="è🙂x",.bytes=7};
  CHECK(ui_context_set_clipboard(c,&fixture_clipboard,&f));
  ui_text_input_focus("edit"); edit(s,sizeof(s),NULL,0);
  press(s,6,PLATFORM_KEY_V,PLATFORM_MOD_CONTROL); CHECK(!strcmp(s,"abè")&&s[4]==0);
  press(s,6,PLATFORM_KEY_A,PLATFORM_MOD_CONTROL); type(s,2,0x1f642); CHECK(!strcmp(s,"abè"));
  /* capacity cannot describe less than the current string: widget rejects it. */
  ui_text_input_focus("edit"); edit(s,sizeof(s),NULL,0);
  press(s,sizeof(s),PLATFORM_KEY_END,0);
  for(int i=0;i<24;++i) type(s,sizeof(s),'a');
  for(int i=0;i<30;++i) press(s,sizeof(s),PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL);
  CHECK(strlen(s)==12); /* 4 original bytes + eight edits older than 16 undos. */
  type(s,sizeof(s),'b'); press(s,sizeof(s),PLATFORM_KEY_Y,PLATFORM_MOD_CONTROL); CHECK(s[strlen(s)-1]=='b');
  UITextInputState *state=ui_text_field_state("edit");
  CHECK(state->history_count<=UI_TEXT_HISTORY_COUNT && state->history_bytes<=UI_TEXT_HISTORY_BYTES);
  /* Reducing the capacity must not truncate an undo revision. */
  strcpy(s,"longer"); edit(s,sizeof(s),NULL,0); press(s,sizeof(s),PLATFORM_KEY_A,PLATFORM_MOD_CONTROL); type(s,sizeof(s),'x');
  press(s,2,PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL); CHECK(!strcmp(s,"x"));
  /* Large documents stay editable without retaining oversized snapshots. */
  char *large=malloc(UI_TEXT_SNAPSHOT_BYTES+10); CHECK(large);
  memset(large,'a',UI_TEXT_SNAPSHOT_BYTES); large[UI_TEXT_SNAPSHOT_BYTES]=0;
  edit(large,UI_TEXT_SNAPSHOT_BYTES+10,NULL,0); type(large,UI_TEXT_SNAPSHOT_BYTES+10,'b');
  CHECK(state->history_count==0); free(large);
  CHECK(ui_context_reset(c)); CHECK(ui_state_clipboard(c)->clipboard.read==fixture_read);
  CHECK(ui_context_destroy(c));
}
static void isolation_and_limits(void) {
  UIContext *a=context(); char left[128]="a";
  ClipboardFixture fa={.source="one",.bytes=3};
  CHECK(ui_context_set_clipboard(a,&fixture_clipboard,&fa));
  ui_text_input_focus("edit"); edit(left,sizeof(left),NULL,0);
  press(left,2,PLATFORM_KEY_A,PLATFORM_MOD_CONTROL); type(left,2,0x20ac);
  CHECK(!strcmp(left,"a")&&ui_text_field_state("edit")->selection_anchor==0);
  fa.source=""; fa.bytes=0; press(left,2,PLATFORM_KEY_V,PLATFORM_MOD_CONTROL); CHECK(!strcmp(left,"a"));
  fa.bytes=1048577; press(left,2,PLATFORM_KEY_V,PLATFORM_MOD_CONTROL); CHECK(!strcmp(left,"a")&&fa.releases==2);
  UIContext *b=context(); char right[128]="b";
  ClipboardFixture fb={.source="two",.bytes=3};
  CHECK(ui_context_set_clipboard(b,&fixture_clipboard,&fb));
  ui_text_input_focus("edit"); edit(right,sizeof(right),NULL,0);
  press(right,sizeof(right),PLATFORM_KEY_A,PLATFORM_MOD_CONTROL);
  press(right,sizeof(right),PLATFORM_KEY_V,PLATFORM_MOD_CONTROL); CHECK(!strcmp(right,"two"));
  CHECK(ui_context_set_current(a));
  type(left,sizeof(left),'x'); CHECK(!strcmp(left,"x"));
  press(left,sizeof(left),PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL); CHECK(!strcmp(left,"a"));
  CHECK(fa.reads==2 && fb.reads==1);
  InputKeyEvent paste=key(PLATFORM_KEY_V,PLATFORM_MOD_CONTROL);
  UIFrameInput lost={.width=640,.height=480,.focus_lost=true,.events=&paste,.event_count=1};
  CHECK(ui_begin_frame_input(&lost));
  CHECK(!ui_context_set_clipboard(a,NULL,NULL));
  ui_text_input("edit",font,ANCHOR_TOP_LEFT,0,14,20,20,100,left,sizeof(left),NULL);
  ui_end_frame(); CHECK(!strcmp(left,"a")&&fa.reads==2);
  char *large=malloc(24000); CHECK(large); memset(large,'a',20000); large[20000]=0;
  ui_text_input_focus("edit"); edit(large,24000,NULL,0);
  for(int i=0;i<5;++i) type(large,24000,'x');
  UITextInputState *st=ui_text_field_state("edit");
  CHECK(st->history_bytes<=UI_TEXT_HISTORY_BYTES&&st->history_count==3);
  for(int i=0;i<8;++i) press(large,24000,PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL);
  CHECK(strlen(large)==20003); free(large);
  CHECK(ui_context_destroy(a)); CHECK(ui_context_destroy(b));
}
int text_edit_suite(void) {
  edits(); capacity_history(); isolation_and_limits();
  CHECK(clips==0);
  puts("UI text editing: PASS (UTF-8 selection, clipboard ownership/failure, bounded undo/redo, external replacement, capacity)");
  return 0;
}

#ifndef UI_TEXT_EDIT_NO_MAIN
int main(void) { return text_edit_suite(); }
#endif
