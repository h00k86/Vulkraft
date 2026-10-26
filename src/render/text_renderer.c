#include "render/text_renderer.h"
#include "render/gfx_vulkan.h"

#include "log/log.h"
#include "render/vulkan_vertex_buffer.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_SLOT 0 // deve corrispondere a FONT_SLOT nel fragment shader

#define INITIAL_HUD_BATCH_CAPACITY 128u
#define MAX_HUD_CLIP_DEPTH 16
static Vertex *text_vertices = NULL;
static uint32_t text_vertex_capacity = 0;
static uint32_t text_offset = 0;
static uint32_t text_peak_vertices = 0;
static uint32_t text_capacity_growth_count = 0;
static bool text_geometry_failed = false;
static HUDDrawBatch *g_hud_batches = NULL;
static uint32_t g_hud_batch_capacity = 0;
static uint32_t g_hud_batch_count = 0;
static HUDDrawBatch g_clip_stack[MAX_HUD_CLIP_DEPTH];
static int g_clip_depth = 0;

static bool text_reserve_vertices(uint32_t required) {
  if (required <= text_vertex_capacity)
    return true;
  if (required > (uint32_t)INT_MAX) {
    LOG_ERROR("UI geometry size overflow: required=%u", required);
    return false;
  }
#if SIZE_MAX < UINT32_MAX
  if (required > SIZE_MAX / sizeof(*text_vertices))
    return false;
#endif

  uint32_t old_capacity = text_vertex_capacity;
  uint32_t new_capacity = old_capacity ? old_capacity
                                       : HUD_INITIAL_VERTEX_CAPACITY;
  while (new_capacity < required) {
    uint64_t grown = (uint64_t)new_capacity + new_capacity / 2u;
    if (grown <= new_capacity || grown > (uint64_t)INT_MAX) {
      new_capacity = required;
      break;
    }
    new_capacity = (uint32_t)grown;
  }

  Vertex *grown = realloc(text_vertices,
                          (size_t)new_capacity * sizeof(*text_vertices));
  if (!grown) {
    LOG_ERROR("UI CPU vertex allocation failed: required=%u capacity=%u",
              required, new_capacity);
    return false;
  }
  text_vertices = grown;
  text_vertex_capacity = new_capacity;
  ++text_capacity_growth_count;
#ifdef DEBUG
  LOG_INFO("[UI] CPU vertex capacity grow: %u -> %u vertices, required=%u, "
           "bytes=%zu",
           old_capacity, new_capacity, required,
           (size_t)new_capacity * sizeof(*text_vertices));
#endif
  return true;
}

static bool text_append_vertices(const Vertex *vertices, uint32_t count) {
  if (!vertices || count == 0)
    return true;
  if (text_geometry_failed || count > UINT32_MAX - text_offset) {
    if (!text_geometry_failed)
      LOG_ERROR("UI vertex-count overflow: current=%u append=%u", text_offset,
                count);
    text_geometry_failed = true;
    return false;
  }
  uint32_t required = text_offset + count;
  if (!text_reserve_vertices(required)) {
    text_geometry_failed = true;
    return false;
  }
  memcpy(&text_vertices[text_offset], vertices,
         (size_t)count * sizeof(*vertices));
  text_offset = required;
  if (text_offset > text_peak_vertices)
    text_peak_vertices = text_offset;
  return true;
}

static bool text_reserve_batches(uint32_t required) {
  if (required <= g_hud_batch_capacity)
    return true;
  uint32_t new_capacity = g_hud_batch_capacity
                              ? g_hud_batch_capacity + g_hud_batch_capacity / 2u
                              : INITIAL_HUD_BATCH_CAPACITY;
  if (new_capacity < required)
    new_capacity = required;
#if SIZE_MAX < UINT32_MAX
  if (new_capacity > SIZE_MAX / sizeof(*g_hud_batches))
    return false;
#endif
  HUDDrawBatch *grown = realloc(
      g_hud_batches, (size_t)new_capacity * sizeof(*g_hud_batches));
  if (!grown)
    return false;
  g_hud_batches = grown;
  g_hud_batch_capacity = new_capacity;
  return true;
}

