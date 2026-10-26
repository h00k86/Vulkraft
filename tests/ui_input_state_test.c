/* White-box deterministic checks for the canonical input ownership state.
 *
 * Build from the repository root with:
 *   gcc -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
 *     -Iinclude -I. tests/ui_input_state_test.c -Wl,--gc-sections -lm \
 *     -o /tmp/ui_input_state_test
 */
#define DEBUG 1
#define EDITOR_ENABLED 1
#include "../src/ui/ui.c"

float g_virtualWidth = 1280.0f;
float g_virtualHeight = 800.0f;
#include <assert.h>
#include <stdio.h>

/* MACROTASK AUDIT-R1 fase D: stub headless per il link -- ui_file_picker_
 * cancel() (esercitata sotto) chiama texture_manager_preview_clear(APP*)
 * per liberare l'anteprima immagine del picker, un lavoro che richiede un
 * contesto Vulkan reale (GPU/device) che questo test white-box non ha e
 * non gli serve (verifica solo lo stato di ownership dell'input, mai il
 * contenuto grafico). Nessun link al vero texture_manager.c/Vulkan solo
 * per soddisfare questo simbolo mai esercitato funzionalmente qui. */


void texture_manager_preview_clear(APP *app) { (void)app; }

static void reset_input_state(void) {
  g_ui_frame_serial = 1;
  memset(g_section_states, 0, sizeof(g_section_states));
  g_section_state_count = 0;
  memset(ui_states, 0, sizeof(ui_states));
  ui_state_count = 0;
  memset(slider_states, 0, sizeof(slider_states));
  slider_state_count = 0;
  memset(text_input_states, 0, sizeof(text_input_states));
  text_input_state_count = 0;
  memset(float_field_states, 0, sizeof(float_field_states));
  float_field_state_count = 0;
  memset(dropdown_states, 0, sizeof(dropdown_states));
  dropdown_state_count = 0;
  memset(g_scroll_states, 0, sizeof(g_scroll_states));
  g_scroll_state_count = 0;
  memset(g_modal_states, 0, sizeof(g_modal_states));
  g_modal_state_count = 0;
  memset(&g_file_picker, 0, sizeof(g_file_picker));
  memset(&g_menu, 0, sizeof(g_menu));
}

static void check_bounded_state_reuse(void) {
  char id[64];
  for (int i = 0; i < MAX_UI_ELEMENTS; ++i) {
    snprintf(id, sizeof(id), "old_widget_%d", i);
    assert(find_or_create_state(id));
  }
  assert(ui_state_count == MAX_UI_ELEMENTS);
  ++g_ui_frame_serial;
  UIState *fresh = find_or_create_state("options_back");
  assert(fresh && strcmp(fresh->id, "options_back") == 0);
  assert(ui_state_count == MAX_UI_ELEMENTS);

  for (int i = 0; i < MAX_UI_SLIDERS; ++i) {
    snprintf(id, sizeof(id), "old_slider_%d", i);
    assert(find_or_create_slider_state(id));
  }
  ++g_ui_frame_serial;
  assert(find_or_create_slider_state("options_audio_volume"));

  for (int i = 0; i < MAX_UI_DROPDOWNS; ++i) {
    snprintf(id, sizeof(id), "old_dropdown_%d", i);
    assert(find_or_create_dropdown_state(id));
  }
  ++g_ui_frame_serial;
  assert(find_or_create_dropdown_state("options_video_mode"));
}

static void check_section_scope_and_layout(void) {
  char left_key[256], right_key[256];
  assert(ui_push_id("left_parent"));
  snprintf(left_key, sizeof(left_key), "%s", ui_scoped_id("shared"));
  UISectionState *left = find_or_create_section_state(left_key);
  ui_pop_id();
  assert(ui_push_id("right_parent"));
  snprintf(right_key, sizeof(right_key), "%s", ui_scoped_id("shared"));
  UISectionState *right = find_or_create_section_state(right_key);
  ui_pop_id();
  assert(left && right && left != right);
  assert(strcmp(left->id, right->id) != 0);
  left->open = false;
  assert(!left->open && right->open);

  UILayout parent = ui_layout_begin((UIRect){0, 10, 300, 500}, 1.0f);
  parent.cursor_y = 50.0f;
  assert(ui_push_id("collapsed"));
  UISection collapsed = {
      .layout = {.cursor_y = 180.0f}, .parent = &parent, .open = false};
  ui_section_end(&collapsed);
  assert(parent.cursor_y == 50.0f);

  assert(ui_push_id("expanded"));
  UISection expanded = {
      .layout = {.cursor_y = 180.0f}, .parent = &parent, .open = true};
  ui_section_end(&expanded);
  assert(parent.cursor_y == 180.0f);

  UIPropertyLayout properties =
      ui_property_layout_begin(0.0f, parent.cursor_y, 300.0f, 1.0f);
  properties.cursor_y = 240.0f;
  ui_property_layout_commit(&parent, &properties);
  assert(parent.cursor_y == 240.0f);
  ui_property_layout_commit(&parent, &properties);
  assert(parent.cursor_y == 240.0f);
}

int main(void) {
  reset_input_state();
  assert(!ui_has_pointer_capture());
  assert(!ui_has_keyboard_capture());

  ui_text_input_focus("field");
  assert(!ui_has_pointer_capture());
  assert(ui_has_keyboard_capture());
  ui_release_input_capture();
  assert(!ui_has_keyboard_capture());

  snprintf(g_modal_states[0].id, sizeof(g_modal_states[0].id), "modal");
  g_modal_states[0].wasOpen = true;
  g_modal_states[0].submittedThisFrame = true;
  g_modal_state_count = 1;
  assert(ui_has_pointer_capture() && ui_has_keyboard_capture());
  ui_modal_end("modal", false);
  assert(!ui_has_input_capture());

  snprintf(g_file_picker.id, sizeof(g_file_picker.id), "picker");
  g_file_picker.open = true;
  snprintf(g_modal_states[0].id, sizeof(g_modal_states[0].id), "picker");
  g_modal_states[0].wasOpen = true;
  assert(ui_has_pointer_capture() && ui_has_keyboard_capture());
  ui_file_picker_cancel();
  assert(!ui_has_input_capture());

  g_menu.depth = 1;
  assert(ui_has_pointer_capture() && ui_has_keyboard_capture());
  ui_menu_close_hierarchy();
  assert(!ui_has_input_capture());

  g_scroll_state_count = 1;
  snprintf(g_scroll_states[0].id, sizeof(g_scroll_states[0].id), "scroll");
  g_scroll_states[0].dragging = true;
  assert(ui_has_pointer_capture());
  g_mouse_down = false;
  for (int i = 0; i < g_scroll_state_count; ++i)
    g_scroll_states[i].dragging = false;
  assert(!ui_has_pointer_capture());

  /* Storage alone is not ownership: inactive menu submissions, scroll
   * offsets and an unfocused widget must leave both capture channels free. */
  g_menu.submission_count = 1;
  g_scroll_states[0].offset = 42.0f;
  ui_text_input_focus("stale");
  ui_release_input_capture();
  assert(!ui_has_input_capture());

  reset_input_state();
  check_bounded_state_reuse();
  reset_input_state();
  check_section_scope_and_layout();

  puts("UI input/section state checks: PASS");
  return 0;
}
