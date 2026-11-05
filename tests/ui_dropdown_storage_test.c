/* Linked-core lifetime and failure tests; no ui.c implementation inclusion. */
#define main navigation_fixture_main
#include "ui_navigation_test.c"
#undef main
#include "../src/ui/ui_state_dropdowns_internal.h"
#include "../src/ui/ui_state_frame_internal.h"

static size_t requests,live_bytes;
static int fail_after=-1;
typedef struct { void *pointer; size_t bytes; } Allocation;
static Allocation owned[64];
void *__real_realloc(void *,size_t);
void __real_free(void *);
void *__wrap_realloc(void *pointer,size_t bytes) {
  ++requests;
  if(fail_after>=0 && fail_after--==0) return NULL;
  size_t slot=64;
  if(pointer) {
    for(size_t i=0;i<64;++i) if(owned[i].pointer==pointer) { slot=i; break; }
    CHECK(slot<64);
  } else {
    for(size_t i=0;i<64;++i) if(!owned[i].pointer) { slot=i; break; }
    CHECK(slot<64);
  }
  size_t previous=owned[slot].bytes;
  void *next=__real_realloc(pointer,bytes);
  if(next) { owned[slot]=(Allocation){next,bytes}; live_bytes=live_bytes-previous+bytes; }
  return next;
}
void __wrap_free(void *pointer) {
  if(pointer) for(size_t i=0;i<64;++i) if(owned[i].pointer==pointer) {
    live_bytes-=owned[i].bytes; owned[i]=(Allocation){0}; break;
  }
  __real_free(pointer);
}
static UIDropdownState *state(UIContext *ctx,const char *id) {
  UIDropdownsState *store=ui_state_dropdowns(ctx);
  for(int i=0;i<store->dropdown_state_count;++i)
    if(!strcmp(store->dropdown_states[i].id,id)) return &store->dropdown_states[i];
  return NULL;
}
static UIContext *make_context(void) {
  UIContext *ctx=ui_context_create(); CHECK(ctx&&ui_context_set_current(ctx));
  CHECK(ui_context_set_renderer(ctx,&renderer,NULL)); return ctx;
}
static void frame(float height,float x,float y,bool down,const InputKeyEvent *event) {
  UIFrameInput input={.width=640,.height=height,.mouse_x=x,.mouse_y=y,
    .mouse_down=down,.delta_time=.016f,.events=event,.event_count=event?1:0};
  CHECK(ui_begin_frame_input(&input));
}
static void draw(const char *id,const char *const *options,int count,int *selected) {
  CHECK(!ui_dropdown(id,font,ANCHOR_TOP_LEFT,0,14,20,20,200,options,count,selected,NULL,NULL));
}
static void end(void) { ui_draw_dropdown_popups(); ui_end_frame(); }
static void copies_and_reuse(void) {
  UIContext *a=make_context(); int selected=0;
  char first[32]="Caffè €",second[32]="Seconda";
  const char *options[]={first,second};
  size_t calls=requests;
  frame(480,-20,-20,false,NULL); draw("choice",options,2,&selected); end();
  UIDropdownState *left=state(a,"choice"); CHECK(left&&!left->popupOptions&&requests==calls);
  ui_dropdown_set_open("choice",true); CHECK(!left->popupOptions);
  frame(480,-20,-20,false,NULL); draw("choice",options,2,&selected);
  CHECK(left->popupOptions&&left->popupOptionCapacity==8&&left->popupSubmitted);
  strcpy(first,"changed"); strcpy(second,"mutated");
  CHECK(!strcmp(left->popupOptions[0],"Caffè €")&&!strcmp(left->popupOptions[1],"Seconda"));
  end();
  void *buffer=left->popupOptions; calls=requests;
  ui_dropdown_set_open("choice",false);
  frame(480,-20,-20,false,NULL); draw("choice",options,2,&selected); end();
  ui_dropdown_set_open("choice",true);
  fail_after=0;
  frame(480,-20,-20,false,NULL); draw("choice",options,2,&selected); end();
  CHECK(requests==calls&&fail_after==0&&left->popupOptions==buffer); fail_after=-1;
  UIContext *b=make_context();
  ui_dropdown_set_open("choice",true);
  frame(480,-20,-20,false,NULL); draw("choice",options,2,&selected); end();
  UIDropdownState *right=state(b,"choice");
  CHECK(right&&right!=left&&right->popupOptions!=left->popupOptions);
  size_t other_bytes=(size_t)right->popupOptionCapacity*sizeof(*right->popupOptions);
  CHECK(ui_context_reset(a)&&live_bytes==other_bytes);
  CHECK(ui_state_dropdowns(a)->dropdown_state_count==0&&!ui_state_dropdowns(a)->dropdown_states[0].popupOptions);
  CHECK(!strcmp(right->popupOptions[0],"changed"));
  CHECK(ui_context_destroy(a)&&ui_context_destroy(b)&&!live_bytes);
}
static void growth_and_failure(void) {
  UIContext *ctx=make_context(); int selected=0;
  const char *options[128]; for(int i=0;i<128;++i) options[i]="Voce";
  ui_dropdown_set_open("grow",true);
  frame(160,-20,-20,false,NULL); draw("grow",options,128,&selected); end();
  UIDropdownState *s=state(ctx,"grow"); CHECK(s&&s->popupOptionCapacity==8);
  void *old=s->popupOptions; size_t bytes=live_bytes;
  fail_after=0;
  frame(4096,-20,-20,false,NULL); draw("grow",options,128,&selected);
  CHECK(!s->open&&!s->popupSubmitted&&!s->popupOpenRequested&&!s->lastListH);
  CHECK(s->popupOptions==old&&s->popupOptionCapacity==8&&live_bytes==bytes&&selected==0);
  CHECK(!ui_state_dropdowns(ctx)->g_dropdown_click_owner); end(); fail_after=-1;
  ui_dropdown_set_open("grow",true);
  frame(4096,-20,-20,false,NULL); draw("grow",options,128,&selected); end();
  CHECK(s->open&&s->popupOptionCapacity==128&&live_bytes==128*128);
  CHECK(s->popupVisibleRows==128&&!strcmp(s->popupOptions[127],"Voce"));
  size_t calls=requests;
  frame(160,-20,-20,false,NULL); draw("grow",options,128,&selected); end();
  CHECK(requests==calls&&s->popupOptionCapacity==128);
  CHECK(ui_context_destroy(ctx)&&!live_bytes);
}
static void cache_reuse(void) {
  UIContext *ctx=make_context(); const char *options[]={"Uno","Due"}; int selected=0;
  for(int i=0;i<MAX_UI_DROPDOWNS;++i) {
    char id[32]; snprintf(id,sizeof(id),"cached-%d",i);
    ui_dropdown_set_open(id,true);
    frame(480,-20,-20,false,NULL); draw(id,options,2,&selected); end();
    ui_dropdown_set_open(id,false);
    frame(480,-20,-20,false,NULL); end();
  }
  CHECK(ui_state_dropdowns(ctx)->dropdown_state_count==MAX_UI_DROPDOWNS);
  size_t bytes=live_bytes,calls=requests;
  UIDropdownState *first=&ui_state_dropdowns(ctx)->dropdown_states[0];
  void *allocation=first->popupOptions;
  ui_dropdown_set_open("replacement",true);
  CHECK(state(ctx,"replacement")==first&&first->popupOptions==allocation);
  CHECK(!state(ctx,"cached-0"));
  fail_after=0;
  frame(480,-20,-20,false,NULL); draw("replacement",options,2,&selected); end();
  CHECK(requests==calls&&fail_after==0&&live_bytes==bytes); fail_after=-1;
  CHECK(ui_context_destroy(ctx)&&!live_bytes);
}
static void input_failure(void) {
  UIContext *ctx=make_context(); const char *options[]={"Uno","Due"}; int selected=0;
  frame(480,-20,-20,false,NULL); draw("mouse",options,2,&selected); end();
  fail_after=0;
  frame(480,30,30,true,NULL); draw("mouse",options,2,&selected);
  CHECK(!ui_dropdown_is_open("mouse")&&!state(ctx,"mouse")->popupOptions);
  CHECK(ui_state_frame(ctx)->g_click_claimed_by_dropdown_list);
  CHECK(!button("under",20,false)); /* Failed opening press cannot fall through. */
  end(); fail_after=-1;
  frame(480,30,30,false,NULL); draw("mouse",options,2,&selected);
  CHECK(!button("under",20,false)); end();
  frame(480,30,30,true,NULL); draw("mouse",options,2,&selected); end();
  CHECK(ui_dropdown_is_open("mouse")&&state(ctx,"mouse")->popupOptions);
  frame(480,30,30,false,NULL); draw("mouse",options,2,&selected); end();
  CHECK(ui_context_reset(ctx)&&!live_bytes);
  frame(480,-20,-20,false,NULL); draw("key",options,2,&selected); end();
  InputKeyEvent tab=key(PLATFORM_KEY_TAB,0),enter=key(PLATFORM_KEY_ENTER,0);
  frame(480,-20,-20,false,&tab); draw("key",options,2,&selected); end();
  fail_after=0;
  frame(480,-20,-20,false,&enter); draw("key",options,2,&selected); end();
  CHECK(!ui_dropdown_is_open("key")&&!state(ctx,"key")->popupOptions&&selected==0); fail_after=-1;
  frame(480,-20,-20,false,&enter); draw("key",options,2,&selected); end();
  CHECK(ui_dropdown_is_open("key")&&state(ctx,"key")->popupOptions);
  CHECK(ui_context_destroy(ctx)&&!live_bytes);
}
int main(void) {
  copies_and_reuse(); growth_and_failure(); cache_reuse(); input_failure();
  CHECK(!clips&&!ui_context_get_current());
  puts("Dropdown storage: PASS (lazy copies, UTF-8, warm reuse, growth/OOM, input recovery, stable cache, context isolation, cleanup)");
}
