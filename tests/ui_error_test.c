#define main navigation_fixture_main
#include "ui_navigation_test.c"
#undef main
#include "ui_storage_alloc_probe.h"
#include "../src/ui/ui_state_frame_internal.h"
#include "../src/ui/ui_state_navigation_internal.h"
static void expected(UIContext *ctx,UIErrorCode code,UIErrorOperation operation,const char *id) {
  UIErrorInfo error=ui_context_error(ctx);
  CHECK(error.code==code&&error.operation==operation&&!strcmp(error.widget_id,id));
}
int main(void) {
  CHECK(ui_begin_frame_input_status(NULL)==UI_ERROR_STATE);
  CHECK(ui_context_error(NULL).code==UI_ERROR_ARGUMENT);
  ui_context_clear_error(NULL);
  UIContext *a=context(),*b=ui_context_create(); CHECK(b);
  UIFrameInput input={.width=640,.height=480,.mouse_x=-20,.mouse_y=-20};
  CHECK(ui_begin_frame_input_status(NULL)==UI_ERROR_ARGUMENT);
  input.width=NAN; CHECK(ui_begin_frame_input_status(&input)==UI_ERROR_ARGUMENT); input.width=640;
  input.event_count=INPUT_KEYBOARD_CAPACITY+1;
  CHECK(ui_begin_frame_input_status(&input)==UI_ERROR_LIMIT);
  input.event_count=1; CHECK(ui_begin_frame_input_status(&input)==UI_ERROR_ARGUMENT);
  InputKeyEvent event=character('x'); input.events=&event;
  UIFrameState before=*ui_state_frame(a); UINavigationState nav=*ui_state_navigation(a);
  storage_fail_after=0; CHECK(ui_begin_frame_input_status(&input)==UI_ERROR_MEMORY); storage_fail_after=-1;
  CHECK(!memcmp(&before,ui_state_frame(a),sizeof(before))&&!memcmp(&nav,ui_state_navigation(a),sizeof(nav)));
  CHECK(!event.consumed); expected(a,UI_ERROR_NONE,UI_OPERATION_NONE,"");
  CHECK(ui_begin_frame_input_status(&input)==UI_ERROR_NONE);
  CHECK(ui_begin_frame_input_status(&input)==UI_ERROR_STATE);
  CHECK(!ui_context_reset(a)); expected(a,UI_ERROR_STATE,UI_OPERATION_CONTEXT,"");
  ui_end_frame(); ui_context_clear_error(a);
  UIMenuItem items[33]; for(int i=0;i<33;++i) items[i]=(UIMenuItem){.id="leaf",.label="Caffè €"};
  begin(NULL,0); char id[]="memory-root";
  storage_fail_after=0; ui_menu_open_context(id,50,50,200,items,1); storage_fail_after=-1;
  id[0]='X'; expected(a,UI_ERROR_MEMORY,UI_OPERATION_MENU_ROOT,"memory-root");
  CHECK(!ui_menu_submit_submenu(NULL,"leaf","child",items,1));
  expected(a,UI_ERROR_MEMORY,UI_OPERATION_MENU_ROOT,"memory-root"); /* First error survives. */
  expected(b,UI_ERROR_NONE,UI_OPERATION_NONE,"");
  ui_context_clear_error(a);
  ui_menu_open_context("too-many",50,50,200,items,33);
  expected(a,UI_ERROR_LIMIT,UI_OPERATION_MENU_ROOT,"too-many");
  ui_context_clear_error(a);
  CHECK(!ui_menu_submit_submenu(NULL,"leaf","child",items,1));
  expected(a,UI_ERROR_ARGUMENT,UI_OPERATION_MENU_SUBMIT,"child");
  ui_menu_close_hierarchy(); ui_context_clear_error(a);
  const char *options[]={"one","two"}; int selected=0;
  ui_dropdown_set_open("oom-choice",true); storage_fail_after=0;
  ui_dropdown("oom-choice",font,ANCHOR_TOP_LEFT,0,14,20,100,200,options,2,&selected,NULL,NULL);
  storage_fail_after=-1;
  expected(a,UI_ERROR_MEMORY,UI_OPERATION_DROPDOWN,"oom-choice");
  CHECK(!ui_dropdown_is_open("oom-choice")&&selected==0);
  ui_context_clear_error(a); CHECK(!ui_push_id(""));
  expected(a,UI_ERROR_ARGUMENT,UI_OPERATION_ID_STACK,"");
  ui_context_clear_error(a);
  for(int i=0;i<16;++i) CHECK(ui_push_id("scope"));
  CHECK(!ui_push_id("overflow")); expected(a,UI_ERROR_LIMIT,UI_OPERATION_ID_STACK,"overflow");
  for(int i=0;i<16;++i) ui_pop_id();
  ui_end_frame(); CHECK(ui_context_reset(a)); expected(a,UI_ERROR_NONE,UI_OPERATION_NONE,"");
  CHECK(!strcmp(ui_error_code_name(UI_ERROR_MEMORY),"out of memory"));
  CHECK(!strcmp(ui_error_code_name((UIErrorCode)99),"unknown error"));
  CHECK(ui_context_destroy(a)&&ui_context_destroy(b)&&!storage_live&&!clips);
  puts("UI diagnostics: PASS (typed status, OOM transaction, first error, copied ID, independent contexts, reset)");
}
