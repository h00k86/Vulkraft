#include "../src/ui/ui_state_text_fields_internal.h"
#include "../src/ui/ui_state_sliders_internal.h"
#include "../src/ui/ui_state_picker_internal.h"
#include "../src/ui/ui_state_panels_internal.h"
#include "../src/ui/ui_state_numeric_internal.h"
#include "../src/ui/ui_state_modals_internal.h"
#include "../src/ui/ui_state_menu_internal.h"
#include "../src/ui/ui_state_frame_internal.h"
#include "../src/ui/ui_state_dropdowns_internal.h"
#include "../src/ui/ui_state_buttons_internal.h"
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
#define TEST_RENDER_STATE_ONLY
#include "ui_test_renderer.h"

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
  ui_state_frame(ui_ctx())->g_ui_frame_serial = 1;
  memset(ui_state_panels(ui_ctx())->g_section_states, 0, sizeof(ui_state_panels(ui_ctx())->g_section_states));
  ui_state_panels(ui_ctx())->g_section_state_count = 0;
  memset(ui_state_buttons(ui_ctx())->ui_states, 0, sizeof(ui_state_buttons(ui_ctx())->ui_states));
  ui_state_buttons(ui_ctx())->ui_state_count = 0;
  memset(ui_state_sliders(ui_ctx())->slider_states, 0, sizeof(ui_state_sliders(ui_ctx())->slider_states));
  ui_state_sliders(ui_ctx())->slider_state_count = 0;
  memset(ui_state_text_fields(ui_ctx())->text_input_states, 0, sizeof(ui_state_text_fields(ui_ctx())->text_input_states));
  ui_state_text_fields(ui_ctx())->text_input_state_count = 0;
  memset(ui_state_numeric(ui_ctx())->float_field_states, 0, sizeof(ui_state_numeric(ui_ctx())->float_field_states));
  ui_state_numeric(ui_ctx())->float_field_state_count = 0;
  for(int i=0;i<ui_state_dropdowns(ui_ctx())->dropdown_state_count;++i)
    free(ui_state_dropdowns(ui_ctx())->dropdown_states[i].popupOptions);
  memset(ui_state_dropdowns(ui_ctx())->dropdown_states, 0, sizeof(ui_state_dropdowns(ui_ctx())->dropdown_states));
  ui_state_dropdowns(ui_ctx())->dropdown_state_count = 0;
  memset(ui_state_panels(ui_ctx())->g_scroll_states, 0, sizeof(ui_state_panels(ui_ctx())->g_scroll_states));
  ui_state_panels(ui_ctx())->g_scroll_state_count = 0;
  memset(ui_state_modals(ui_ctx())->g_modal_states, 0, sizeof(ui_state_modals(ui_ctx())->g_modal_states));
  ui_state_modals(ui_ctx())->g_modal_state_count = 0;
  ui_state_picker(ui_ctx())->file_picker_open=false;
  ui_state_picker(ui_ctx())->file_picker_id[0]=0;
  for(int i=0;i<UI_MENU_MAX_DEPTH;++i) free(ui_state_menu(ui_ctx())->g_menu.levels[i].items);
  for(int i=0;i<MAX_UI_SUBMENU_SUBMISSIONS;++i) free(ui_state_menu(ui_ctx())->g_menu.submissions[i].items);
  memset(&ui_state_menu(ui_ctx())->g_menu, 0, sizeof(ui_state_menu(ui_ctx())->g_menu));
}

static void check_bounded_state_reuse(void) {
  char id[64];
  for (int i = 0; i < MAX_UI_ELEMENTS; ++i) {
    snprintf(id, sizeof(id), "old_widget_%d", i);
    assert(ui_button_state(id));
  }
  assert(ui_state_buttons(ui_ctx())->ui_state_count == MAX_UI_ELEMENTS);
  ++ui_state_frame(ui_ctx())->g_ui_frame_serial;
  UIState *fresh = ui_button_state("options_back");
  assert(fresh && strcmp(fresh->id, "options_back") == 0);
  assert(ui_state_buttons(ui_ctx())->ui_state_count == MAX_UI_ELEMENTS);

  for (int i = 0; i < MAX_UI_SLIDERS; ++i) {
    snprintf(id, sizeof(id), "old_slider_%d", i);
    assert(ui_slider_state(id));
  }
  ++ui_state_frame(ui_ctx())->g_ui_frame_serial;
  assert(ui_slider_state("options_audio_volume"));

  for (int i = 0; i < MAX_UI_DROPDOWNS; ++i) {
    snprintf(id, sizeof(id), "old_dropdown_%d", i);
    assert(test_dropdown_state(id));
  }
  ++ui_state_frame(ui_ctx())->g_ui_frame_serial;
  assert(test_dropdown_state("options_video_mode"));
}