static HUDDrawBatch text_clip_intersect(HUDDrawBatch a, HUDDrawBatch b) {
  float x0 = a.clip_x > b.clip_x ? a.clip_x : b.clip_x;
  float y0 = a.clip_y > b.clip_y ? a.clip_y : b.clip_y;
  float ax1 = a.clip_x + a.clip_w;
  float ay1 = a.clip_y + a.clip_h;
  float bx1 = b.clip_x + b.clip_w;
  float by1 = b.clip_y + b.clip_h;
  float x1 = ax1 < bx1 ? ax1 : bx1;
  float y1 = ay1 < by1 ? ay1 : by1;
  return (HUDDrawBatch){.clip_x = x0, .clip_y = y0,
                        .clip_w = x1 > x0 ? x1 - x0 : 0.0f,
                        .clip_h = y1 > y0 ? y1 - y0 : 0.0f};
}

static void text_begin_batch(void) {
  if (!text_reserve_batches(g_hud_batch_count + 1u)) {
    LOG_ERROR("UI draw-batch allocation failed: required=%u",
              g_hud_batch_count + 1u);
    text_geometry_failed = true;
    return;
  }
  HUDDrawBatch *batch = &g_hud_batches[g_hud_batch_count++];
  *batch = g_clip_stack[g_clip_depth - 1];
  batch->first_vertex = (int)text_offset;
  batch->vertex_count = 0;
}

static void text_finish_batch(void) {
  if (g_hud_batch_count > 0)
    g_hud_batches[g_hud_batch_count - 1].vertex_count =
        text_offset - g_hud_batches[g_hud_batch_count - 1].first_vertex;
}

/* TRASFORMAZIONE DELL'HUD (2026-10-01, schermo diviso): mentre si disegna
 * l'HUD di un giocatore, tutto cio' che si disegna in coordinate dell'intero
 * schermo finisce scalato e spostato nel suo riquadro. 0,0,1 = nessuna. */
static float g_hudOx = 0.0f, g_hudOy = 0.0f, g_hudScale = 1.0f;
void text_hud_transform(float ox, float oy, float scale) {
  g_hudOx = ox;
  g_hudOy = oy;
  g_hudScale = scale > 0.0f ? scale : 1.0f;
}

static Vertex text_make_vertex(float x, float y, float r, float g, float b,
                               float a, float u, float v, int texture_slot) {
  x = g_hudOx + x * g_hudScale;
  y = g_hudOy + y * g_hudScale;
  return (Vertex){.pos = {x, y, 0.0f},
                  .color = {r, g, b, a},
                  .uv = {u, v},
                  .textureID = texture_slot};
}

static bool draw_char_colored(Glyph *g, float x, float y, float scale, float r,
                              float col_g, float b) {
  float x0 = x + g->plane_left * scale;
  float x1 = x + g->plane_right * scale;

  float y0 = y - g->plane_top * scale;
  float y1 = y - g->plane_bottom * scale;

  Vertex quad[6] = {
      text_make_vertex(x0, y0, r, col_g, b, 1.0f, g->u0, g->v0, FONT_SLOT),
      text_make_vertex(x1, y0, r, col_g, b, 1.0f, g->u1, g->v0, FONT_SLOT),
      text_make_vertex(x1, y1, r, col_g, b, 1.0f, g->u1, g->v1, FONT_SLOT),
      text_make_vertex(x0, y0, r, col_g, b, 1.0f, g->u0, g->v0, FONT_SLOT),
      text_make_vertex(x1, y1, r, col_g, b, 1.0f, g->u1, g->v1, FONT_SLOT),
      text_make_vertex(x0, y1, r, col_g, b, 1.0f, g->u0, g->v1, FONT_SLOT)};

  return text_append_vertices(quad, 6);
}

// ---- Menu principale provvisorio (2026-09-11): stile testo dedicato ----
// vedi il commento in include/render/text_renderer.h (UITextStyle) sul
// perche' isWater e' il canale riusato per portare lo stato al fragment
// shader senza toccare il layout dei vertici o la pipeline generica.

// Hash intero economico (Wang hash), deterministico: stesso ingresso ->
// stessa uscita SEMPRE, nessuno stato/rand() globale -- e' quello che
// rende "seed deterministico per carattere" letterale, non un'
// approssimazione.



// Vertice di un glifo con rotazione/offset (effetto stop-motion, tutto
// CPU-side: nessuna modifica alla pipeline/al layout Vertex) e canale
// isWater riusato per lo STATO (vedi header) -- unica differenza reale
// rispetto a draw_char_colored sopra.




void text_begin(APP *app) {
  (void)app;
  text_offset = 0;
  g_hud_batch_count = 0;
  text_geometry_failed = !text_reserve_vertices(HUD_INITIAL_VERTEX_CAPACITY);
  g_clip_depth = 1;
  g_clip_stack[0] = (HUDDrawBatch){.clip_x = 0.0f, .clip_y = 0.0f,
                                    .clip_w = VIRTUAL_W, .clip_h = VIRTUAL_H};
  text_begin_batch();
}

