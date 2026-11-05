#define TEST_CUSTOM_TEXT_RENDERING
#define main old_memory_test_main
#include "ui_memory_safety_test.c"
#undef main
float measure_text_width(Font *font,const char *s,float scale) {
  assert(utf8_valid(s,strlen(s)));
  float width=0; size_t n=strlen(s);
  for(size_t p=0;p<n;p=utf8_next(s,n,p)) width+=10;
  return width;
}
void draw_text_colored(APP *a,Font *f,const char *s,float x,float y,float z,float r,float g,float b) {
  assert(utf8_valid(s,strlen(s)));
}
static APP app={.window=(PlatformWindow *)1};
static Font font={.ascender=1};
static void unicode_field(char *s,int n) {
  ui_keyboard_begin_frame(&app);
  ui_text_input("unicode",&font,ANCHOR_TOP_LEFT,0,14,0,0,100,s,n,NULL);
}
static void key(int k) { mock_key(app.window,k,0,PLATFORM_PRESS,0); }
int main(void) {
  UIContext *test_ui=ui_context_create();
  bind_test_renderer(test_ui);
  assert(test_ui && ui_context_set_current(test_ui));
  // Strict decode, all scalar ranges, truncated and forbidden sequences.
  for (uint32_t cp=1;cp<=0x10ffff;++cp) {
    char b[4]; size_t n=utf8_encode(cp,b);
    if(cp>=0xd800 && cp<=0xdfff) { assert(!n); continue; }
    uint32_t decoded; assert(utf8_decode(b,n,&decoded)==n && decoded==cp);
    assert(utf8_valid(b,n));
    if(n>1) assert(!utf8_valid(b,n-1));
  }
  const char *bad[]={"\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\x80","\xe2\x82"};
  for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i) assert(!utf8_valid(bad[i],strlen(bad[i])));
  char invalid[4]; assert(!utf8_encode(0x110000,invalid));
  char clipped[32]="èèèèèè";
  ui_ellipsize_left(&font,clipped,1,50,clipped,sizeof(clipped));
  assert(!strcmp(clipped,"...èè"));
  strcpy(clipped,"èèèèèè");
  ui_ellipsize_right(&font,clipped,1,50,clipped,sizeof(clipped));
  assert(!strcmp(clipped,"èè..."));
  char cut[4]; utf8_copy(cut,sizeof(cut),"èè"); assert(!strcmp(cut,"è"));
  input_keyboard_register(app.window);
  UITextInputState *st=ui_text_field_state("unicode"); st->focused=true;
  char s[64]="";
  mock_character(app.window,0xe8); mock_character(app.window,0x20ac); mock_character(app.window,0x1f642);
  unicode_field(s,sizeof(s)); assert(!strcmp(s,"è€🙂") && st->caret==9);
  key(PLATFORM_KEY_LEFT); key(PLATFORM_KEY_BACKSPACE); unicode_field(s,sizeof(s));
  assert(!strcmp(s,"è🙂") && st->caret==2);
  key(PLATFORM_KEY_DELETE); unicode_field(s,sizeof(s)); assert(!strcmp(s,"è"));
  key(PLATFORM_KEY_HOME); mock_character(app.window,0x4e2d); unicode_field(s,sizeof(s));
  assert(!strcmp(s,"中è") && st->caret==3);
  key(PLATFORM_KEY_END); key(PLATFORM_KEY_BACKSPACE); unicode_field(s,sizeof(s)); assert(!strcmp(s,"中"));
  strcpy(s,"èx"); st->caret=1; unicode_field(s,sizeof(s)); assert(st->caret==0);
  mock_character(app.window,0xd800); mock_character(app.window,0x110000);
  mock_character(app.window,0x85); mock_character(app.window,0x2028);
  unicode_field(s,sizeof(s)); assert(!strcmp(s,"èx"));
  char tiny[5]=""; st->caret=0;
  mock_character(app.window,0x1f642); mock_character(app.window,'a'); unicode_field(tiny,sizeof(tiny));
  assert(!strcmp(tiny,"🙂") && tiny[4]==0);
  char too_small[4]=""; st->caret=0;
  mock_character(app.window,0x1f642); unicode_field(too_small,sizeof(too_small)); assert(!*too_small);
  strcpy(s,"\xc0\xaf"); unicode_field(s,sizeof(s)); assert(!strcmp(s,"\xc0\xaf")); // rejected unchanged
  char large[1024], saved[1024];
  for(int i=0;i<300;++i) memcpy(large+2*i,"è",2);
  large[600]=0; strcpy(saved,large);
  st->focused=false; ui_text_input_focus("unicode"); unicode_field(large,sizeof(large));
  key(PLATFORM_KEY_BACKSPACE); unicode_field(large,sizeof(large)); assert(strlen(large)==598);
  key(PLATFORM_KEY_ESCAPE); unicode_field(large,sizeof(large)); assert(!strcmp(large,saved) && !st->original_long);
  input_keyboard_unregister(app.window);
  puts("UTF-8: all scalars, invalid sequences, editor boundaries, atomic insertion, long Escape: PASS");
  assert(ui_context_destroy(test_ui));
}
