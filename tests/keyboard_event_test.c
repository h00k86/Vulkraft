#define main old_memory_test_main
#include "ui_memory_safety_test.c"
#undef main
#include <limits.h>
static APP app={.window=(PlatformWindow *)1};
static Font font={.ascender=1};
static void key_event(int key,int action,int mods) { mock_key(app.window,key,0,action,mods); }
static void characters(const char *s) { while(*s) mock_character(app.window,(unsigned char)*s++); }
static void text_field(char *s) {
  ui_text_input("events",&font,ANCHOR_TOP_LEFT,0,14,0,0,100,s,64,NULL);
}
static void tick(void) { ui_keyboard_begin_frame(&app); }
static int observed_keys,observed_characters,observed_focus;
static void observer_key(PlatformWindow *w,int k,int s,int a,int m) { ++observed_keys; }
static void observer_character(PlatformWindow *w,unsigned c) { ++observed_characters; }
static void observer_focus(PlatformWindow *w,int f) { ++observed_focus; }
int main(void) {
  UIContext *test_ui=ui_context_create();
  bind_test_renderer(test_ui);
  assert(test_ui && ui_context_set_current(test_ui));
  platform_set_key_callback(app.window,observer_key);
  platform_set_char_callback(app.window,observer_character);
  platform_set_focus_callback(app.window,observer_focus);
  input_keyboard_register(app.window);
  UITextInputState *state=ui_text_field_state("events");
  state->focused=true;
  char text[64]="";
  // Press and release before the first consumer; text arrives as a char event.
  key_event(PLATFORM_KEY_Z,PLATFORM_PRESS,0); characters("z"); key_event(PLATFORM_KEY_Z,PLATFORM_RELEASE,0);
  tick(); text_field(text);
  assert(!strcmp(text,"z") && observed_keys==2 && observed_characters==1);
  tick(); text_field(text); assert(!strcmp(text,"z")); // no replay

  text[0]=0; state->caret=0;
  characters("zaz"); // order differs from key-code order; same key twice
  key_event(PLATFORM_KEY_LEFT,PLATFORM_PRESS,0);
  key_event(PLATFORM_KEY_BACKSPACE,PLATFORM_PRESS,0);
  characters("!");
  key_event(PLATFORM_KEY_DELETE,PLATFORM_PRESS,0);
  key_event(PLATFORM_KEY_ENTER,PLATFORM_PRESS,0); characters("discarded");
  tick(); text_field(text);
  assert(!strcmp(text,"z!") && !state->focused);
  state->focused=true; tick(); text_field(text); assert(!strcmp(text,"z!"));

  key_event(PLATFORM_KEY_A,PLATFORM_PRESS,PLATFORM_MOD_CONTROL);
  key_event(PLATFORM_KEY_A,PLATFORM_RELEASE,PLATFORM_MOD_CONTROL);
  characters("aaa");
  key_event(PLATFORM_KEY_BACKSPACE,PLATFORM_PRESS,0);
  key_event(PLATFORM_KEY_BACKSPACE,PLATFORM_REPEAT,0);
  tick(); text_field(text); assert(!strcmp(text,"a"));

  UIFloatFieldState *number=ui_numeric_state("number");
  number->focused=true; strcpy(number->buffer,"0");
  key_event(PLATFORM_KEY_A,PLATFORM_PRESS,PLATFORM_MOD_CONTROL); characters("2.75");
  key_event(PLATFORM_KEY_ENTER,PLATFORM_PRESS,0);
  tick(); float value=0;
  assert(ui_float_field("number",&font,0,0,100,20,14,&value,-100,100));
  assert(value==2.75f && !number->focused);
  number->focused=true; strcpy(number->buffer,"0");
  key_event(PLATFORM_KEY_A,PLATFORM_PRESS,PLATFORM_MOD_CONTROL); characters("2147483647");
  key_event(PLATFORM_KEY_KP_ENTER,PLATFORM_PRESS,0);
  tick(); int integer=0;
  assert(ui_number_field("number",&font,0,0,100,20,14,NULL,0,0,&integer,INT_MIN,INT_MAX));
  assert(integer==INT_MAX);

  state->focused=true; characters("stale"); mock_focus(app.window,PLATFORM_FALSE);
  tick(); text_field(text);
  assert(!state->focused && !strcmp(text,"a") && observed_focus==1);
  state->focused=true; characters("background"); tick(); text_field(text);
  assert(!state->focused && !strcmp(text,"a"));
  mock_focus(app.window,PLATFORM_TRUE);
  tick();
  characters("no focused widget"); tick(); tick();
  state->focused=true; text_field(text); assert(!strcmp(text,"a"));

  int keys_before=observed_keys, chars_before=observed_characters;
  mock_focus(app.window,PLATFORM_FALSE);
  key_event(PLATFORM_KEY_ENTER,PLATFORM_PRESS,0); characters("x");
  mock_focus(app.window,PLATFORM_TRUE); characters("y");
  tick();
  size_t batch_count;
  input_keyboard_events(&batch_count);
  assert(input_keyboard_focus_lost() && !batch_count);
  assert(observed_keys==keys_before+1 && observed_characters==chars_before+2);
  state->focused=true; characters("b"); tick(); text_field(text);
  assert(!input_keyboard_focus_lost() && !strcmp(text,"ab"));
  strcpy(text,"a"); state->caret=1;

  for(int i=0;i<=INPUT_KEYBOARD_CAPACITY;++i) characters("x");
  tick(); text_field(text); assert(!state->focused && !strcmp(text,"a"));
  // Unregister/re-register cannot leak a pending character into a new session.
  characters("stale");
  platform_set_key_callback(app.window,observer_key); // later wrapper is removed too
  input_keyboard_unregister(app.window);
  assert(mock_key==observer_key && mock_character==observer_character && mock_focus==observer_focus);
  input_keyboard_register(app.window); state->focused=true;
  tick(); text_field(text); assert(!strcmp(text,"a"));
  input_keyboard_unregister(app.window);
  puts("Keyboard ordered bursts, repeats, modifiers, numeric commits, focus and lifecycle: PASS");
  assert(ui_context_destroy(test_ui));
}