void text_end(APP *app) {
  if (g_clip_depth != 1) {
    fprintf(stderr, "UI clip stack imbalance at frame end (depth %d)\n",
            g_clip_depth);
    g_clip_depth = 1;
  }
  text_finish_batch();
  if (text_geometry_failed) {
    LOG_ERROR("UI geometry frame discarded safely after capacity failure");
    update_vertex_buffer(app, &app->renderer.gpu->hud_buffer, NULL, 0);
  } else if (text_offset > 0) {
    update_vertex_buffer(app, &app->renderer.gpu->hud_buffer, text_vertices,
                         text_offset);
  } else {
    update_vertex_buffer(app, &app->renderer.gpu->hud_buffer, NULL, 0);
  }
}

void text_renderer_shutdown(void) {
  free(text_vertices);
  text_vertices = NULL;
  text_vertex_capacity = 0;
  text_offset = 0;
  text_peak_vertices = 0;
  text_capacity_growth_count = 0;
  text_geometry_failed = false;
  free(g_hud_batches);
  g_hud_batches = NULL;
  g_hud_batch_capacity = 0;
  g_hud_batch_count = 0;
  g_clip_depth = 0;
}

bool text_hud_get_geometry_diagnostics(HUDGeometryDiagnostics *out) {
  if (!out)
    return false;
  *out = (HUDGeometryDiagnostics){
      .vertex_count = text_offset,
      .vertex_capacity = text_vertex_capacity,
      .peak_vertex_count = text_peak_vertices,
      .capacity_growth_count = text_capacity_growth_count};
  return true;
}

bool text_clip_push(float x, float y, float w, float h) {
  if (g_clip_depth >= MAX_HUD_CLIP_DEPTH) {
    fprintf(stderr, "UI clip stack overflow (max %d)\n", MAX_HUD_CLIP_DEPTH);
    return false;
  }
  text_finish_batch();
  /* con la trasformazione dell'HUD attiva anche il ritaglio va nel riquadro */
  HUDDrawBatch requested = {.clip_x = g_hudOx + x * g_hudScale, .clip_y = g_hudOy + y * g_hudScale,
                            .clip_w = w * g_hudScale, .clip_h = h * g_hudScale};
  g_clip_stack[g_clip_depth] =
      text_clip_intersect(g_clip_stack[g_clip_depth - 1], requested);
  g_clip_depth++;
  text_begin_batch();
  return true;
}

bool text_clip_pop(void) {
  if (g_clip_depth <= 1) {
    fprintf(stderr, "UI clip stack underflow\n");
    return false;
  }
  text_finish_batch();
  g_clip_depth--;
  text_begin_batch();
  return true;
}

int text_hud_batch_count(void) { return (int)g_hud_batch_count; }
const HUDDrawBatch *text_hud_batch_at(int index) {
  return index >= 0 && (uint32_t)index < g_hud_batch_count
             ? &g_hud_batches[index]
             : NULL;
}
bool text_clip_stack_balanced(void) { return g_clip_depth == 1; }
int text_clip_depth(void) { return g_clip_depth; }

void draw_text_colored(APP *app, Font *font, const char *text, float x,
                       float y, float scale, float r, float col_g, float b) {
  (void)app;
  float cursor_x = x;

  while (*text && !text_geometry_failed) {
    Glyph *g = font_get(font, (uint32_t)*text);
    if (g) {
      if (!draw_char_colored(g, cursor_x, y, scale, r, col_g, b))
        break;

      float glyph_width =
          g->plane_right > g->advance ? g->plane_right : g->advance;
      cursor_x += glyph_width * scale;
    }
    text++;
  }
}

void draw_text_tilted(APP *app, Font *font, const char *text, float x,
                      float y, float scale, float degrees,
                      float r, float g, float b, float alpha) {
  const uint32_t first = text_offset;
  const float px = g_hudOx + (x + measure_text_width(font, text, scale)*0.5f)*g_hudScale;
  const float py = g_hudOy + (y-scale*0.35f)*g_hudScale;
  const float angle = degrees * 0.0174532925f;
  const float cs = cosf(angle), sn = sinf(angle);
  draw_text_colored(app, font, text, x, y, scale, r, g, b);
  for (uint32_t i = first; i < text_offset; ++i) {
    Vertex *v = &text_vertices[i];
    const float dy = v->pos[1]-py, dx = v->pos[0]-px + dy*0.035f;
    v->pos[0] = px + dx*cs - dy*sn;
    v->pos[1] = py + dx*sn + dy*cs;
    v->color[3] *= fmaxf(0.0f, fminf(1.0f, alpha));
  }
}

