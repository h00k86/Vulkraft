#pragma once

/* Public drawing primitives in logical canvas coordinates. HUD-prefixed names
 * are retained for source compatibility; they draw into the same UI batches. */

typedef struct APP APP;
#include "math/types.h"
#include "render/font.h"
#include "render/text_anchor.h"
#include <stdbool.h>
#include <stdint.h>

#define HUD_INITIAL_VERTEX_CAPACITY 10000u

void text_begin(APP *app);
void text_end(APP *app);
void text_renderer_shutdown(void);

typedef struct {
  uint32_t vertex_count;
  uint32_t vertex_capacity;
  uint32_t peak_vertex_count;
  uint32_t capacity_growth_count;
} HUDGeometryDiagnostics;

bool text_hud_get_geometry_diagnostics(HUDGeometryDiagnostics *out);

typedef struct {
  int first_vertex;
  int vertex_count;
  float clip_x, clip_y, clip_w, clip_h;
} HUDDrawBatch;

bool text_clip_push(float x, float y, float w, float h);
bool text_clip_pop(void);
int text_hud_batch_count(void);
const HUDDrawBatch *text_hud_batch_at(int index);
bool text_clip_stack_balanced(void);
int text_clip_depth(void);

void draw_text(APP *app, Font *font, const char *text, float x, float y,
               float scale);

void draw_text_colored(APP *app, Font *font, const char *text,
                       float x, float y, float scale,
                       float r, float g, float b);

/* Tilt/shear text around its baseline. */
void draw_text_tilted(APP *app, Font *font, const char *text, float x,
                      float y, float scale, float degrees,
                      float r, float g, float b, float alpha);

void draw_text_anchored(APP *app, Font *font, const char *text,
                        TextAnchor anchor, float margin, float scale);

float measure_text_width(Font *font, const char *text, float scale);

void draw_quad_alpha(APP *app, float x, float y, float w, float h, float r,
                     float g, float b, float a);
void draw_quad(APP *app, float x, float y, float w, float h,
              float r, float g, float b);

void draw_textured_quad(APP *app, float x, float y, float w, float h,
                        int textureSlot, float u0, float v0, float u1,
                        float v1);

void draw_quad_points(APP *app, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3,
                      float r, float g, float b);

void draw_hud_quad_aa(APP *app, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3,
                      float r, float g, float b);

void hud_paper_corners(float x, float y, float w, float h, uint32_t seed, Vec2 p[4]);

/* Optional paper skin: decorative textured accent, slot -1 disables it. */
void draw_hud_trash_swipe(APP *app, float x, float y, float w, float h, bool focused, uint32_t seed,
                          int textureSlot);
void draw_hud_paper_panel(APP *app, float x, float y, float w, float h,
                          const float fill[4], const float accent[3],
                          bool focused, uint32_t seed);
void draw_hud_star_badge(APP *app, float x, float y, float radius,
                         const float color[3]);

void draw_hud_line(APP *app, float x0, float y0, float x1, float y1,
                   float thickness, float r, float g, float b);

/* Offset/scale subsequent geometry and clips; (0,0,1) restores identity. */
void text_hud_transform(float ox, float oy, float scale);

void draw_hud_tri_rgba(APP *app, Vec2 p0, const float c0[4], Vec2 p1,
                       const float c1[4], Vec2 p2, const float c2[4]);
void draw_hud_circle(APP *app, float cx, float cy, float radius,
                     bool filled, float thickness, float r, float g,
                     float b);

void draw_textured_quad_points(APP *app, Vec2 p0, Vec2 p1, Vec2 p2,
                               Vec2 p3, int textureSlot, float u0, float v0,
                               float u1, float v1);

/* Per-corner UVs and RGBA tint. Corners are ordered around the quad. */
void draw_textured_quad_uv4(APP *app, const Vec2 p[4], const Vec2 uv[4],
                            int textureSlot, const float rgba[4]);

void draw_textured_disc(APP *app, float cx, float cy, float radius,
                        int textureSlot, float u0, float v0, float u1,
                        float v1, const float rgba[4]);

void draw_textured_quad_points_alpha(APP *app, Vec2 p0, Vec2 p1, Vec2 p2,
                                     Vec2 p3, int textureSlot, float u0,
                                     float v0, float u1, float v1,
                                     float alpha);
