#include "../src/ui/ui_state_render_internal.h"
#include "render/font.h"
/* Recording backend: real widgets, no APP definition or concrete renderer. */
#include "ui/ui.h"
#include "../src/ui/ui_paint_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
typedef struct {
  int calls[14], depth, clears;
  float last_width, alpha;
  char label[128];
} Recorder;
static void record_draw_quad(void *user, float x, float y, float w, float h, float r, float g, float b) {
  Recorder *rec=user; ++rec->calls[0];
  rec->last_width=w;
}
static void record_draw_quad_alpha(void *user, float x, float y, float w, float h, float r, float g, float b, float a) {
  Recorder *rec=user; ++rec->calls[1];
  rec->alpha=a;
}
static void record_draw_text_colored(void *user, Font *font, const char *text, float x, float y, float scale, float r, float g, float b) {
  Recorder *rec=user; ++rec->calls[2];
  snprintf(rec->label,sizeof(rec->label),"%s",text);
}
static void record_draw_textured_quad(void *user, float x, float y, float w, float h, int textureSlot, float u0, float v0, float u1, float v1) {
  Recorder *rec=user; ++rec->calls[3];

}
static void record_draw_hud_paper_panel(void *user, float x, float y, float w, float h, const float fill[4], const float accent[3], bool focused, uint32_t seed) {
  Recorder *rec=user; ++rec->calls[4];

}
static void record_draw_hud_trash_swipe(void *user, float x, float y, float w, float h, bool focused, uint32_t seed, int textureSlot) {
  Recorder *rec=user; ++rec->calls[5];

}
static float record_font_ascent(void *user, Font *font, float scale) {
  Recorder *rec=user; ++rec->calls[13];
  return font->ascender * scale;
}
static float record_measure_text_width(void *user, Font *font, const char *text, float scale) {
  Recorder *rec=user; ++rec->calls[6];
  return (float)strlen(text)*scale;
}
static bool record_text_clip_push(void *user, float x, float y, float w, float h) {
  Recorder *rec=user; ++rec->calls[7];
  ++rec->depth; return true;
}
static bool record_text_clip_pop(void *user) {
  Recorder *rec=user; ++rec->calls[8];
  if (!rec->depth) return false;
  --rec->depth; return true;
}
static int record_text_clip_depth(void *user) {
  Recorder *rec=user; ++rec->calls[9];
  return rec->depth;
}
static bool record_texture_manager_get_size(void *user, int slot, int *outW, int *outH) {
  Recorder *rec=user; ++rec->calls[10];
  assert(slot==7); *outW=64; *outH=32; return true;
}
static int record_texture_manager_preview_load(void *user, const char *path) {
  Recorder *rec=user; ++rec->calls[11];
  assert(!strcmp(path,"image.png")); return 7;
}
static void record_texture_manager_preview_clear(void *user) {
  Recorder *rec=user; ++rec->calls[12];
  ++rec->clears;
}
static UIRenderer recording_backend(void) {
  return (UIRenderer){
    .draw_quad=record_draw_quad,
    .draw_quad_alpha=record_draw_quad_alpha,
    .draw_text_colored=record_draw_text_colored,
    .draw_textured_quad=record_draw_textured_quad,
    .draw_hud_paper_panel=record_draw_hud_paper_panel,
    .draw_hud_trash_swipe=record_draw_hud_trash_swipe,
    .measure_text_width=record_measure_text_width,
    .font_ascent=record_font_ascent,
    .text_clip_push=record_text_clip_push,
    .text_clip_pop=record_text_clip_pop,
    .text_clip_depth=record_text_clip_depth,
    .texture_manager_get_size=record_texture_manager_get_size,
    .texture_manager_preview_load=record_texture_manager_preview_load,
    .texture_manager_preview_clear=record_texture_manager_preview_clear,
  };
}
static bool reject_scope_clip(void *user,float x,float y,float w,float h) {
  Recorder *rec=user; ++rec->calls[7];
  return false;
}
static void check_scope_clips(void) {
  Recorder rec={0};
  UIContext *ctx=ui_context_create();
  UIRenderer backend=recording_backend();
  assert(ctx && ui_context_set_current(ctx));
  assert(ui_context_set_renderer(ctx,&backend,&rec));
  assert(ui_clip_push((UIRect){0,0,640,400}));
  Font font={.ascender=1};
  UIPanel panel=ui_panel_begin("panel","",&font,(UIRect){0,0,200,100},1);
  UIScrollScope scroll=ui_scroll_begin("scroll",(UIRect){10,10,100,50},1);
  assert(panel.clip_pushed && scroll.clip_pushed && rec.depth==3);
  ui_scroll_end(&scroll); ui_scroll_end(&scroll);
  assert(rec.depth==2 && !scroll.open && !scroll.clip_pushed);
  ui_panel_end(&panel); ui_panel_end(&panel);
  assert(rec.depth==1 && !panel.open && !panel.clip_pushed);
  backend.text_clip_push=reject_scope_clip;
  assert(ui_context_set_renderer(ctx,&backend,&rec));
  int pops=rec.calls[8];
  panel=ui_panel_begin("failed-panel","",&font,(UIRect){0,0,200,100},1);
  scroll=ui_scroll_begin("failed-scroll",(UIRect){10,10,100,50},1);
  assert(!panel.clip_pushed && !scroll.clip_pushed && rec.depth==1);
  ui_scroll_end(&scroll); ui_panel_end(&panel);
  ui_scroll_end(&scroll); ui_panel_end(&panel);
  assert(rec.depth==1 && rec.calls[8]==pops);
  /* Invalid/empty content rejects clipping before calling the backend. */
  backend=recording_backend();
  assert(ui_context_set_renderer(ctx,&backend,&rec));
  panel=ui_panel_begin("empty-panel","",&font,(UIRect){0,0,0,0},1);
  scroll=ui_scroll_begin("empty-scroll",(UIRect){0,0,0,0},1);
  assert(!panel.clip_pushed && !scroll.clip_pushed);
  ui_scroll_end(&scroll); ui_panel_end(&panel);
  UIScrollScope empty={0}; UIPanel empty_panel={0};
  ui_scroll_end(&empty); ui_panel_end(&empty_panel);
  assert(rec.depth==1 && rec.calls[8]==pops);
  int pushes=rec.calls[7];
  assert(!ui_clip_push((UIRect){NAN,0,10,10}));
  assert(!ui_clip_push((UIRect){0,0,INFINITY,10}));
  assert(rec.calls[7]==pushes && rec.depth==1);
  assert(ui_clip_pop() && rec.depth==0);
  assert(ui_context_destroy(ctx));
}
int main(void) {
  check_scope_clips();
  Recorder first={0}, second={0};
  UIContext *a=ui_context_create(), *b=ui_context_create();
  assert(a && b);
  UIRenderer backend=recording_backend(), incomplete=backend;
  incomplete.measure_text_width=NULL;
  assert(!ui_context_set_renderer(a,&incomplete,&first));
  incomplete=backend; incomplete.font_ascent=NULL;
  assert(!ui_context_set_renderer(a,&incomplete,&first));
  assert(ui_context_set_renderer(a,&backend,&first));
  assert(ui_context_set_renderer(b,&backend,&second));
  backend.draw_quad=NULL; /* Table is copied, not borrowed. */
  assert(ui_context_set_current(a));
  UIFrameInput frame={.width=640,.height=400,.delta_time=0.016f};
  assert(ui_begin_frame_input(&frame));
  assert(!ui_context_set_renderer(a,NULL,NULL));
  Font tall={.ascender=1.75f};
  UIButtonStyle metrics_style={.padding_y=3};
  assert(ui_button_height(&tall,12,&metrics_style)==27);
  assert(first.calls[13]==1 && second.calls[13]==0);
  Font font={.ascender=1};
  ui_button_labeled("button","Caffè",&font,ANCHOR_TOP_LEFT,0,14,0,0,NULL);
  assert(first.calls[0]>0 && first.calls[2]>0 && first.calls[6]>0);
  assert(!strcmp(first.label,"Caffè") && second.calls[0]==0);
  assert(ui_clip_push((UIRect){0,0,100,50}) && ui_clip_depth()==1);
  assert(ui_clip_pop() && ui_clip_depth()==0 && !ui_clip_pop());
  bool open=true;
  float x,y,w,h;
  ui_modal_begin("modal","Dialog",&font,300,200,&open,&x,&y,&w,&h);
  assert(first.calls[1]>0 && first.alpha>0 && first.alpha<1);
  ui_modal_end("modal",false);
  ui_end_frame();
  /* Preview resource operations and teardown use the same bound user. */
  assert(ui_paint_texture_manager_preview_load("image.png")==7);
  int width=0,height=0;
  assert(ui_paint_texture_manager_get_size(7,&width,&height) && width==64 && height==32);
  ui_paint_draw_textured_quad(0,0,64,32,7,0,0,1,1);
  ui_state_render(a)->preview_owned=true;
  assert(ui_context_set_current(b));
  assert(first.clears==1 && second.clears==0);
  assert(ui_begin_frame_input(&frame));
  ui_button_labeled("button","Second",&font,ANCHOR_TOP_LEFT,0,14,0,0,NULL);
  ui_end_frame();
  assert(second.calls[0]>0 && !strcmp(second.label,"Second"));
  ui_state_render(b)->preview_owned=true;
  backend=recording_backend();
  assert(ui_context_set_renderer(b,&backend,&first));
  assert(second.clears==1); /* Clear with old backend before replacing user. */
  assert(ui_context_reset(b) && ui_state_render(b)->renderer_user==&first);
  ui_state_render(b)->preview_owned=true;
  assert(ui_context_set_renderer(b,NULL,NULL));
  assert(first.clears==2 && ui_state_render(b)->renderer_user==NULL);
  assert(ui_context_destroy(a) && ui_context_destroy(b));
  puts("UI renderer: PASS (real widgets, copied backend, text/alpha/clip/image dispatch, isolation, lifecycle)");
  return 0;
}