void draw_text(APP *app, Font *font, const char *text, float x, float y,
               float scale) {
  draw_text_colored(app, font, text, x, y, scale, 1.0f, 1.0f, 1.0f);
}

float measure_text_width(Font *font, const char *text, float scale) {
  float width = 0.0f;
  while (*text) {
    Glyph *g = font_get(font, (uint32_t)*text);
    if (g) {
      float glyph_width =
          g->plane_right > g->advance ? g->plane_right : g->advance;
      width += glyph_width * scale;
    }
    text++;
  }
  return width;
}

void draw_text_anchored(APP *app, Font *font, const char *text,
                        TextAnchor anchor, float margin, float scale) {
  float w = measure_text_width(font, text, scale);
  float ascent = font->ascender * scale;
  float descent = 0.2f * scale;

  float x, y;
  switch (anchor) {
  case ANCHOR_TOP_LEFT:
    x = margin;
    y = margin + ascent;
    break;
  case ANCHOR_TOP_RIGHT:
    x = VIRTUAL_W - margin - w;
    y = margin + ascent;
    break;
  case ANCHOR_BOTTOM_LEFT:
    x = margin;
    y = VIRTUAL_H - margin - descent;
    break;
  case ANCHOR_BOTTOM_RIGHT:
    x = VIRTUAL_W - margin - w;
    y = VIRTUAL_H - margin - descent;
    break;
  case ANCHOR_CENTER:
    x = (VIRTUAL_W - w) * 0.5f;
    y = (VIRTUAL_H + ascent - descent) * 0.5f;
    break;
  default:
    x = margin;
    y = margin + ascent;
  }

  draw_text(app, font, text, x, y, scale);
}

void draw_quad_alpha(APP *app, float x, float y, float w, float h, float r,
                     float g, float b, float a) {
  (void)app;
  if (!(a > 0.0f))
    return;
  if (a > 1.0f)
    a = 1.0f;
  Vertex quad[6] = {
      text_make_vertex(x, y, r, g, b, a, -1.0f, -1.0f, -1),
      text_make_vertex(x + w, y, r, g, b, a, -1.0f, -1.0f, -1),
      text_make_vertex(x + w, y + h, r, g, b, a, -1.0f, -1.0f, -1),
      text_make_vertex(x, y, r, g, b, a, -1.0f, -1.0f, -1),
      text_make_vertex(x + w, y + h, r, g, b, a, -1.0f, -1.0f, -1),
      text_make_vertex(x, y + h, r, g, b, a, -1.0f, -1.0f, -1)};
  text_append_vertices(quad, 6);
}

void draw_quad(APP *app, float x, float y, float w, float h, float r, float g,
               float b) {
  (void)app;

  Vertex quad[6] = {
      text_make_vertex(x, y, r, g, b, 1.0f, -1.0f, -1.0f, -1),
      text_make_vertex(x + w, y, r, g, b, 1.0f, -1.0f, -1.0f, -1),
      text_make_vertex(x + w, y + h, r, g, b, 1.0f, -1.0f, -1.0f, -1),
      text_make_vertex(x, y, r, g, b, 1.0f, -1.0f, -1.0f, -1),
      text_make_vertex(x + w, y + h, r, g, b, 1.0f, -1.0f, -1.0f, -1),
      text_make_vertex(x, y + h, r, g, b, 1.0f, -1.0f, -1.0f, -1)};

  text_append_vertices(quad, 6);
}

// Stesso primitivo di draw_quad sopra, ma da 4 punti ARBITRARI invece
// di x/y/larghezza/altezza — draw_quad da solo può disegnare solo
// rettangoli allineati agli assi, niente di ruotato. Ordine dei punti:
// p0,p1,p2,p3 in giro (es. NW,NE,SE,SW) — stesso ordine di
// gizmo_compute_handles/box2d_compute_world_hitbox_polygon, così chi
// chiama può passare direttamente i loro 4 angoli senza doverli
// riordinare.
void draw_quad_points(APP *app, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float r,
                      float g, float b) {
  (void)app;

  Vertex quad[6] = {
      text_make_vertex(p0.x, p0.y, r, g, b, 1.0f, -1.0f, -1.0f, -1),
      text_make_vertex(p1.x, p1.y, r, g, b, 1.0f, -1.0f, -1.0f, -1),
      text_make_vertex(p2.x, p2.y, r, g, b, 1.0f, -1.0f, -1.0f, -1),
      text_make_vertex(p0.x, p0.y, r, g, b, 1.0f, -1.0f, -1.0f, -1),
      text_make_vertex(p2.x, p2.y, r, g, b, 1.0f, -1.0f, -1.0f, -1),
      text_make_vertex(p3.x, p3.y, r, g, b, 1.0f, -1.0f, -1.0f, -1)};

  text_append_vertices(quad, 6);
}

