#define main high_regression_main
#include "ui_memory_safety_test.c"
#undef main
#include <limits.h>

static APP app = {.window = (PlatformWindow *)1};
static Font font = {.ascender = 1};
static void frame(int key) {
  memset(keys, 0, sizeof(keys));
  if (key) keys[key] = PLATFORM_PRESS;
  test_keyboard_frame(&app);
}
static bool edit_int(const char *text, int *value) {
  frame(0);
  UIFloatFieldState *state = ui_numeric_state("integer");
  state->focused = true;
  snprintf(state->buffer, sizeof(state->buffer), "%s", text);
  frame(PLATFORM_KEY_ENTER);
  return ui_number_field("integer", &font, 0, 0, 100, 20, 14,
                          NULL, 0, 0, value, INT_MIN, INT_MAX);
}
int main(void) {
  UIContext *test_ui=ui_context_create();
  bind_test_renderer(test_ui);
  assert(test_ui && ui_context_set_current(test_ui));
  char a[UI_ID_CAPACITY], b[UI_ID_CAPACITY], invalid[UI_ID_CAPACITY + 1];
  memset(a, 'a', sizeof(a)-1); a[sizeof(a)-1] = 0;
  strcpy(b, a); b[200] = 'b';
  memset(invalid, 'x', sizeof(invalid)-1); invalid[sizeof(invalid)-1] = 0;
  UITextInputState *sa = ui_text_field_state(a);
  UITextInputState *sb = ui_text_field_state(b);
  assert(sa && sb && sa != sb);
  assert(ui_text_field_state(a) == sa);
  assert(ui_text_field_state(b) == sb);
  assert(!ui_text_field_state(invalid));
  assert(!ui_push_id(invalid));
  assert(strcmp(ui_scoped_id(a), a) == 0);
  assert(ui_push_id("scope"));
  assert(!*ui_scoped_id(a));
  ui_pop_id();
  assert(strcmp(ui_scoped_id("normal"), "normal") == 0);

  int value = 0;
  assert(edit_int("2147483647", &value) && value == INT_MAX);
  assert(!edit_int("2147483647", &value) && value == INT_MAX);
  assert(edit_int("-2147483648", &value) && value == INT_MIN);
  assert(edit_int("16777217", &value) && value == 16777217);
  assert(!edit_int("1.5", &value) && value == 16777217);
  assert(!edit_int("999999999999999999999999999", &value));

  char buffer[32] = "abc";
  UITextInputState *s = ui_text_field_state("field");
  s->focused = true; s->caret = 3;
  frame(0);
  keys[PLATFORM_KEY_ENTER] = PLATFORM_PRESS;
  field(buffer, sizeof(buffer));
  assert(!s->focused);
  frame(0); // Release while no widget is focused or submitted.
  s->focused = true;
  keys[PLATFORM_KEY_ENTER] = PLATFORM_PRESS;
  field(buffer, sizeof(buffer));
  assert(!s->focused);
  frame(0);
  frame(PLATFORM_KEY_A);
  assert(ui_key_pressed(PLATFORM_KEY_A));
  assert(!ui_key_pressed(PLATFORM_KEY_A));
  puts("UI long IDs, exact integers and focus/key transitions: PASS");
  assert(ui_context_destroy(test_ui));
}
