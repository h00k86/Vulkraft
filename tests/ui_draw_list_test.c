#define main renderer_regression_main
#include "ui_renderer_test.c"
#undef main
static int fail_after=-1;
static size_t allocation_calls;
static void *force_move_pointer;
static size_t force_move_bytes;
static bool moved_arena;
static bool should_fail(void) { ++allocation_calls; return fail_after>=0 && fail_after--==0; }
static void *list_calloc(size_t n,size_t size) { return should_fail()?NULL:calloc(n,size); }
static void *list_realloc(void *p,size_t n) {
  if (should_fail()) return NULL;
  if (p && p==force_move_pointer) {
    void *next=malloc(n);
    if (!next) return NULL;
    memcpy(next,p,force_move_bytes<n?force_move_bytes:n);
    free(p); force_move_pointer=NULL; moved_arena=true;
    return next;
  }
  return realloc(p,n);
}
#define calloc list_calloc
#define realloc list_realloc
#include "../src/ui/ui_draw_list.c"
#undef calloc
#undef realloc
static int order[8], order_count;
static void ordered_quad(void *user,float x,float y,float w,float h,float r,float g,float b) {
  Recorder *rec=user;
  order[order_count++]=rec->depth;
}
static bool reject_clip(void *user,float x,float y,float w,float h) { return false; }
static char expected_text[200][96];
static size_t replayed_text;
static void check_text_copy(void *user,Font *font,const char *text,float x,float y,float scale,float r,float g,float b) {
  assert(replayed_text<200);
  assert(!strcmp(text,expected_text[replayed_text++]));
}
static void record_text_batch(UIDrawList *list,Font *font) {
  const UIRenderer *r=ui_draw_list_renderer();
  for (int i=0;i<200;++i) {
    char local[96];
    snprintf(local,sizeof(local),"%03d Caffè € etichetta UTF-8",i);
    if (!i) local[0]=0; /* Include empty strings and their terminator. */
    strcpy(expected_text[i],local);
    r->draw_text_colored(list,font,local,0,0,1,1,1,1);
    memset(local,'x',sizeof(local)); /* Recording must own a copy. */
    if (!i && !moved_arena) {
      force_move_pointer=list->text;
      force_move_bytes=list->text_capacity;
    }
  }
}
static void check_text_arena(void) {
  Recorder rec={0}; UIRenderer backend=recording_backend();
  backend.draw_text_colored=check_text_copy;
  UIDrawList *list=ui_draw_list_create(&backend,&rec);
  Font font={.ascender=1};
  assert(list && ui_draw_list_begin(list));
  record_text_batch(list,&font);
  assert(moved_arena && ui_draw_list_count(list)==200);
  size_t capacity=list->text_capacity;
  replayed_text=0;
  assert(ui_draw_list_replay(list) && replayed_text==200);
  assert(!list->text_bytes && list->text_capacity==capacity);
  size_t calls=allocation_calls;
  fail_after=0; /* No allocator calls allowed in warmed frames. */
  for (int i=0;i<3;++i) {
    assert(ui_draw_list_begin(list)); record_text_batch(list,&font);
    if (i==1) assert(ui_draw_list_discard(list));
    else { replayed_text=0; assert(ui_draw_list_replay(list) && replayed_text==200); }
    assert(!list->text_bytes && list->text_capacity==capacity);
  }
  assert(allocation_calls==calls && fail_after==0);
  fail_after=-1;
  assert(ui_draw_list_begin(list));
  const UIRenderer *r=ui_draw_list_renderer();
  r->draw_text_colored(list,&font,"preserved",0,0,1,1,1,1);
  char *previous=list->text;
  size_t bytes=list->text_bytes;
  char *large=malloc(capacity+1); assert(large);
  memset(large,'L',capacity); large[capacity]=0;
  fail_after=0;
  r->draw_text_colored(list,&font,large,0,0,1,1,1,1);
  fail_after=-1; free(large);
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_MEMORY);
  assert(list->text==previous && list->text_bytes==bytes && !strcmp(list->text,"preserved"));
  assert(!ui_draw_list_replay(list));
  assert(ui_draw_list_discard(list) && ui_draw_list_error(list)==UI_DRAW_LIST_MEMORY);
  assert(ui_draw_list_begin(list));
  record_text_batch(list,&font); replayed_text=0;
  assert(ui_draw_list_replay(list) && replayed_text==200);
  ui_draw_list_destroy(list);
}
static void check_text_limit(void) {
  Recorder rec={0}; UIRenderer backend=recording_backend();
  UIDrawList *list=ui_draw_list_create(&backend,&rec);
  const UIRenderer *r=ui_draw_list_renderer(); Font font={.ascender=1};
  char *large=malloc(UI_LIST_MAX_TEXT_BYTES+1); assert(large);
  memset(large,'L',UI_LIST_MAX_TEXT_BYTES); large[UI_LIST_MAX_TEXT_BYTES]=0;
  assert(list && ui_draw_list_begin(list));
  r->draw_quad(list,0,0,1,1,1,1,1); /* Reserve commands before checking text allocations. */
  size_t calls=allocation_calls;
  r->draw_text_colored(list,&font,large,0,0,1,1,1,1);
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_LIMIT && allocation_calls==calls);
  assert(!list->text_bytes && !list->text_capacity && !ui_draw_list_replay(list));
  assert(ui_draw_list_begin(list));
  large[UI_LIST_MAX_TEXT_BYTES-1]=0;
  r->draw_text_colored(list,&font,large,0,0,1,1,1,1);
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_OK && list->text_bytes==UI_LIST_MAX_TEXT_BYTES);
  assert(list->text_capacity==UI_LIST_MAX_TEXT_BYTES);
  assert(ui_draw_list_replay(list) && rec.calls[2]==1);
  assert(ui_draw_list_begin(list));
  r->draw_text_colored(list,&font,large,0,0,1,1,1,1);
  r->draw_text_colored(list,&font,"",0,0,1,1,1,1);
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_LIMIT && !ui_draw_list_replay(list));
  assert(ui_draw_list_discard(list) && ui_draw_list_begin(list));
  r->draw_text_colored(list,&font,"recovered",0,0,1,1,1,1);
  assert(ui_draw_list_replay(list) && !strcmp(rec.label,"recovered"));
  free(large); ui_draw_list_destroy(list);
}
static UIDrawList *discard_during_replay;
static void try_discard_quad(void *user,float x,float y,float w,float h,float r,float g,float b) {
  assert(!ui_draw_list_discard(discard_during_replay));
  ((Recorder *)user)->calls[0]++;
}
static void check_discard(void) {
  Recorder rec={.depth=1}; UIRenderer backend=recording_backend();
  UIDrawList *list=ui_draw_list_create(&backend,&rec);
  const UIRenderer *r=ui_draw_list_renderer(); Font font={.ascender=1};
  assert(!ui_draw_list_discard(NULL));
  assert(list && ui_draw_list_begin(list));
  r->draw_text_colored(list,&font,"owned text",0,0,1,1,1,1);
  r->draw_textured_quad(list,0,0,10,10,7,0,0,1,1);
  r->texture_manager_preview_clear(list);
  assert(r->text_clip_push(list,0,0,10,10));
  assert(!ui_draw_list_replay(list)); /* Unbalanced recorded clip. */
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_CLIP && rec.clears==0);
  size_t capacity=list->capacity;
  assert(ui_draw_list_discard(list));
  assert(ui_draw_list_count(list)==0 && !list->text_bytes && list->capacity==capacity);
  assert(!list->recording && !list->depth && rec.depth==1);
  assert(rec.calls[2]==0 && rec.calls[3]==0 && rec.clears==1);
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_CLIP);
  assert(ui_draw_list_discard(list) && rec.clears==1);
  assert(ui_draw_list_begin(list) && ui_draw_list_error(list)==UI_DRAW_LIST_OK);
  assert(r->text_clip_depth(list)==1);
  r->draw_quad(list,0,0,10,10,1,1,1);
  list->target.draw_quad=try_discard_quad;
  discard_during_replay=list;
  assert(ui_draw_list_replay(list) && rec.calls[0]==1);
  assert(ui_draw_list_discard(list) && ui_draw_list_discard(list));
  assert(!ui_draw_list_replay(list) && ui_draw_list_error(list)==UI_DRAW_LIST_STATE);
  assert(ui_draw_list_begin(list));
  r->draw_textured_quad(list,0,0,10,10,7,0,0,1,1);
  r->texture_manager_preview_clear(list);
  assert(ui_draw_list_replay(list) && rec.clears==1);
  assert(ui_draw_list_discard(list) && rec.clears==2);
  ui_draw_list_destroy(list);
  assert(rec.clears==2 && rec.depth==1);
}
static void check_optional_skin(void) {
  for (int omitted=1;omitted<=3;++omitted) {
    UIRenderer backend=recording_backend();
    if (omitted&1) backend.draw_hud_paper_panel=NULL;
    if (omitted&2) backend.draw_hud_trash_swipe=NULL;
    Recorder immediate={0}, deferred={0};
    UIContext *ctx=ui_context_create();
    assert(ctx && ui_context_set_current(ctx));
    assert(ui_context_set_renderer(ctx,&backend,&immediate));
    const float fill[4]={.1f,.2f,.3f,.4f}, accent[3]={.8f,.7f,.6f};
    ui_paint_draw_hud_paper_panel(1,2,30,1,fill,accent,true,42);
    ui_paint_draw_hud_trash_swipe(1,2,30,10,false,42,-1);
    ui_paint_draw_hud_trash_swipe(1,2,30,10,true,42,7);
    assert(immediate.calls[4]==((omitted&1)?0:1));
    assert(immediate.calls[5]==((omitted&2)?0:2));
    assert(immediate.calls[0]==((omitted&1)?1:0));
    assert(immediate.calls[1]==((omitted&1)?1:0)+((omitted&2)?1:0));
    assert(immediate.calls[3]==((omitted&2)?1:0));
    UIDrawList *list=ui_draw_list_create(&backend,&deferred);
    assert(list && ui_draw_list_begin(list));
    assert(ui_context_set_renderer(ctx,ui_draw_list_renderer(),list));
    ui_paint_draw_hud_paper_panel(1,2,30,1,fill,accent,true,42);
    ui_paint_draw_hud_trash_swipe(1,2,30,10,false,42,-1);
    ui_paint_draw_hud_trash_swipe(1,2,30,10,true,42,7);
    for (int i=0;i<6;++i) assert(deferred.calls[i]==0);
    assert(ui_draw_list_count(list)==3 && ui_draw_list_replay(list));
    for (int i=0;i<6;++i) assert(immediate.calls[i]==deferred.calls[i]);
    assert(immediate.alpha==deferred.alpha);
    assert(ui_context_destroy(ctx));
    ui_draw_list_destroy(list);
  }
}
static void check_optional_previews(void) {
  UIContext *ctx=ui_context_create();
  assert(ctx && ui_context_set_current(ctx));
  Recorder rec={0};
  for (int mask=0;mask<8;++mask) {
    UIRenderer backend=recording_backend();
    if (!(mask&1)) backend.texture_manager_get_size=NULL;
    if (!(mask&2)) backend.texture_manager_preview_load=NULL;
    if (!(mask&4)) backend.texture_manager_preview_clear=NULL;
    bool valid=mask==0 || mask==1 || mask==7;
    assert(ui_context_set_renderer(ctx,&backend,&rec)==valid);
    UIDrawList *list=ui_draw_list_create(&backend,&rec);
    assert((list!=NULL)==valid);
    ui_draw_list_destroy(list);
  }
  UIRenderer backend=recording_backend();
  backend.texture_manager_get_size=NULL;
  backend.texture_manager_preview_load=NULL;
  backend.texture_manager_preview_clear=NULL;
  assert(ui_context_set_renderer(ctx,&backend,&rec));
  int w=17,h=19;
  assert(!ui_paint_texture_manager_get_size(7,&w,&h) && w==17 && h==19);
  assert(ui_paint_texture_manager_preview_load("unsupported.png")==-1);
  UIDrawList *list=ui_draw_list_create(&backend,&rec);
  const UIRenderer *r=ui_draw_list_renderer();
  assert(list && ui_draw_list_begin(list));
  assert(!r->texture_manager_get_size(list,7,&w,&h) && w==17 && h==19);
  r->draw_textured_quad(list,0,0,10,10,7,0,0,1,1);
  assert(r->texture_manager_preview_load(list,"unsupported.png")==-1);
  r->texture_manager_preview_clear(list);
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_OK && !list->pending_clear);
  assert(ui_draw_list_replay(list) && rec.calls[3]==1);
  assert(r->texture_manager_preview_load(list,"unsupported.png")==-1);
  r->texture_manager_preview_clear(list);
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_OK && !list->pending_clear);
  assert(ui_context_destroy(ctx));
  ui_draw_list_destroy(list);
  assert(rec.clears==0);
}
static void check_deferred_scopes(void) {
  Recorder rec={0}; UIRenderer backend=recording_backend();
  UIDrawList *list=ui_draw_list_create(&backend,&rec);
  UIContext *ctx=ui_context_create();
  assert(list && ctx && ui_context_set_current(ctx));
  assert(ui_context_set_renderer(ctx,ui_draw_list_renderer(),list));
  assert(ui_draw_list_begin(list) && ui_clip_push((UIRect){0,0,100,100}));
  Font font={.ascender=1};
  UIPanel panel=ui_panel_begin("empty","",&font,(UIRect){0,0,0,0},1);
  UIScrollScope scroll=ui_scroll_begin("empty-scroll",(UIRect){0,0,0,0},1);
  assert(!panel.clip_pushed && !scroll.clip_pushed);
  ui_scroll_end(&scroll); ui_panel_end(&panel);
  ui_scroll_end(&scroll); ui_panel_end(&panel);
  assert(ui_clip_depth()==1 && ui_clip_pop() && ui_clip_depth()==0);
  assert(ui_draw_list_replay(list) && rec.depth==0 && rec.calls[8]==1);
  assert(ui_context_destroy(ctx)); ui_draw_list_destroy(list);
}
static void check_first_error(void) {
  Recorder rec={0}; UIRenderer backend=recording_backend();
  UIDrawList *list=ui_draw_list_create(&backend,&rec);
  const UIRenderer *r=ui_draw_list_renderer();
  Font font={.ascender=1};
  assert(list && ui_draw_list_begin(list));
  /* Reserve commands first, then fail the owned text allocation. */
  r->draw_quad(list,0,0,1,1,1,1,1);
  fail_after=0;
  r->draw_text_colored(list,&font,"allocation failure",0,0,1,1,1,1);
  fail_after=-1;
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_MEMORY);
  size_t count=ui_draw_list_count(list);
  assert(!r->text_clip_push(list,0,0,-1,1) && !r->text_clip_pop(list));
  assert(r->texture_manager_preview_load(list,"image.png")==-1);
  r->draw_quad(list,0,0,1,1,1,1,1);
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_MEMORY);
  assert(ui_draw_list_count(list)==count && !ui_draw_list_replay(list));
  assert(rec.calls[11]==0 && rec.calls[0]==0);
  /* Cleanup must still reach the backend on a failed frame. */
  r->texture_manager_preview_clear(list); assert(rec.clears==1);
  assert(ui_draw_list_begin(list) && ui_draw_list_error(list)==UI_DRAW_LIST_OK);
  assert(r->texture_manager_preview_load(list,"image.png")==7);
  assert(rec.calls[11]==1);
  r->draw_textured_quad(list,0,0,10,10,7,0,0,1,1);
  assert(r->texture_manager_preview_load(list,"image.png")==-1);
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_RESOURCE);
  assert(!r->text_clip_pop(list));
  assert(!r->text_clip_push(list,0,0,NAN,1));
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_RESOURCE && rec.calls[11]==1);
  r->texture_manager_preview_clear(list);
  assert(list->pending_clear && rec.clears==1);
  assert(!ui_draw_list_replay(list));
  assert(ui_draw_list_begin(list) && rec.clears==2 && !list->pending_clear);
  assert(!r->text_clip_pop(list));
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_CLIP);
  assert(r->texture_manager_preview_load(list,"image.png")==-1 && rec.calls[11]==1);
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_CLIP);
  assert(ui_draw_list_begin(list));
  r->draw_quad(list,0,0,1,1,1,1,1);
  assert(ui_draw_list_replay(list) && rec.calls[0]==1);
  ui_draw_list_destroy(list);
}
int main(void) {
  check_text_arena();
  check_text_limit();
  check_discard();
  check_first_error();
  check_deferred_scopes();
  check_optional_skin();
  check_optional_previews();
  Recorder rec={0}; UIRenderer backend=recording_backend();
  UIRenderer incomplete=backend; incomplete.font_ascent=NULL;
  assert(!ui_draw_list_create(&incomplete,&rec));
  fail_after=0; assert(!ui_draw_list_create(&backend,&rec)); fail_after=-1;
  UIDrawList *list=ui_draw_list_create(&backend,&rec); assert(list);
  const UIRenderer *r=ui_draw_list_renderer();
  assert(!ui_draw_list_replay(list));
  assert(ui_draw_list_begin(list));
  UIContext *ctx=ui_context_create(); assert(ctx && ui_context_set_current(ctx));
  assert(ui_context_set_renderer(ctx,r,list));
  UIFrameInput frame={.width=640,.height=400}; assert(ui_begin_frame_input(&frame));
  Font font={.ascender=1}; char label[32]="Caffè";
  assert(r->font_ascent(list,&font,14)==14);
  assert(rec.calls[13]==1 && ui_draw_list_count(list)==0);
  ui_button_labeled("button",label,&font,ANCHOR_TOP_LEFT,0,14,0,0,NULL);
  ui_end_frame();
  assert(rec.calls[0]==0 && rec.calls[2]==0 && rec.calls[6]>0);
  strcpy(label,"changed");
  assert(ui_draw_list_count(list)>0 && ui_draw_list_replay(list));
  assert(!strcmp(rec.label,"Caffè") && rec.calls[0]>0);
  assert(!ui_draw_list_replay(list));
  assert(ui_draw_list_begin(list));
  float fill[4]={1,2,3,4}, accent[3]={5,6,7};
  r->draw_hud_paper_panel(list,0,0,1,1,fill,accent,false,10);
  fill[0]=0; accent[0]=0;
  assert(list->commands[0].data.draw_hud_paper_panel.fill[0]==1);
  assert(list->commands[0].data.draw_hud_paper_panel.accent[0]==5);
  assert(ui_draw_list_begin(list)); /* Explicit discard, no output. */
  assert(rec.calls[4]==0);
  list->target.draw_quad=ordered_quad;
  r->draw_quad(list,0,0,1,1,1,1,1);
  assert(r->text_clip_push(list,0,0,10,10));
  r->draw_quad(list,0,0,1,1,1,1,1);
  assert(r->text_clip_pop(list));
  r->draw_quad(list,0,0,1,1,1,1,1);
  assert(ui_draw_list_replay(list));
  assert(order_count==3 && order[0]==0 && order[1]==1 && order[2]==0);
  assert(ui_draw_list_begin(list));
  assert(r->text_clip_push(list,0,0,10,10));
  assert(!ui_draw_list_replay(list) && order_count==3);
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_CLIP);
  assert(ui_draw_list_begin(list));
  r->draw_text_colored(list,&font,"text",0,0,1,1,1,1);
  assert(r->text_clip_push(list,0,0,10,10)); assert(r->text_clip_pop(list));
  list->target.text_clip_push=reject_clip;
  assert(!ui_draw_list_replay(list) && rec.depth==0);
  list->target.text_clip_push=record_text_clip_push;
  assert(ui_draw_list_begin(list));
  char *growth=malloc(list->text_capacity+1); assert(growth);
  memset(growth,'x',list->text_capacity); growth[list->text_capacity]=0;
  fail_after=0; r->draw_text_colored(list,&font,growth,0,0,1,1,1,1); fail_after=-1;
  free(growth);
  int texts=rec.calls[2];
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_MEMORY && !ui_draw_list_replay(list));
  assert(rec.calls[2]==texts);
  assert(ui_draw_list_begin(list));
  assert(r->texture_manager_preview_load(list,"image.png")==7);
  r->draw_textured_quad(list,0,0,64,32,7,0,0,1,1);
  r->texture_manager_preview_clear(list);
  assert(rec.clears==0 && ui_draw_list_replay(list));
  assert(rec.clears==0); /* GPU submission must precede next begin/discard. */
  assert(ui_draw_list_begin(list) && rec.clears==1);
  r->draw_textured_quad(list,0,0,64,32,7,0,0,1,1);
  assert(r->texture_manager_preview_load(list,"image.png")==-1);
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_RESOURCE && !ui_draw_list_replay(list));
  assert(ui_context_destroy(ctx));
  ui_draw_list_destroy(list);
  list=ui_draw_list_create(&backend,&rec); assert(list && ui_draw_list_begin(list));
  fail_after=0; r->draw_quad(list,0,0,1,1,1,1,1); fail_after=-1;
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_MEMORY && !ui_draw_list_replay(list));
  assert(ui_draw_list_begin(list));
  for (size_t i=0;i<=UI_LIST_MAX_COMMANDS;++i)
    r->draw_quad(list,0,0,1,1,1,1,1);
  assert(ui_draw_list_count(list)==UI_LIST_MAX_COMMANDS);
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_LIMIT && !ui_draw_list_replay(list));
  assert(ui_draw_list_begin(list) && ui_draw_list_count(list)==0);
  assert(!r->text_clip_pop(list));
  assert(ui_draw_list_error(list)==UI_DRAW_LIST_CLIP);
  ui_draw_list_destroy(list);
  puts("UI draw list: PASS (deferred widgets, owned text/arrays, clip order, failures, preview lifetime)");
}