void draw_textured_quad(APP *app, float x, float y, float w, float h,
                        int textureSlot, float u0, float v0, float u1,
                        float v1) {
  (void)app;

  // Colore bianco pieno: il fragment shader moltiplica texture*colore
  // vertice, bianco = texture mostrata senza tinta, stesso comportamento
  // di batch_add_box per i box texturizzati nel mondo.
  Vertex quad[6] = {
      text_make_vertex(x, y, 1, 1, 1, 1, u0, v0, textureSlot),
      text_make_vertex(x + w, y, 1, 1, 1, 1, u1, v0, textureSlot),
      text_make_vertex(x + w, y + h, 1, 1, 1, 1, u1, v1, textureSlot),
      text_make_vertex(x, y, 1, 1, 1, 1, u0, v0, textureSlot),
      text_make_vertex(x + w, y + h, 1, 1, 1, 1, u1, v1, textureSlot),
      text_make_vertex(x, y + h, 1, 1, 1, 1, u0, v1, textureSlot)};

  text_append_vertices(quad, 6);
}

// Variante con TINTA vera — draw_textured_quad sopra è già usata
// altrove con la firma originale (bianco fisso), non toccata per non
// romperle. Qui invece serve colorare l'icona (verde=attivo,
// grigio=disattivo, ecc.) senza dover ridisegnare la texture in più
// varianti di colore — lo shader moltiplica texture*colore vertice,
// quindi un'icona bianca pura + tinta qui dà lo stesso risultato di
// un'icona pre-colorata, ma con UNA SOLA texture invece di N copie.
void draw_textured_quad_tinted(APP *app, float x, float y, float w,
                               float h, int textureSlot, float u0, float v0,
                               float u1, float v1, float r, float g,
                               float b) {
  (void)app;

  Vertex quad[6] = {
      text_make_vertex(x, y, r, g, b, 1, u0, v0, textureSlot),
      text_make_vertex(x + w, y, r, g, b, 1, u1, v0, textureSlot),
      text_make_vertex(x + w, y + h, r, g, b, 1, u1, v1, textureSlot),
      text_make_vertex(x, y, r, g, b, 1, u0, v0, textureSlot),
      text_make_vertex(x + w, y + h, r, g, b, 1, u1, v1, textureSlot),
      text_make_vertex(x, y + h, r, g, b, 1, u0, v1, textureSlot)};

  text_append_vertices(quad, 6);
}

/* Generic HUD primitives (Animation Lab integration). The HUD accumulator
 * already accepts arbitrary triangles, so lines and circles need no new
 * shader/pipeline: a line is a thin rotated quad, a circle a triangle fan,
 * both flat-colored exactly like draw_quad (textureID -1). They live here
 * -- not in editor code -- because they are renderer-level primitives any
 * HUD caller can use, same status as draw_quad/draw_quad_points. */
void draw_hud_line(APP *app, float x0, float y0, float x1, float y1,
                   float thickness, float r, float g, float b) {
  (void)app;
  float dx = x1 - x0, dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.0001f) {
    draw_quad(app, x0 - thickness * 0.5f, y0 - thickness * 0.5f, thickness,
              thickness, r, g, b);
    return;
  }
  float nx = -dy / len * thickness * 0.5f;
  float ny = dx / len * thickness * 0.5f;
  draw_quad_points(app, (Vec2){x0 + nx, y0 + ny}, (Vec2){x1 + nx, y1 + ny},
                   (Vec2){x1 - nx, y1 - ny}, (Vec2){x0 - nx, y0 - ny}, r, g,
                   b);
}

void draw_hud_tri_rgba(APP *app, Vec2 p0, const float c0[4], Vec2 p1,
                       const float c1[4], Vec2 p2, const float c2[4]) {
  (void)app;
  if (!(c0[3] > 0.0f) && !(c1[3] > 0.0f) && !(c2[3] > 0.0f))
    return;
  Vertex tri[3] = {
      text_make_vertex(p0.x, p0.y, c0[0], c0[1], c0[2], c0[3], -1.0f, -1.0f, -1),
      text_make_vertex(p1.x, p1.y, c1[0], c1[1], c1[2], c1[3], -1.0f, -1.0f, -1),
      text_make_vertex(p2.x, p2.y, c2[0], c2[1], c2[2], c2[3], -1.0f, -1.0f, -1)};
  text_append_vertices(tri, 3);
}

