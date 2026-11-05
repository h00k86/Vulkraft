/* Link only the editor implementation: no UIContext, frame or renderer. */
#include "../src/ui/ui_text_edit_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"text module:%d: %s\n",__LINE__,#x); exit(1); } } while(0)
#ifdef UI_TEXT_TEST_FAULT_ALLOC
static bool fail_alloc;
void *__real_malloc(size_t bytes);
void *__wrap_malloc(size_t bytes) { return fail_alloc?NULL:__real_malloc(bytes); }
#endif

typedef struct {
  const char *source;
  size_t bytes;
  char copied[128];
  int reads,releases,writes;
  bool fail_write;
} Clipboard;
static bool read_clipboard(void *user,const char **text,size_t *bytes) {
  Clipboard *c=user; ++c->reads; *text=c->source; *bytes=c->bytes; return true;
}
static void release_clipboard(void *user,const char *text) {
  Clipboard *c=user; CHECK(text==c->source); ++c->releases;
}
static bool write_clipboard(void *user,const char *text) {
  Clipboard *c=user; ++c->writes;
  if(c->fail_write) return false;
  snprintf(c->copied,sizeof(c->copied),"%s",text); return true;
}
static const UIClipboard clipboard={read_clipboard,release_clipboard,write_clipboard};
static void key(UITextInputState *s,char *buffer,int capacity,int code,int mods,Clipboard *c) {
  int length;
  CHECK(ui_text_edit_prepare(s,buffer,capacity,&length));
  InputKeyEvent e={.kind=INPUT_KEY,.key=code,.mods=mods,.action=PLATFORM_PRESS};
  ui_text_edit_event(s,buffer,capacity,&e,&clipboard,c);
  ui_text_edit_observe(s,buffer);
}
static void type(UITextInputState *s,char *buffer,int capacity,uint32_t cp,Clipboard *c) {
  int length;
  CHECK(ui_text_edit_prepare(s,buffer,capacity,&length));
  InputKeyEvent e={.kind=INPUT_CHARACTER,.codepoint=cp};
  ui_text_edit_event(s,buffer,capacity,&e,&clipboard,c);
  ui_text_edit_observe(s,buffer);
}
static void dispose(UITextInputState *s) {
  ui_text_history_clear(s); free(s->original_long);
}
static void editing(void) {
  UITextInputState a={.focused=true,.caret=8,.selection_anchor=8}, b={.caret=1,.selection_anchor=1};
  char left[128]="aè🙂z",right[128]="B";
  Clipboard ca={0},cb={.source="other",.bytes=5};
  CHECK(ui_text_snapshot(&a,left)); ui_text_edit_observe(&a,left);
  key(&a,left,sizeof(left),PLATFORM_KEY_LEFT,PLATFORM_MOD_SHIFT,&ca);
  key(&a,left,sizeof(left),PLATFORM_KEY_LEFT,PLATFORM_MOD_SHIFT,&ca);
  CHECK(a.caret==3&&a.selection_anchor==8);
  key(&a,left,sizeof(left),PLATFORM_KEY_C,PLATFORM_MOD_CONTROL,&ca);
  CHECK(!strcmp(ca.copied,"🙂z"));
  ca.fail_write=true;
  key(&a,left,sizeof(left),PLATFORM_KEY_X,PLATFORM_MOD_CONTROL,&ca);
  CHECK(!strcmp(left,"aè🙂z"));
  type(&a,left,sizeof(left),0x20ac,&ca); CHECK(!strcmp(left,"aè€"));
  key(&a,left,sizeof(left),PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL,&ca);
  CHECK(!strcmp(left,"aè🙂z")&&a.caret==3&&a.selection_anchor==8);
  key(&a,left,sizeof(left),PLATFORM_KEY_Y,PLATFORM_MOD_CONTROL,&ca); CHECK(!strcmp(left,"aè€"));

  /* Clipboard input need not end in NUL; controls are filtered. */
  const char source[]={'Q',(char)0xe2,(char)0x82,(char)0xac,'\n'};
  ca.source=source; ca.bytes=sizeof(source);
  key(&a,left,sizeof(left),PLATFORM_KEY_A,PLATFORM_MOD_CONTROL,&ca);
  key(&a,left,sizeof(left),PLATFORM_KEY_V,PLATFORM_MOD_CONTROL,&ca);
  CHECK(!strcmp(left,"Q€")&&ca.reads==1&&ca.releases==1);
  key(&b,right,sizeof(right),PLATFORM_KEY_A,PLATFORM_MOD_CONTROL,&cb);
  key(&b,right,sizeof(right),PLATFORM_KEY_V,PLATFORM_MOD_CONTROL,&cb);
  CHECK(!strcmp(right,"other")&&!strcmp(left,"Q€")&&cb.reads==1&&cb.releases==1);
  CHECK(ca.writes==2&&cb.writes==0);

  /* An externally replaced document invalidates history and the Escape baseline. */
  strcpy(left,"external"); int length;
  CHECK(ui_text_edit_prepare(&a,left,sizeof(left),&length)&&length==8);
  CHECK(!a.history_count&&!strcmp(a.original,"external"));
  ui_text_edit_observe(&a,left);
  key(&a,left,sizeof(left),PLATFORM_KEY_END,0,&ca); type(&a,left,sizeof(left),'!',&ca);
  ui_text_edit_cancel(&a,left,sizeof(left)); CHECK(!strcmp(left,"external")&&!a.focused&&!a.history_count);

  /* Reconcile offsets to scalar boundaries; reject malformed/unterminated input. */
  UITextInputState offsets={.caret=2,.selection_anchor=4}; char unicode[]="aè🙂";
  CHECK(ui_text_edit_prepare(&offsets,unicode,sizeof(unicode),&length));
  CHECK(offsets.caret==1&&offsets.selection_anchor==3);
  char bad[]={(char)0xc0,(char)0xaf,0},unterminated[]={'x','y'};
  CHECK(!ui_text_edit_prepare(&offsets,bad,sizeof(bad),&length));
  CHECK(!ui_text_edit_prepare(&offsets,unterminated,sizeof(unterminated),&length));
  dispose(&a); dispose(&b); dispose(&offsets);
}
static void history_limits(void) {
  UITextInputState s={.caret=1,.selection_anchor=1}; char text[64]="B"; Clipboard c={0};
  for(int n=0;n<25;++n) type(&s,text,sizeof(text),'x',&c);
  CHECK(s.history_count==UI_TEXT_HISTORY_COUNT&&s.history_bytes<=UI_TEXT_HISTORY_BYTES);
  for(int n=0;n<40;++n) key(&s,text,sizeof(text),PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL,&c);
  CHECK(strlen(text)==10);
  type(&s,text,sizeof(text),'q',&c);
  key(&s,text,sizeof(text),PLATFORM_KEY_Y,PLATFORM_MOD_CONTROL,&c);
  CHECK(strlen(text)==11&&text[10]=='q'); dispose(&s);
}
#ifdef UI_TEXT_TEST_FAULT_ALLOC
static void allocation_failures(void) {
  UITextInputState s={.focused=true}; char original[400],replacement[400];
  memset(original,'a',sizeof(original)-1); original[sizeof(original)-1]=0;
  memset(replacement,'b',sizeof(replacement)-1); replacement[sizeof(replacement)-1]=0;
  CHECK(ui_text_snapshot(&s,original)); char *saved=s.original_long;
  s.needs_snapshot=true; fail_alloc=true;
  CHECK(!ui_text_snapshot(&s,replacement));
  fail_alloc=false;
  CHECK(s.original_long==saved&&!strcmp(saved,original)&&s.needs_snapshot);
  free(s.original_long); s.original_long=NULL;

  char text[32]="keep"; Clipboard c={.source="new",.bytes=3};
  s.caret=4; s.selection_anchor=0; ui_text_edit_observe(&s,text);
  fail_alloc=true;
  key(&s,text,sizeof(text),PLATFORM_KEY_V,PLATFORM_MOD_CONTROL,&c);
  CHECK(!strcmp(text,"keep")&&c.reads==1&&c.releases==1&&s.history_count==1);
  key(&s,text,sizeof(text),PLATFORM_KEY_X,PLATFORM_MOD_CONTROL,&c);
  CHECK(!strcmp(text,"keep")&&!c.writes&&s.history_count==1);
  type(&s,text,sizeof(text),'x',&c); CHECK(!strcmp(text,"x")&&!s.history_count);
  fail_alloc=false;
  ui_text_edit_observe(&s,text); CHECK(s.history_count==1);
  dispose(&s);
}
#endif
int main(void) {
  editing(); history_limits();
#ifdef UI_TEXT_TEST_FAULT_ALLOC
  allocation_failures();
#endif
  puts("Text editor module: PASS (no UI context/renderer, UTF-8, clipboard ownership/isolation, external changes, cancel, bounded history)");
  return 0;
}
