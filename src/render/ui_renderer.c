#include "ui/app_adapter.h"
#include "ui/ui.h"
#include "app/application.h"
#include "render/text_renderer.h"
#include "render/texture_manager.h"

static void app_draw_quad(void *user, float x, float y, float w, float h, float r, float g, float b) {
  draw_quad((APP *)user, x, y, w, h, r, g, b);
}
static void app_draw_quad_alpha(void *user, float x, float y, float w, float h, float r, float g, float b, float a) {
  draw_quad_alpha((APP *)user, x, y, w, h, r, g, b, a);
}
static void app_draw_text_colored(void *user, Font *font, const char *text, float x, float y, float scale, float r, float g, float b) {
  draw_text_colored((APP *)user, font, text, x, y, scale, r, g, b);
}
static void app_draw_textured_quad(void *user, float x, float y, float w, float h, int textureSlot, float u0, float v0, float u1, float v1) {
  draw_textured_quad((APP *)user, x, y, w, h, textureSlot, u0, v0, u1, v1);
}
static void app_draw_hud_paper_panel(void *user, float x, float y, float w, float h, const float fill[4], const float accent[3], bool focused, uint32_t seed) {
  draw_hud_paper_panel((APP *)user, x, y, w, h, fill, accent, focused, seed);
}
static void app_draw_hud_trash_swipe(void *user, float x, float y, float w, float h, bool focused, uint32_t seed, int textureSlot) {
  draw_hud_trash_swipe((APP *)user, x, y, w, h, focused, seed, textureSlot);
}
static float app_font_ascent(void *user, Font *font, float scale) {
  return font->ascender * scale;
}
static float app_measure_text_width(void *user, Font *font, const char *text, float scale) {
  return measure_text_width(font, text, scale);
}
static bool app_text_clip_push(void *user, float x, float y, float w, float h) {
  return text_clip_push(x, y, w, h);
}
static bool app_text_clip_pop(void *user) {
  return text_clip_pop();
}
static int app_text_clip_depth(void *user) {
  return text_clip_depth();
}
static bool app_texture_manager_get_size(void *user, int slot, int *outW, int *outH) {
  return texture_manager_get_size(slot, outW, outH);
}
static int app_texture_manager_preview_load(void *user, const char *path) {
  return texture_manager_preview_load((APP *)user, path);
}
static void app_texture_manager_preview_clear(void *user) {
  texture_manager_preview_clear((APP *)user);
}
const UIRenderer *ui_app_renderer(void) {
  static const UIRenderer renderer={
    .draw_quad=app_draw_quad,
    .draw_quad_alpha=app_draw_quad_alpha,
    .draw_text_colored=app_draw_text_colored,
    .draw_textured_quad=app_draw_textured_quad,
    .draw_hud_paper_panel=app_draw_hud_paper_panel,
    .draw_hud_trash_swipe=app_draw_hud_trash_swipe,
    .measure_text_width=app_measure_text_width,
    .font_ascent=app_font_ascent,
    .text_clip_push=app_text_clip_push,
    .text_clip_pop=app_text_clip_pop,
    .text_clip_depth=app_text_clip_depth,
    .texture_manager_get_size=app_texture_manager_get_size,
    .texture_manager_preview_load=app_texture_manager_preview_load,
    .texture_manager_preview_clear=app_texture_manager_preview_clear,
  };
  return &renderer;
}
bool ui_bind_app_renderer(APP *app) {
  return app && app->ui && ui_context_set_renderer(app->ui, ui_app_renderer(), app)
      && ui_bind_app_clipboard(app);
}
