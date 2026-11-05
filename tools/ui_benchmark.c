/* CPU-only benchmark. GNU ld wrappers observe framework allocator requests;
 * they do not measure RSS, allocator overhead, GPU work or backend font cost. */
#define _POSIX_C_SOURCE 200809L
#include "ui/ui.h"
#include "ui/draw_list.h"
#include "../src/ui/ui_state_buttons_internal.h" /* isolated registry timing */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct Font { int unused; };
static bool tracking;
static size_t allocations, frees, requested;
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
void *__real_realloc(void *,size_t);
void __real_free(void *);
void *__wrap_malloc(size_t n) { if(tracking) { ++allocations; requested+=n; } return __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t s) { if(tracking) { ++allocations; requested+=n*s; } return __real_calloc(n,s); }
void *__wrap_realloc(void *p,size_t n) { if(tracking) { ++allocations; requested+=n; } return __real_realloc(p,n); }
void __wrap_free(void *p) { if(tracking&&p) ++frees; __real_free(p); }
static void reset_counts(void) { allocations=frees=requested=0; }
static void require(bool ok) { if(!ok) { fputs("benchmark contract failed\n",stderr); exit(1); } }
static double now(void) {
  struct timespec t; require(clock_gettime(CLOCK_MONOTONIC,&t)==0);
  return t.tv_sec+t.tv_nsec*1e-9;
}
static unsigned long long draws;
static void quad(void *u,float x,float y,float w,float h,float r,float g,float b) { ++draws; }
static void alpha(void *u,float x,float y,float w,float h,float r,float g,float b,float a) { ++draws; }
static void text(void *u,Font *f,const char *s,float x,float y,float z,float r,float g,float b) { ++draws; }
static void image(void *u,float x,float y,float w,float h,int s,float a,float b,float c,float d) { ++draws; }
static float ascent(void *u,Font *f,float s) { return s; }
static float measure(void *u,Font *f,const char *s,float z) { return strlen(s)*z*.55f; }
static int depth;
static bool push(void *u,float x,float y,float w,float h) { ++depth; return true; }
static bool pop(void *u) { if(!depth) return false; --depth; return true; }
static int clip_depth(void *u) { return depth; }
static const UIRenderer renderer={.draw_quad=quad,.draw_quad_alpha=alpha,
  .draw_text_colored=text,.draw_textured_quad=image,.font_ascent=ascent,
  .measure_text_width=measure,.text_clip_push=push,.text_clip_pop=pop,.text_clip_depth=clip_depth};
static char ids[512][32];
static Font font;
static void frame(int count,UIDrawList *list) {
  UIFrameInput input={.width=4096,.height=4096,.mouse_x=-1,.mouse_y=-1,.delta_time=1.f/60};
  if(list) require(ui_draw_list_begin(list));
  require(ui_begin_frame_input(&input));
  UIButtonStyle style={.min_width=100,.min_height=24};
  for(int i=0;i<count;++i)
    ui_button_labeled(ids[i],"Benchmark",&font,ANCHOR_TOP_LEFT,0,12,
                      (i%16)*200.f,(i/16)*32.f,&style);
  ui_end_frame();
  if(list) require(ui_draw_list_replay(list));
  require(depth==0);
}
static int compare(const void *a,const void *b) {
  double x=*(const double *)a,y=*(const double *)b; return (x>y)-(x<y);
}
int main(void) {
  enum { SAMPLES=200, WARMUP=20 };
  for(int i=0;i<512;++i) snprintf(ids[i],sizeof(ids[i]),"button-%04d",i);
  reset_counts(); tracking=true;
  UIContext *probe=ui_context_create();
  tracking=false; require(probe!=NULL);
  size_t context_bytes=requested, context_requests=allocations;
  require(context_bytes>0&&context_requests>0);
  require(ui_context_destroy(probe));
  printf("{\"scope\":\"CPU core, simulated renderer, GNU allocator wrappers\","
         "\"context_requested_bytes\":%zu,\"context_allocation_requests\":%zu,"
         "\"warmup_frames\":%d,\"sample_frames\":%d,\"cases\":[",
         context_bytes,context_requests,WARMUP,SAMPLES);
  int counts[]={32,128,512};
  for(int mode=0;mode<2;++mode) for(int c=0;c<3;++c) {
    int n=counts[c];
    UIContext *ctx=ui_context_create(); require(ctx&&ui_context_set_current(ctx));
    UIDrawList *list=mode?ui_draw_list_create(&renderer,NULL):NULL;
    require(!mode||list);
    require(ui_context_set_renderer(ctx,list?ui_draw_list_renderer():&renderer,list));
    UITheme theme=ui_get_theme(); theme.paper_panels=false; ui_set_theme(theme);
    for(int i=0;i<WARMUP;++i) frame(n,list);
    require(ui_state_buttons(ctx)->ui_state_count==n);
    double samples[SAMPLES]; reset_counts(); draws=0; tracking=true;
    for(int i=0;i<SAMPLES;++i) { double start=now(); frame(n,list); samples[i]=(now()-start)*1e6; }
    tracking=false;
    size_t allocs=allocations, freed=frees, bytes=requested;
    double total=0; for(int i=0;i<SAMPLES;++i) total+=samples[i];
    qsort(samples,SAMPLES,sizeof(double),compare);
    /* All entries exist: uniform successful lookup, including first/last ID.
     * This private probe excludes painting, registration and frame bookkeeping. */
    UIState *volatile sink=NULL;
    double start=now();
    for(int repeat=0;repeat<1000;++repeat)
      for(int i=0;i<n;++i) sink=ui_button_state(ids[i]);
    double lookup_ns=(now()-start)*1e9/(1000*n); require(sink!=NULL);
    printf("%s{\"mode\":\"%s\",\"buttons\":%d,\"mean_us\":%.3f,\"median_us\":%.3f,"
           "\"p95_us\":%.3f,\"registry_lookup_ns\":%.3f,\"allocation_requests_per_frame\":%.2f,"
           "\"free_calls_per_frame\":%.2f,\"requested_bytes_per_frame\":%.2f,\"draw_callbacks_per_frame\":%.2f}",
           mode||c?",":"",mode?"deferred":"immediate",n,total/SAMPLES,samples[SAMPLES/2],
           samples[(SAMPLES*95/100)-1],lookup_ns,(double)allocs/SAMPLES,
           (double)freed/SAMPLES,(double)bytes/SAMPLES,(double)draws/SAMPLES);
    require(ui_context_destroy(ctx)); ui_draw_list_destroy(list);
  }
  puts("]}"); return 0;
}
