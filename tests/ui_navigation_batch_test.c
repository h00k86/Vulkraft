#define main navigation_fixture_main
#include "ui_navigation_test.c"
#undef main
#include "../src/ui/ui_state_navigation_internal.h"
#include "../src/ui/ui_state_frame_internal.h"
#include "../src/ui/ui_navigation_internal.h"
#ifdef UI_STORAGE_TEST_ALLOC
#include "ui_storage_alloc_probe.h"
#endif
static void mixed_widgets(void) {
  UIContext *c=context(); char a[64]="",b[64]="";
  begin(NULL,0); field("a",20,a); field("b",80,b); ui_end_frame();
  ui_text_input_focus("a");
  InputKeyEvent events[]={character('x'),key(PLATFORM_KEY_TAB,0),character('y'),
    key(PLATFORM_KEY_TAB,PLATFORM_MOD_SHIFT),character('z'),key(PLATFORM_KEY_ENTER,0),
    key(PLATFORM_KEY_TAB,0),character(0x20ac)};
  begin(events,sizeof(events)/sizeof(events[0]));
  field("a",20,a); field("b",80,b); ui_end_frame();
  CHECK(!strcmp(a,"xz")&&!strcmp(b,"y€")); focus("b");
  for(size_t i=0;i<sizeof(events)/sizeof(events[0]);++i) CHECK(!events[i].consumed);
  CHECK(ui_context_destroy(c));
}
static void full_batch(void) {
  UIContext *a=ui_context_create(),*b=ui_context_create(); CHECK(a&&b);
  UIFrameState *f=ui_state_frame(a); f->frame_active=true;
  char left[UI_ID_CAPACITY],right[UI_ID_CAPACITY];
  memset(left,'x',sizeof(left)-1); left[sizeof(left)-1]=0;
  strcpy(right,left); right[sizeof(right)-2]='y';
  ui_nav_begin(a,false,false);
  ui_nav_register(a,left,(UIRect){0,0,100,20},UI_NAV_TEXT,true,false);
  ui_nav_register(a,right,(UIRect){0,40,100,20},UI_NAV_TEXT,true,false); ui_nav_end(a);
  ui_nav_set(a,left,true);
  for(size_t i=0;i<INPUT_KEYBOARD_CAPACITY;++i) f->key_events[i]=i%2?character('x'):key(PLATFORM_KEY_TAB,0);
  f->key_event_count=INPUT_KEYBOARD_CAPACITY; ui_nav_begin(a,false,false);
  for(size_t i=0;i<INPUT_KEYBOARD_CAPACITY;++i) {
    const char *owner=(i/2)%2?left:right;
    CHECK(ui_nav_event(a,owner,i)); CHECK(!ui_nav_event(a,owner==left?right:left,i));
  }
  /* Unknown/unregistered focus is still copied, not borrowed or hashed. */
  f->key_events[0]=character('a'); f->key_event_count=1;
  char arbitrary[]="not-yet-submitted"; ui_nav_set(a,arbitrary,true); ui_nav_begin(a,false,false);
  arbitrary[0]='X'; ui_nav_set(a,"other",true);
  CHECK(ui_nav_event(a,"not-yet-submitted",0)&&!ui_nav_event(a,"other",0));
  ui_state_frame(b)->frame_active=true; ui_nav_set(b,"different-context",true);
  ui_state_frame(b)->key_events[0]=character('b'); ui_state_frame(b)->key_event_count=1;
  ui_nav_begin(b,false,false); CHECK(ui_nav_event(b,"different-context",0));
  CHECK(ui_nav_event(a,"not-yet-submitted",0));
  /* A pointer retarget affects only unconsumed events. */
  f->key_events[0].consumed=true; f->key_events[1]=character('c'); f->key_event_count=2;
  ui_nav_set(a,left,true); ui_nav_begin(a,false,false);
  f->g_mouse_clicked=true;
  ui_nav_register(a,"clicked",(UIRect){0,0,100,20},UI_NAV_TEXT,true,true);
  CHECK(ui_nav_event(a,left,0)&&ui_nav_event(a,"clicked",1));
  f->frame_active=ui_state_frame(b)->frame_active=false;
  CHECK(ui_context_destroy(a)&&ui_context_destroy(b));
}
#ifdef UI_STORAGE_TEST_ALLOC
static void allocation_failure(void) {
  UIContext *ctx=context(); char value[64]="";
  begin(NULL,0); field("edit",20,value); ui_end_frame();
  CHECK(!ui_state_navigation(ctx)->nav_event_owner&&!storage_live);
  ui_text_input_focus("edit"); InputKeyEvent one=character('a');
  UIFrameInput initial={.width=640,.height=480,.mouse_x=-20,.mouse_y=-20,.events=&one,.event_count=1};
  storage_fail_after=0; CHECK(!ui_begin_frame_input(&initial)); storage_fail_after=-1;
  CHECK(!ui_state_navigation(ctx)->nav_event_owner&&!ui_state_navigation(ctx)->nav_event_capacity&&!storage_live);
  begin(&one,1); field("edit",20,value); ui_end_frame();
  CHECK(ui_state_navigation(ctx)->nav_event_capacity==1&&storage_live==UI_ID_CAPACITY);
  UIFrameState frame_before=*ui_state_frame(ctx);
  UINavigationState nav_before=*ui_state_navigation(ctx);
  InputKeyEvent events[9]; for(int i=0;i<9;++i) events[i]=character('x');
  UIFrameInput input={.width=640,.height=480,.mouse_x=-20,.mouse_y=-20,.events=events,.event_count=9};
  storage_fail_after=0;
  CHECK(!ui_begin_frame_input(&input)); storage_fail_after=-1;
  CHECK(!memcmp(&frame_before,ui_state_frame(ctx),sizeof(frame_before)));
  CHECK(!memcmp(&nav_before,ui_state_navigation(ctx),sizeof(nav_before)));
  CHECK(!strcmp(value,"a")); for(int i=0;i<9;++i) CHECK(!events[i].consumed);
  void *old=ui_state_navigation(ctx)->nav_event_owner; storage_force_move=true;
  CHECK(ui_begin_frame_input(&input)); field("edit",20,value); ui_end_frame();
  CHECK(ui_state_navigation(ctx)->nav_event_owner!=old&&ui_state_navigation(ctx)->nav_event_capacity==16);
  CHECK(!strcmp(value,"axxxxxxxxx"));
  size_t calls=storage_requests; storage_fail_after=0;
  CHECK(ui_begin_frame_input(&input)); field("edit",20,value); ui_end_frame();
  CHECK(storage_requests==calls&&storage_fail_after==0); storage_fail_after=-1;
  CHECK(ui_context_reset(ctx)&&!storage_live&&!ui_state_navigation(ctx)->nav_event_owner);
  /* Losing focus ignores the batch, without allocating routing storage. */
  input.focus_lost=true; storage_fail_after=0;
  CHECK(ui_begin_frame_input(&input)); field("edit",20,value); ui_end_frame();
  CHECK(!storage_live&&storage_fail_after==0); storage_fail_after=-1;
  CHECK(ui_context_destroy(ctx));
}
#endif
int main(void) {
  mixed_widgets(); full_batch();
#ifdef UI_STORAGE_TEST_ALLOC
  CHECK(!storage_live); allocation_failure(); CHECK(!storage_live);
#endif
  puts("Navigation batches: PASS (ordered UTF-8/Tab/Enter, full batch, exact IDs, pointer retarget, contexts, OOM transaction/retry, warm reuse, cleanup)");
}
