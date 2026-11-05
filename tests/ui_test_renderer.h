#include "../src/ui/ui_state_dropdowns_internal.h"
#pragma once
#include <assert.h>
#include <string.h>
/* Headless backend for legacy widget tests; no GPU linkage. */
#include "render/text_renderer.h"
void texture_manager_preview_clear(APP *app);
static void mock_render_draw_quad(void *user, float x, float y, float w, float h, float r, float g, float b) {
#ifndef TEST_RENDER_STATE_ONLY
  draw_quad(NULL, x, y, w, h, r, g, b);
#else
#endif
}
static void mock_render_draw_quad_alpha(void *user, float x, float y, float w, float h, float r, float g, float b, float a) {
}
static void mock_render_draw_text_colored(void *user, Font *font, const char *text, float x, float y, float scale, float r, float g, float b) {
#ifndef TEST_RENDER_STATE_ONLY
  draw_text_colored(NULL, font, text, x, y, scale, r, g, b);
#else
#endif
}
static void mock_render_draw_textured_quad(void *user, float x, float y, float w, float h, int textureSlot, float u0, float v0, float u1, float v1) {
}
static void mock_render_draw_hud_paper_panel(void *user, float x, float y, float w, float h, const float fill[4], const float accent[3], bool focused, uint32_t seed) {
#ifndef TEST_RENDER_STATE_ONLY
  draw_hud_paper_panel(NULL, x, y, w, h, fill, accent, focused, seed);
#else
#endif
}
static void mock_render_draw_hud_trash_swipe(void *user, float x, float y, float w, float h, bool focused, uint32_t seed, int textureSlot) {
}
static float mock_render_font_ascent(void *user, Font *font, float scale) {
  return font->ascender * scale;
}
static float mock_render_measure_text_width(void *user, Font *font, const char *text, float scale) {
#ifndef TEST_RENDER_STATE_ONLY
  return measure_text_width(font, text, scale);
#else
  return 0;
#endif
}
static bool mock_render_text_clip_push(void *user, float x, float y, float w, float h) {
  return false;
}
static bool mock_render_text_clip_pop(void *user) {
  return false;
}
static int mock_render_text_clip_depth(void *user) {
  return -1;
}
static bool mock_render_texture_manager_get_size(void *user, int slot, int *outW, int *outH) {
  return false;
}
static int mock_render_texture_manager_preview_load(void *user, const char *path) {
  return -1;
}
static void mock_render_texture_manager_preview_clear(void *user) {
  texture_manager_preview_clear(NULL);
}
static void bind_test_renderer(UIContext *ctx) {
  const UIRenderer renderer={
    .draw_quad=mock_render_draw_quad,
    .draw_quad_alpha=mock_render_draw_quad_alpha,
    .draw_text_colored=mock_render_draw_text_colored,
    .draw_textured_quad=mock_render_draw_textured_quad,
    .draw_hud_paper_panel=mock_render_draw_hud_paper_panel,
    .draw_hud_trash_swipe=mock_render_draw_hud_trash_swipe,
    .measure_text_width=mock_render_measure_text_width,
    .font_ascent=mock_render_font_ascent,
    .text_clip_push=mock_render_text_clip_push,
    .text_clip_pop=mock_render_text_clip_pop,
    .text_clip_depth=mock_render_text_clip_depth,
    .texture_manager_get_size=mock_render_texture_manager_get_size,
    .texture_manager_preview_load=mock_render_texture_manager_preview_load,
    .texture_manager_preview_clear=mock_render_texture_manager_preview_clear,
  };
  assert(ui_context_set_renderer(ctx,&renderer,NULL));
}

/* Create through the public API, then inspect the existing white-box registry. */
static inline UIDropdownState *test_dropdown_state(const char *id) {
  ui_dropdown_set_open(id,false);
  UIContext *ctx=ui_context_get_current();
  for(int n=0;n<ui_state_dropdowns(ctx)->dropdown_state_count;++n)
    if(!strcmp(ui_state_dropdowns(ctx)->dropdown_states[n].id,id)) return &ui_state_dropdowns(ctx)->dropdown_states[n];
  return NULL;
}