static void hud_quad_rgba_aa(APP *app, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3,
                             float r, float g, float b, float alpha) {
  const Vec2 p[4] = {p0, p1, p2, p3};
  Vec2 n[4], outer[4];
  float area = 0.0f;
  for (int i = 0; i < 4; ++i)
    area += p[i].x * p[(i+1)%4].y - p[(i+1)%4].x * p[i].y;
  if (fabsf(area) < 0.001f) return;
  const float sign = area > 0 ? 1.0f : -1.0f;
  const float fringe = 2.0f / g_hudScale;
  for (int i = 0; i < 4; ++i) {
    const float dx = p[(i+1)%4].x-p[i].x, dy = p[(i+1)%4].y-p[i].y;
    const float length = sqrtf(dx*dx+dy*dy);
    if (length < 0.001f) return;
    n[i] = (Vec2){sign*dy/length, -sign*dx/length};
  }
  for (int i = 0; i < 4; ++i) {
    const Vec2 prev = n[(i+3)%4];
    const float miter = fringe / fmaxf(0.25f, 1.0f+prev.x*n[i].x+prev.y*n[i].y);
    outer[i] = (Vec2){p[i].x+(prev.x+n[i].x)*miter,
                      p[i].y+(prev.y+n[i].y)*miter};
  }
  const float solid[4] = {r,g,b,alpha}, clear[4] = {r,g,b,0};
  draw_hud_tri_rgba(app,p0,solid,p1,solid,p2,solid);
  draw_hud_tri_rgba(app,p0,solid,p2,solid,p3,solid);
  for (int i = 0; i < 4; ++i) {
    const int j = (i+1)%4;
    draw_hud_tri_rgba(app,p[i],solid,p[j],solid,outer[j],clear);
    draw_hud_tri_rgba(app,p[i],solid,outer[j],clear,outer[i],clear);
  }
}

void draw_hud_quad_aa(APP *app, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3,
                      float r, float g, float b) {
  hud_quad_rgba_aa(app,p0,p1,p2,p3,r,g,b,1);
}

void hud_paper_corners(float x, float y, float w, float h, uint32_t seed, Vec2 p[4]) {
  const float cut = fminf(12.0f, fminf(w*0.07f,h*0.14f));
  const float slope = (seed & 1u) ? 0.85f : 0.25f;
  p[0] = (Vec2){x+cut*0.15f,y+cut*slope};
  p[1] = (Vec2){x+w-cut*0.2f,y+cut*(1.0f-slope)};
  p[2] = (Vec2){x+w-cut*0.75f,y+h-cut*0.18f};
  p[3] = (Vec2){x+cut*((seed & 2u) ? 0.9f : 0.4f),y+h-cut*0.72f};
}

void draw_hud_trash_swipe(APP *app, float x, float y, float w, float h, bool focused, uint32_t seed,
                          int textureSlot) {
  if (textureSlot < 0 || w <= 0 || h <= 0)
    return;
  /* sporge dal bottone a destra e in basso (sfalsata come un'ombra),
   * appena inclinata; a riposo velata, col fuoco piena e piu' lunga */
  /* (piu' grande del bottone: dietro la carta se ne vedeva solo un filo) */
  const float grow = focused ? 1.0f : 0.0f;
  const float x0 = x - 26.0f - 10.0f * grow, x1 = x + w + 34.0f + 30.0f * grow;
  const float tilt = ((seed & 1u) ? -1.0f : 1.0f) * h * 0.12f;
  const float y0 = y - h * 0.30f, y1 = y + h + h * 0.55f;
  const Vec2 p[4] = {{x0, y0 + tilt}, {x1, y0 - tilt}, {x1, y1 - tilt}, {x0, y1 + tilt}};
  /* specchiata a meta' dei bottoni: non tutti uguali */
  const bool flip = (seed >> 1) & 1u;
  const Vec2 uv[4] = {{flip ? 1.0f : 0.0f, 0}, {flip ? 0.0f : 1.0f, 0}, {flip ? 0.0f : 1.0f, 1},
                      {flip ? 1.0f : 0.0f, 1}};
  const float a = focused ? 1.0f : 0.75f;
  const float rgba[4] = {1.0f, 1.0f, 1.0f, a};
  draw_textured_quad_uv4(app, p, uv, textureSlot, rgba);
}

