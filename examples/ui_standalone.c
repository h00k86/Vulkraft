/* Public-API example: no application, window, GPU, assets or private UI headers.
 * A deterministic backend records draw operations instead of displaying pixels. */
#include "ui/ui.h"
#include "ui/draw_list.h"
#include "text/utf8.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* This backend needs only a baseline metric, not glyphs or an atlas. */
struct Font { float baseline; };
static float font_ascent(void *user, Font *font, float scale) {
  (void)user;
  return font->baseline * scale;
}

typedef struct {
  uint64_t hash;
  unsigned draws, texts, clips, measurements;
  UIRect clip[32];
  int depth;
  bool failed;
} TraceRenderer;

static void hash_bytes(TraceRenderer *trace, const void *bytes, size_t size) {
  const unsigned char *p=bytes;
  for (size_t i=0;i<size;++i) { trace->hash^=p[i]; trace->hash*=UINT64_C(1099511628211); }
}
static void command(TraceRenderer *trace, const char *name, const float *values, size_t count) {
  hash_bytes(trace,name,strlen(name)+1);
  hash_bytes(trace,values,count*sizeof(*values));
  ++trace->draws;
}
static void quad(void *user,float x,float y,float w,float h,float r,float g,float b) {
  const float values[]={x,y,w,h,r,g,b}; command(user,"quad",values,7);
}
static void quad_alpha(void *user,float x,float y,float w,float h,float r,float g,float b,float a) {
  const float values[]={x,y,w,h,r,g,b,a}; command(user,"alpha",values,8);
}
static void text(void *user,Font *font,const char *label,float x,float y,float scale,float r,float g,float b) {
  TraceRenderer *trace=user; (void)font;
  const float values[]={x,y,scale,r,g,b}; command(trace,"text",values,6);
  hash_bytes(trace,label,strlen(label)+1); ++trace->texts;
}
static void image(void *user,float x,float y,float w,float h,int slot,float u0,float v0,float u1,float v1) {
  TraceRenderer *trace=user;
  const float values[]={x,y,w,h,u0,v0,u1,v1}; command(trace,"image",values,8);
  hash_bytes(trace,&slot,sizeof(slot));
}
static float measure(void *user,Font *font,const char *label,float scale) {
  TraceRenderer *trace=user; (void)font; ++trace->measurements;
  /* Simulated monospace metrics in Unicode scalars, not UTF-8 bytes. */
  size_t remaining=strlen(label), count=0;
  while (remaining) {
    uint32_t scalar; size_t bytes=utf8_decode(label,remaining,&scalar);
    label+=bytes; remaining-=bytes; ++count;
  }
  return (float)count*scale*0.55f;
}
static bool clip_push(void *user,float x,float y,float w,float h) {
  TraceRenderer *trace=user;
  if (trace->depth==32) { trace->failed=true; return false; }
  UIRect rect={x,y,w,h};
  if (trace->depth) {
    UIRect parent=trace->clip[trace->depth-1];
    float right=fminf(x+w,parent.x+parent.w), bottom=fminf(y+h,parent.y+parent.h);
    rect.x=fmaxf(x,parent.x); rect.y=fmaxf(y,parent.y);
    rect.w=fmaxf(0,right-rect.x); rect.h=fmaxf(0,bottom-rect.y);
  }
  trace->clip[trace->depth++]=rect; ++trace->clips;
  const float values[]={rect.x,rect.y,rect.w,rect.h}; command(trace,"push",values,4);
  return true;
}
static bool clip_pop(void *user) {
  TraceRenderer *trace=user;
  if (!trace->depth) { trace->failed=true; return false; }
  --trace->depth; command(trace,"pop",NULL,0); return true;
}
static int clip_depth(void *user) { return ((TraceRenderer *)user)->depth; }
static const UIRenderer backend={
  .draw_quad=quad, .draw_quad_alpha=quad_alpha, .draw_text_colored=text,
  .draw_textured_quad=image, /* Skin callbacks intentionally omitted. */
  .font_ascent=font_ascent, .measure_text_width=measure, .text_clip_push=clip_push, .text_clip_pop=clip_pop,
  .text_clip_depth=clip_depth /* No preview service needed. */
};

