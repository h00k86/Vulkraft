/* Contract-level test: typed stores, no physical UIContext definition or ui.c. */
#include "../src/ui/ui_state_frame_internal.h"
#include "../src/ui/ui_state_buttons_internal.h"
#include "../src/ui/ui_state_text_fields_internal.h"
#include "../src/ui/ui_state_clipboard_internal.h"
#include "../src/ui/ui_state_navigation_internal.h"
#include "../src/ui/ui_navigation_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"state contract:%d: %s\n",__LINE__,#x); exit(1); } } while(0)
static bool write_clipboard(void *user,const char *text) { return user&&text; }
int main(void) {
  UIContext *a=ui_context_create(),*b=ui_context_create(); CHECK(a&&b);
  UIFrameState *af=ui_state_frame(a),*bf=ui_state_frame(b);
  CHECK(af!=bf&&ui_state_frame_read(a)==af&&af->canvas_width==1280);
  af->canvas_width=900; CHECK(bf->canvas_width==1280);
  int owner=7; UIClipboard clipboard={.write=write_clipboard};
  CHECK(ui_context_set_clipboard(a,&clipboard,&owner));
  CHECK(ui_context_set_current(a)); UIState *left=ui_button_state("same"); CHECK(left); left->is_pressed=true;
  UITextInputState *field=ui_text_field_state("same"); CHECK(field);
  char original[512]; memset(original,'x',sizeof(original)-1); original[sizeof(original)-1]=0;
  CHECK(ui_text_snapshot(field,original)); ui_text_edit_observe(field,original);
  CHECK(field->original_long&&field->history_count==1);
  CHECK(ui_context_set_current(b)); UIState *right=ui_button_state("same");
  CHECK(right&&right!=left&&!right->is_pressed);
  CHECK(ui_context_set_current(NULL));
  ui_nav_set(a,"left",true); ui_nav_set(b,"right",true);
  CHECK(!strcmp(ui_state_navigation(a)->nav_focus,"left"));
  CHECK(!strcmp(ui_state_navigation(b)->nav_focus,"right"));
  CHECK(ui_context_reset(a));
  CHECK(ui_state_frame(a)==af&&af->canvas_width==1280);
  CHECK(!ui_state_buttons(a)->ui_state_count&&!ui_state_text_fields(a)->text_input_state_count);
  CHECK(!ui_state_text_fields(a)->text_input_states[0].original_long);
  CHECK(!ui_state_navigation(a)->nav_focus[0]);
  CHECK(ui_state_clipboard(a)->clipboard.write==write_clipboard&&ui_state_clipboard(a)->clipboard_user==&owner);
  CHECK(!strcmp(ui_state_navigation(b)->nav_focus,"right")&&ui_state_buttons(b)->ui_state_count==1);
  CHECK(ui_context_destroy(a)&&ui_context_destroy(b)&&!ui_context_get_current());
  puts("UI state contract: PASS (opaque context, typed stores, same-ID isolation, stable views, reset/snapshot cleanup, clipboard ownership)");
}