void draw_hud_paper_panel(APP *app, float x, float y, float w, float h,
                          const float fill[4], const float accent[3],
                          bool focused, uint32_t seed) {
  if (w <= 0 || h <= 0 || (fill[3] <= 0 && !focused)) return;
  const float border = fminf(6.0f, fminf(w,h)*0.085f);
  Vec2 p[4], inner[4];
  hud_paper_corners(x,y,w,h,seed,p);
  for (int i = 0; i < 4; ++i) {
    inner[i].x = x+border+(p[i].x-x)*(w-2*border)/w;
    inner[i].y = y+border+(p[i].y-y)*(h-2*border)/h;
  }
  if (fill[3] > 0) {
    hud_quad_rgba_aa(app,p[0],p[1],p[2],p[3],0.001f,0.001f,0.001f,fill[3]);
    hud_quad_rgba_aa(app,inner[0],inner[1],inner[2],inner[3],
                     fill[0],fill[1],fill[2],fill[3]);
  }
  if (focused && accent) {
    /* Inset outline, not an opaque plate: transparent portrait buttons
     * must never cover the character image under them. */
    for (int i = 0; i < 4; ++i) {
      const int j = (i+1)%4;
      Vec2 a = inner[i], b = inner[j];
      const float dx = b.x-a.x, dy = b.y-a.y;
      const float len = sqrtf(dx*dx+dy*dy);
      if (len < 0.001f) continue;
      const float thick = fminf(2.8f,border*0.55f);
      Vec2 n = {-dy/len*thick, dx/len*thick};
      draw_hud_quad_aa(app,a,b,(Vec2){b.x+n.x,b.y+n.y},
                       (Vec2){a.x+n.x,a.y+n.y},accent[0],accent[1],accent[2]);
    }
  }
}

void draw_hud_star_badge(APP *app, float x, float y, float radius,
                         const float color[3]) {
  for (int layer = 0; layer < 2; ++layer) {
    const float s = radius*(layer ? 0.68f : 1.0f);
    const float c[4] = {layer ? color[0] : 0.001f,layer ? color[1] : 0.001f,
                        layer ? color[2] : 0.001f,1};
    const float clear[4] = {c[0],c[1],c[2],0};
    Vec2 p[16], edge[16];
    for (int i = 0; i < 16; ++i) {
      const float a = i*6.2831853f/16+0.13f;
      const float r = s*((i&1) ? 0.60f : 1.0f)*(1+0.09f*sinf(i*2.3f));
      p[i] = (Vec2){x+cosf(a)*r,y+sinf(a)*r};
      edge[i] = (Vec2){p[i].x+cosf(a)*1.2f,p[i].y+sinf(a)*1.2f};
    }
    for (int i = 0; i < 16; ++i) {
      const int j = (i+1)%16;
      draw_hud_tri_rgba(app,(Vec2){x,y},c,p[i],c,p[j],c);
      draw_hud_tri_rgba(app,p[i],c,edge[i],clear,edge[j],clear);
      draw_hud_tri_rgba(app,p[i],c,edge[j],clear,p[j],c);
    }
  }
}

void draw_hud_circle(APP *app, float cx, float cy, float radius,
                     bool filled, float thickness, float r, float g,
                     float b) {
  (void)app;
  if (radius <= 0.0f)
    return;
  /* Segment count scales with radius so small joint dots stay cheap and
   * large debug circles stay round. */
  int segments = (int)(radius * 0.8f) + 12;
  if (segments > 48)
    segments = 48;
  float prevX = cx + radius, prevY = cy;
  for (int i = 1; i <= segments; ++i) {
    float angle = (float)i / (float)segments * 6.2831853f;
    float x = cx + cosf(angle) * radius;
    float y = cy + sinf(angle) * radius;
    if (filled) {
      Vertex tri[3] = {
          text_make_vertex(cx, cy, r, g, b, 1.0f, -1.0f, -1.0f, -1),
          text_make_vertex(prevX, prevY, r, g, b, 1.0f, -1.0f, -1.0f, -1),
          text_make_vertex(x, y, r, g, b, 1.0f, -1.0f, -1.0f, -1)};
      text_append_vertices(tri, 3);
    } else {
      draw_hud_line(app, prevX, prevY, x, y, thickness, r, g, b);
    }
    prevX = x;
    prevY = y;
  }
}

/* Rotated/arbitrary textured quad -- the missing piece for bone-attached
 * sprites, whose corners rotate with the bone. Corner order matches
 * draw_quad_points; UVs map p0->(u0,v0), p1->(u1,v0), p2->(u1,v1),
 * p3->(u0,v1). */