typedef struct { TraceRenderer trace; char value[64]; unsigned clicks; } Result;
static bool run(bool deferred,bool skin,Result *result) {
  bool ok=false, frame_open=false;
  UIContext *previous=ui_context_get_current(), *context=ui_context_create();
  UIDrawList *list=NULL;
  Font font={.baseline=1}; /* Metrics supplied by the simulated backend; no atlas. */
  result->trace.hash=UINT64_C(14695981039346656037);
  if (!context) goto cleanup;
  if (deferred) {
    list=ui_draw_list_create(&backend,&result->trace);
    if (!list) goto cleanup;
  }
  if (!ui_context_set_renderer(context,list?ui_draw_list_renderer():&backend,
                               list?(void *)list:(void *)&result->trace) ||
      !ui_context_set_current(context)) goto cleanup;
  UITheme theme=ui_get_theme();
  theme.paper_panels=skin;
  ui_set_theme(theme);
  ui_text_input_focus("name");
  const InputKeyEvent first[]={
    {.kind=INPUT_CHARACTER,.codepoint='C'}, {.kind=INPUT_CHARACTER,.codepoint='a'},
    {.kind=INPUT_CHARACTER,.codepoint='f'}, {.kind=INPUT_CHARACTER,.codepoint='f'},
    {.kind=INPUT_CHARACTER,.codepoint=0xe8}
  };
  const InputKeyEvent second[]={
    {.kind=INPUT_CHARACTER,.codepoint=' '}, {.kind=INPUT_CHARACTER,.codepoint=0x20ac}
  };
  for (int frame=0;frame<5;++frame) {
    UIFrameInput input={.width=640,.height=400,.delta_time=1.0f/60,
                       .mouse_x=-1,.mouse_y=-1};
    if (frame==0) { input.events=first; input.event_count=5; }
    if (frame==1) { input.events=second; input.event_count=2; }
    if (frame==2 || frame==3) { input.mouse_x=20; input.mouse_y=20; input.mouse_down=frame==2; }
    if (list && !ui_draw_list_begin(list)) goto cleanup;
    UIErrorCode status=ui_begin_frame_input_status(&input);
    if (status!=UI_ERROR_NONE) {
      fprintf(stderr,"UI begin: %s\n",ui_error_code_name(status));
      goto cleanup;
    }
    frame_open=true;
    unsigned before=result->trace.draws;
    if (!ui_clip_push((UIRect){0,0,640,400})) goto cleanup;
    UIButtonStyle style={.min_width=140,.min_height=28};
    if (ui_button_labeled("confirm","Conferma",&font,ANCHOR_TOP_LEFT,0,14,10,10,&style))
      ++result->clicks;
    ui_text_input("name",&font,ANCHOR_TOP_LEFT,0,14,10,60,240,
                  result->value,sizeof(result->value),NULL);
    if (!ui_clip_pop()) goto cleanup;
    ui_draw_dropdown_popups();
    ui_end_frame(); frame_open=false;
    if (list && (result->trace.draws!=before || !ui_draw_list_count(list) || !ui_draw_list_replay(list)))
      goto cleanup;
    if (result->trace.failed || result->trace.depth) goto cleanup;
  }
  ok=result->clicks==1 && !strcmp(result->value,"Caffè €") && result->trace.texts>0;
cleanup:
  if (frame_open) ui_end_frame();
  if (!ui_context_destroy(context)) ok=false;
  if (!ui_context_set_current(previous)) ok=false;
  ui_draw_list_destroy(list);
  return ok;
}
int main(void) {
  for (int skin=0;skin<2;++skin) {
    Result immediate={0}, deferred={0};
    if (!run(false,skin!=0,&immediate) || !run(true,skin!=0,&deferred) ||
        immediate.trace.hash!=deferred.trace.hash ||
        immediate.trace.draws!=deferred.trace.draws ||
        immediate.trace.measurements!=deferred.trace.measurements) {
      fputs("Standalone UI: FAILED (input, lifecycle or replay mismatch)\n",stderr);
      return 1;
    }
    printf("skin=%s\n",skin?"portable fallback":"flat");
    printf("Standalone UI: PASS\ntext=\"%s\" clicks=%u frames=5 per mode\n"
           "immediate/deferred: identical trace (%u operations, %u texts, %u clips)\n",
           deferred.value,deferred.clicks,deferred.trace.draws,deferred.trace.texts,deferred.trace.clips);
  }
  return 0;
}
