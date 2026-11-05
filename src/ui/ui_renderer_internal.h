#pragma once
#include "ui/renderer.h"

/* Shared by immediate painting and deferred replay so fallback geometry agrees. */
static inline bool ui_renderer_valid(const UIRenderer *r) {
  return r && r->draw_quad && r->draw_quad_alpha && r->draw_text_colored &&
         r->draw_textured_quad && r->font_ascent && r->measure_text_width &&
         r->text_clip_push && r->text_clip_pop && r->text_clip_depth &&
         ((!r->texture_manager_preview_load && !r->texture_manager_preview_clear) ||
          (r->texture_manager_preview_load && r->texture_manager_preview_clear &&
           r->texture_manager_get_size));
}
static inline void ui_renderer_paper(const UIRenderer *r, void *user,
    float x, float y, float w, float h, const float fill[4],
    const float accent[3], bool focused, uint32_t seed) {
  if (r->draw_hud_paper_panel) {
    r->draw_hud_paper_panel(user,x,y,w,h,fill,accent,focused,seed);
    return;
  }
  if (w<=0 || h<=0) return;
  r->draw_quad_alpha(user,x,y,w,h,fill[0],fill[1],fill[2],fill[3]);
  if (focused)
    r->draw_quad(user,x,y,w,h<2.0f?h:2.0f,accent[0],accent[1],accent[2]);
}
static inline void ui_renderer_swipe(const UIRenderer *r, void *user,
    float x, float y, float w, float h, bool focused, uint32_t seed, int slot) {
  if (r->draw_hud_trash_swipe) {
    r->draw_hud_trash_swipe(user,x,y,w,h,focused,seed,slot);
    return;
  }
  if (w<=0 || h<=0) return;
  if (slot>=0)
    r->draw_textured_quad(user,x,y,w,h,slot,0,0,1,1);
  else
    r->draw_quad_alpha(user,x,y,w,h,0,0,0,focused?0.25f:0.15f);
}