void draw_textured_quad_points(APP *app, Vec2 p0, Vec2 p1, Vec2 p2,
                               Vec2 p3, int textureSlot, float u0, float v0,
                               float u1, float v1) {
  (void)app;
  Vertex quad[6] = {
      text_make_vertex(p0.x, p0.y, 1, 1, 1, 1, u0, v0, textureSlot),
      text_make_vertex(p1.x, p1.y, 1, 1, 1, 1, u1, v0, textureSlot),
      text_make_vertex(p2.x, p2.y, 1, 1, 1, 1, u1, v1, textureSlot),
      text_make_vertex(p0.x, p0.y, 1, 1, 1, 1, u0, v0, textureSlot),
      text_make_vertex(p2.x, p2.y, 1, 1, 1, 1, u1, v1, textureSlot),
      text_make_vertex(p3.x, p3.y, 1, 1, 1, 1, u0, v1, textureSlot)};
  text_append_vertices(quad, 6);
}

void draw_textured_quad_uv4(APP *app, const Vec2 p[4], const Vec2 uv[4],
                            int textureSlot, const float rgba[4]) {
  (void)app;
  const float r = rgba[0], g = rgba[1], b = rgba[2], a = rgba[3];
  if (!(a > 0.0f))
    return;
  Vertex quad[6] = {
      text_make_vertex(p[0].x, p[0].y, r, g, b, a, uv[0].x, uv[0].y, textureSlot),
      text_make_vertex(p[1].x, p[1].y, r, g, b, a, uv[1].x, uv[1].y, textureSlot),
      text_make_vertex(p[2].x, p[2].y, r, g, b, a, uv[2].x, uv[2].y, textureSlot),
      text_make_vertex(p[0].x, p[0].y, r, g, b, a, uv[0].x, uv[0].y, textureSlot),
      text_make_vertex(p[2].x, p[2].y, r, g, b, a, uv[2].x, uv[2].y, textureSlot),
      text_make_vertex(p[3].x, p[3].y, r, g, b, a, uv[3].x, uv[3].y, textureSlot)};
  text_append_vertices(quad, 6);
}

void draw_textured_disc(APP *app, float cx, float cy, float radius,
                        int textureSlot, float u0, float v0, float u1,
                        float v1, const float rgba[4]) {
  (void)app;
  if (!(radius > 0.0f) || !(rgba[3] > 0.0f) || textureSlot < 0)
    return;
  enum { SEGMENTS = 48 };
  Vertex fan[SEGMENTS * 3];
  const float uc = (u0 + u1) * 0.5f, vc = (v0 + v1) * 0.5f;
  const float ur = (u1 - u0) * 0.5f, vr = (v1 - v0) * 0.5f;
  for (int i = 0; i < SEGMENTS; ++i) {
    fan[i * 3] = text_make_vertex(cx, cy, rgba[0], rgba[1], rgba[2],
                                  rgba[3], uc, vc, textureSlot);
    for (int j = 0; j < 2; ++j) {
      const float angle = 6.28318530718f * (float)(i + j) / SEGMENTS;
      const float x = cosf(angle), y = sinf(angle);
      fan[i * 3 + 1 + j] = text_make_vertex(
          cx + radius * x, cy + radius * y, rgba[0], rgba[1], rgba[2],
          rgba[3], uc + ur * x, vc + vr * y, textureSlot);
    }
  }
  text_append_vertices(fan, SEGMENTS * 3);
}

void draw_textured_quad_points_alpha(APP *app, Vec2 p0, Vec2 p1, Vec2 p2,
                                     Vec2 p3, int textureSlot, float u0,
                                     float v0, float u1, float v1,
                                     float alpha) {
  (void)app;
  if (!(alpha > 0.0f))
    return;
  if (alpha > 1.0f)
    alpha = 1.0f;
  const float a = alpha;
  Vertex quad[6] = {
      text_make_vertex(p0.x, p0.y, 1, 1, 1, a, u0, v0, textureSlot),
      text_make_vertex(p1.x, p1.y, 1, 1, 1, a, u1, v0, textureSlot),
      text_make_vertex(p2.x, p2.y, 1, 1, 1, a, u1, v1, textureSlot),
      text_make_vertex(p0.x, p0.y, 1, 1, 1, a, u0, v0, textureSlot),
      text_make_vertex(p2.x, p2.y, 1, 1, 1, a, u1, v1, textureSlot),
      text_make_vertex(p3.x, p3.y, 1, 1, 1, a, u0, v1, textureSlot)};
  text_append_vertices(quad, 6);
}
