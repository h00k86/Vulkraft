#include "../src/ui/ui_state_text_fields_internal.h"
#include "../src/ui/ui_state_style_internal.h"
#include "../src/ui/ui_state_render_internal.h"
#include "../src/ui/ui_state_ids_internal.h"
#include "../src/ui/ui_state_frame_internal.h"
#include "../src/ui/ui_state_dropdowns_internal.h"
/* Real UI implementation, mocked drawing: independent ownership and lifecycle. */
#include <stdlib.h>
static int fail_allocation;
static void *context_test_malloc(size_t size) {
  return fail_allocation ? NULL : malloc(size);
}
#define malloc context_test_malloc
#define main memory_regression_main
#include "ui_memory_safety_test.c"
#undef main
#undef malloc

int main(void) {
  assert(!ui_context_get_current());
  fail_allocation=1;
  assert(!ui_context_create());
  fail_allocation=0;
  UIContext *a=ui_context_create(), *b=ui_context_create();
  bind_test_renderer(a); bind_test_renderer(b);
  assert(a && b && a!=b);
  APP app={.window=(PlatformWindow *)1};
  assert(ui_context_set_current(a));
  UITheme theme=ui_get_theme();
  theme.row_height=97;
  ui_set_theme(theme);
  assert(ui_push_id("first"));
  ui_notify(UI_TOAST_SUCCESS,"first context");
  UITextInputState *ta=ui_text_field_state("same-id");
  ta->focused=true;
  char long_text[1024]; memset(long_text,'x',sizeof(long_text)-1);
  long_text[sizeof(long_text)-1]=0;
  assert(ui_text_snapshot(ta,long_text) && ta->original_long);
  UIDropdownState *da=test_dropdown_state("same-id");
  ui_state_dropdowns(a)->g_dropdown_click_owner=da;
  input_keyboard_register(app.window);
  mock_key(app.window,PLATFORM_KEY_A,0,PLATFORM_PRESS,0);
  ui_keyboard_begin_frame(&app);
  assert(ui_key_pressed(PLATFORM_KEY_A));
  size_t count=0;
  assert(!input_keyboard_events(&count)[0].consumed && count==1);
  ui_state_render(a)->preview_owned=true;
  assert(ui_context_set_current(b));
  assert(preview_clear_count==1 && !ui_state_render(a)->preview_owned);
  assert(ui_get_theme().row_height!=97);
  assert(!strcmp(ui_scoped_id("field"),"field"));
  assert(ui_state_style(b)->g_toast_count==0 && ui_state_frame(b)->key_event_count==0);
  UITextInputState *tb=ui_text_field_state("same-id");
  assert(tb!=ta && !tb->focused && !tb->original_long);
  assert(test_dropdown_state("same-id")!=da);
  assert(ui_context_reset(a));
  assert(ui_context_get_current()==b && preview_clear_count==1);
  assert(ui_context_set_current(a));
  assert(!ui_state_text_fields(a)->text_input_state_count && !ui_state_dropdowns(a)->g_dropdown_click_owner);
  assert(!ui_state_ids(a)->g_id_stack_depth && !ui_state_style(a)->g_toast_count && !ui_state_frame(a)->key_event_count);
  assert(ui_get_theme().row_height!=97);
  ui_begin_frame(&app,0,0,false,0.016f);
  assert(!ui_context_set_current(b));
  assert(!ui_context_set_current(NULL));
  assert(!ui_context_reset(a) && !ui_context_destroy(a));
  assert(ui_context_get_current()==a);
  ui_end_frame();
  /* Destroy a non-current context with its own dynamically allocated snapshot. */
  assert(ui_context_set_current(b));
  assert(ui_text_snapshot(tb,long_text));
  assert(ui_context_set_current(a));
  assert(ui_context_destroy(b) && ui_context_get_current()==a);
  ui_state_render(a)->preview_owned=true;
  assert(ui_context_destroy(a));
  assert(preview_clear_count==2 && !ui_context_get_current());
  assert(ui_context_destroy(NULL) && !ui_context_reset(NULL));
  input_keyboard_unregister(app.window);
  puts("UI context: PASS (allocation failure, isolation, input ownership, reset, frame guards, teardown)");
  return 0;
}
