#include "../src/ui/ui_state_text_fields_internal.h"
#include "../src/ui/ui_state_numeric_internal.h"
#include "../src/ui/ui_state_navigation_internal.h"
#include "../src/ui/ui_state_frame_internal.h"
/* Compiled consumer of the private navigation contract: no ui.c inclusion and
 * no current context or renderer. Detects accidental singleton dependencies. */
#include "../src/ui/ui_navigation_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"navigation module:%d: %s\n",__LINE__,#x); exit(1); } } while(0)
static InputKeyEvent key(int keycode) {
  return (InputKeyEvent){.kind=INPUT_KEY,.key=keycode,.action=PLATFORM_PRESS};
}
static void declare(UIContext *c,const char *first,const char *second) {
  ui_nav_register(c,first,(UIRect){0,0,100,20},UI_NAV_BUTTON,true,false);
  ui_nav_register(c,"disabled",(UIRect){0,20,100,20},UI_NAV_BUTTON,false,false);
  ui_nav_register(c,second,(UIRect){0,40,100,20},UI_NAV_TEXT,true,false);
}
int main(void) {
  UIContext *a=ui_context_create(),*b=ui_context_create();
  CHECK(a&&b&&!ui_context_get_current());
  ui_state_frame(a)->frame_active=ui_state_frame(b)->frame_active=true;
  ui_nav_begin(a,false,false); declare(a,"a-first","a-second"); ui_nav_end(a);
  ui_nav_begin(b,false,false); declare(b,"b-first","b-second"); ui_nav_end(b);
  CHECK(ui_state_navigation(a)->nav_previous_count==2&&ui_state_navigation(b)->nav_previous_count==2);

  /* Ordered ownership survives a Tab between two character events. */
  ui_nav_set(a,"a-first",true);
  ui_state_frame(a)->key_events[0]=(InputKeyEvent){.kind=INPUT_CHARACTER,.codepoint='x'};
  ui_state_frame(a)->key_events[1]=key(PLATFORM_KEY_TAB);
  ui_state_frame(a)->key_events[2]=(InputKeyEvent){.kind=INPUT_CHARACTER,.codepoint=0x20ac};
  ui_state_frame(a)->key_event_count=3;
  ui_nav_begin(a,false,false);
  CHECK(!strcmp(ui_state_navigation(a)->nav_focus,"a-second")&&ui_state_frame(a)->key_events[1].consumed);
  CHECK(ui_nav_event(a,"a-first",0)&&!ui_nav_event(a,"a-second",0));
  CHECK(ui_nav_event(a,"a-second",2)&&!ui_nav_event(a,"a-first",2));
  CHECK(ui_nav_has_events(a,"a-first")&&ui_nav_has_events(a,"a-second"));
  declare(a,"a-first","a-second"); ui_nav_end(a);

  /* Processing B must neither redirect A's events nor change its focus. */
  ui_nav_set(b,"b-first",true);
  ui_state_frame(b)->key_events[0]=key(PLATFORM_KEY_ENTER); ui_state_frame(b)->key_event_count=1;
  ui_nav_begin(b,false,false); declare(b,"b-first","b-second");
  CHECK(ui_nav_activate(b,"b-first")&&!ui_nav_activate(b,"b-first"));
  ui_nav_end(b);
  CHECK(!strcmp(ui_state_navigation(a)->nav_focus,"a-second")&&!ui_state_frame(a)->key_events[2].consumed);
  CHECK(!strcmp(ui_state_navigation(b)->nav_focus,"b-first")&&!ui_context_get_current());

  /* The caller supplies menu capture; navigation leaves Tab available. */
  ui_state_frame(a)->key_events[0]=key(PLATFORM_KEY_TAB); ui_state_frame(a)->key_event_count=1;
  ui_nav_begin(a,false,true); declare(a,"a-first","a-second"); ui_nav_end(a);
  CHECK(!ui_state_frame(a)->key_events[0].consumed&&!strcmp(ui_state_navigation(a)->nav_focus,"a-second"));

  /* Focus cancellation releases only the requested context's editor snapshot. */
  ui_state_text_fields(a)->text_input_state_count=ui_state_text_fields(b)->text_input_state_count=1;
  ui_state_text_fields(a)->text_input_states[0].focused=ui_state_text_fields(b)->text_input_states[0].focused=true;
  ui_state_text_fields(a)->text_input_states[0].original_long=malloc(16);
  ui_state_text_fields(b)->text_input_states[0].original_long=malloc(16);
  CHECK(ui_state_text_fields(a)->text_input_states[0].original_long&&ui_state_text_fields(b)->text_input_states[0].original_long);
  ui_state_numeric(a)->float_field_state_count=1; ui_state_numeric(a)->float_field_states[0].focused=true;
  ui_nav_clear_editor_focus(a);
  CHECK(!ui_state_text_fields(a)->text_input_states[0].focused&&!ui_state_text_fields(a)->text_input_states[0].original_long);
  CHECK(!ui_state_numeric(a)->float_field_states[0].focused&&ui_state_text_fields(b)->text_input_states[0].focused);
  CHECK(ui_state_text_fields(b)->text_input_states[0].original_long);
  ui_state_frame(a)->key_event_count=0;
  ui_nav_begin(a,true,false); declare(a,"a-first","a-second"); ui_nav_end(a);
  CHECK(!ui_state_navigation(a)->nav_focus[0]&&!ui_state_navigation(a)->nav_enabled&&!ui_state_navigation(a)->nav_keyboard);
  CHECK(!strcmp(ui_state_navigation(b)->nav_focus,"b-first"));

  ui_state_frame(a)->frame_active=ui_state_frame(b)->frame_active=false;
  CHECK(ui_context_destroy(a)&&ui_context_destroy(b));
  puts("Navigation module: PASS (explicit contexts, ordered ownership, activation, menu capture, editor cleanup, cancellation; no current context/renderer)");
  return 0;
}