static void check_section_scope_and_layout(void) {
  char left_key[256], right_key[256];
  Font font={.ascender=1};
  UILayout section_layout=ui_layout_begin((UIRect){0,0,300,500},1);
  assert(ui_push_id("left_parent"));
  snprintf(left_key, sizeof(left_key), "%s", ui_scoped_id("shared"));
  UISection left_section=ui_section_begin(&section_layout,"shared","Shared",&font);
  ui_section_end(&left_section);
  UISectionState *left=NULL;
  for(int i=0;i<ui_state_panels(ui_ctx())->g_section_state_count;++i)
    if(!strcmp(ui_state_panels(ui_ctx())->g_section_states[i].id,left_key)) left=&ui_state_panels(ui_ctx())->g_section_states[i];
  ui_pop_id();
  assert(ui_push_id("right_parent"));
  snprintf(right_key, sizeof(right_key), "%s", ui_scoped_id("shared"));
  UISection right_section=ui_section_begin(&section_layout,"shared","Shared",&font);
  ui_section_end(&right_section);
  UISectionState *right=NULL;
  for(int i=0;i<ui_state_panels(ui_ctx())->g_section_state_count;++i)
    if(!strcmp(ui_state_panels(ui_ctx())->g_section_states[i].id,right_key)) right=&ui_state_panels(ui_ctx())->g_section_states[i];
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
  UIContext *test_ui=ui_context_create();
  bind_test_renderer(test_ui);
  assert(test_ui && ui_context_set_current(test_ui));
  reset_input_state();
  assert(!ui_has_pointer_capture());
  assert(!ui_has_keyboard_capture());

  ui_text_input_focus("field");
  assert(!ui_has_pointer_capture());
  assert(ui_has_keyboard_capture());
  ui_release_input_capture();
  assert(!ui_has_keyboard_capture());

  snprintf(ui_state_modals(ui_ctx())->g_modal_states[0].id, sizeof(ui_state_modals(ui_ctx())->g_modal_states[0].id), "modal");
  ui_state_modals(ui_ctx())->g_modal_states[0].wasOpen = true;
  ui_state_modals(ui_ctx())->g_modal_states[0].submittedThisFrame = true;
  ui_state_modals(ui_ctx())->g_modal_state_count = 1;
  assert(ui_has_pointer_capture() && ui_has_keyboard_capture());
  ui_modal_end("modal", false);
  assert(!ui_has_input_capture());

  /* Optional picker lifecycle is covered by ui_file_picker_test. */
  ui_state_menu(ui_ctx())->g_menu.depth = 1;
  assert(ui_has_pointer_capture() && ui_has_keyboard_capture());
  ui_menu_close_hierarchy();
  assert(!ui_has_input_capture());

  ui_state_panels(ui_ctx())->g_scroll_state_count = 1;
  snprintf(ui_state_panels(ui_ctx())->g_scroll_states[0].id, sizeof(ui_state_panels(ui_ctx())->g_scroll_states[0].id), "scroll");
  ui_state_panels(ui_ctx())->g_scroll_states[0].dragging = true;
  assert(ui_has_pointer_capture());
  ui_state_frame(ui_ctx())->g_mouse_down = false;
  for (int i = 0; i < ui_state_panels(ui_ctx())->g_scroll_state_count; ++i)
    ui_state_panels(ui_ctx())->g_scroll_states[i].dragging = false;
  assert(!ui_has_pointer_capture());

  /* Storage alone is not ownership: inactive menu submissions, scroll
   * offsets and an unfocused widget must leave both capture channels free. */
  ui_state_menu(ui_ctx())->g_menu.submission_count = 1;
  ui_state_panels(ui_ctx())->g_scroll_states[0].offset = 42.0f;
  ui_text_input_focus("stale");
  ui_release_input_capture();
  assert(!ui_has_input_capture());

  reset_input_state();
  check_bounded_state_reuse();
  reset_input_state();
  check_section_scope_and_layout();

  puts("UI input/section state checks: PASS");
  assert(ui_context_destroy(test_ui));
  return 0;
}
