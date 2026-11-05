#include "../src/ui/ui_state_ids_internal.h"
/* Exercise the real widget code with headless input/drawing adapters. */
#include "../src/ui/ui.c"
#include "ui_test_renderer.h"
#include "ui_app_test_adapter.h"
#include <assert.h>
#include "../src/input/keyboard.c"

float g_virtualWidth = 1280, g_virtualHeight = 800;
static int keys[PLATFORM_KEY_LAST + 1];
float input_scroll_take(void) { return 0; }
int platform_key_state(PlatformWindow *window, int key) { return keys[key]; }
static PlatformKeyCallback mock_key;
static PlatformCharCallback mock_character;
static PlatformFocusCallback mock_focus;
PlatformKeyCallback platform_set_key_callback(PlatformWindow *w, PlatformKeyCallback callback) {
  PlatformKeyCallback old=mock_key; mock_key=callback; return old;
}
PlatformCharCallback platform_set_char_callback(PlatformWindow *w, PlatformCharCallback callback) {
  PlatformCharCallback old=mock_character; mock_character=callback; return old;
}
PlatformFocusCallback platform_set_focus_callback(PlatformWindow *w, PlatformFocusCallback callback) {
  PlatformFocusCallback old=mock_focus; mock_focus=callback; return old;
}
static void test_keyboard_frame(APP *app) {
  static int previous[PLATFORM_KEY_LAST+1];
  input_keyboard_register(app->window);
  for (int key=PLATFORM_KEY_SPACE;key<=PLATFORM_KEY_LAST;++key) {
    if (keys[key]!=previous[key]) mock_key(app->window,key,0,keys[key],0);
    previous[key]=keys[key];
  }
  ui_keyboard_begin_frame(app);
}
void draw_quad(APP *a, float x, float y, float w, float h, float r, float g, float b) {}
void draw_hud_paper_panel(APP *a, float x, float y, float w, float h,
                          const float f[4], const float c[3], bool focus, uint32_t seed) {}
#ifndef TEST_CUSTOM_TEXT_RENDERING
float measure_text_width(Font *f, const char *t, float s) { return 0; }
void draw_text_colored(APP *a, Font *f, const char *t, float x, float y, float s, float r, float g, float b) {}

#endif

static void check_scopes(void) {
  assert(strcmp(ui_scoped_id(NULL), "widget") == 0);
  assert(ui_push_id("panel"));
  assert(strcmp(ui_scoped_id("field"), "panel/field") == 0);
  ui_pop_id();
  const int lengths[] = {1, 15, 60, 63};
  char scope[64], local[512];
  memset(local, 'y', sizeof(local) - 1); local[sizeof(local) - 1] = 0;
  for (unsigned n = 0; n < sizeof(lengths) / sizeof(lengths[0]); ++n) {
    memset(scope, 'x', (size_t)lengths[n]); scope[lengths[n]] = 0;
    for (int depth = 1; depth <= MAX_UI_ID_STACK; ++depth) {
      assert(ui_push_id(scope));
      /* Cross every ring-buffer boundary, including the last buffer. */
      for (int i = 0; i < UI_SCOPED_ID_BUFFER_COUNT * 2; ++i) {
        const char *id = ui_scoped_id(i % 2 ? local : "widget");
        assert(memchr(id, '\0', sizeof(ui_state_ids(ui_ctx())->g_scoped_id_buffers[0])));
      }
    }
    for (int i = 0; i < MAX_UI_ID_STACK; ++i) ui_pop_id();
  }
}

static void field(char *buffer, int capacity) {
  APP app = {.window = (PlatformWindow *)1};
  test_keyboard_frame(&app);
  Font font = {.ascender = 1};
  ui_text_input("field", &font, ANCHOR_TOP_LEFT, 0, 14, 0, 0, 100,
                buffer, capacity, NULL);
}

static void check_cursor(void) {
  char buffer[32] = "original text";
  UITextInputState *state = ui_text_field_state("field");
  state->focused = true; state->caret = 13;
  field(buffer, sizeof(buffer));
  strcpy(buffer, "a"); /* Caller replaces text between frames. */
  keys[PLATFORM_KEY_BACKSPACE] = PLATFORM_PRESS;
  field(buffer, sizeof(buffer));
  assert(buffer[0] == 0 && state->caret == 0);
  keys[PLATFORM_KEY_BACKSPACE] = PLATFORM_RELEASE;
  field(buffer, sizeof(buffer));

  strcpy(buffer, "ab"); state->caret = -10;
  keys[PLATFORM_KEY_DELETE] = PLATFORM_PRESS;
  field(buffer, sizeof(buffer));
  assert(strcmp(buffer, "b") == 0 && state->caret == 0);
  keys[PLATFORM_KEY_DELETE] = PLATFORM_RELEASE;
  field(buffer, sizeof(buffer));

  strcpy(buffer, "longer"); state->caret = 6;
  strcpy(state->original, "a");
  keys[PLATFORM_KEY_ESCAPE] = keys[PLATFORM_KEY_BACKSPACE] = PLATFORM_PRESS;
  field(buffer, sizeof(buffer));
  assert(state->caret >= 0 && state->caret <= (int)strlen(buffer));
  memset(keys, 0, sizeof(keys));
  state->focused = true;
  field(buffer, sizeof(buffer));

  char empty[1] = {0}; state->caret = 100;
  keys[PLATFORM_KEY_BACKSPACE] = PLATFORM_PRESS;
  field(empty, sizeof(empty));
  assert(empty[0] == 0 && state->caret == 0);
  char unterminated[2] = {'a', 'b'};
  field(unterminated, sizeof(unterminated));
  assert(unterminated[0] == 'a' && unterminated[1] == 'b');
}

int main(void) {
  UIContext *test_ui=ui_context_create();
  bind_test_renderer(test_ui);
  assert(test_ui && ui_context_set_current(test_ui));
  check_scopes();
  check_cursor();
  puts("UI memory safety regressions: PASS");
  assert(ui_context_destroy(test_ui));
  return 0;
}

static int preview_clear_count;
void texture_manager_preview_clear(APP *app) { ++preview_clear_count; }
