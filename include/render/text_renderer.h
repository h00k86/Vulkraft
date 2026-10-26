#pragma once

typedef struct APP APP;
#include "math/types.h"
#include "render/font.h"
#include "render/text_anchor.h"   // <-- AGGIUNGI
#include <stdbool.h>
#include <stdint.h>

/* CPU and Vulkan HUD storage start from the same vertex capacity. Both grow
 * geometrically when a legitimate frame exceeds it. */
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
/* Small paper-label tilt/shear, centered on the text baseline. */
void draw_text_tilted(APP *app, Font *font, const char *text, float x,
                      float y, float scale, float degrees,
                      float r, float g, float b, float alpha);

void draw_text_anchored(APP *app, Font *font, const char *text,
                        TextAnchor anchor, float margin, float scale);

float measure_text_width(Font *font, const char *text, float scale);

// faseD.md D2: come draw_quad ma con opacita' (alpha 0..1).
void draw_quad_alpha(APP *app, float x, float y, float w, float h, float r,
                     float g, float b, float a);
void draw_quad(APP *app, float x, float y, float w, float h,
              float r, float g, float b);

// Come draw_quad, ma con una texture vera invece di un colore pieno —
// prima non esisteva un modo di mostrare una texture in overlay HUD
// (draw_quad scrive sempre slot -1 / UV -1,-1, "nessuna texture" per il
// fragment shader). Usa lo stesso buffer/pipeline di testo e draw_quad,
// solo con slot/UV veri. u0/v0/u1/v1 in [0,1] — per mostrare un singolo
// frame di un atlas: u1=1/cols, v1=1/rows (frame in alto a sinistra).
void draw_textured_quad(APP *app, float x, float y, float w, float h,
                        int textureSlot, float u0, float v0, float u1,
                        float v1);

// Quad HUD con i 4 angoli espliciti (per forme ruotate) — stesso
// buffer/pipeline di draw_quad.
void draw_quad_points(APP *app, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3,
                      float r, float g, float b);
/* Convex quad with a coverage fringe, including oblique silhouettes. */
void draw_hud_quad_aa(APP *app, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3,
                      float r, float g, float b);
/* Stable paper-cut silhouettes, bounded by the original layout rectangle. */
void hud_paper_corners(float x, float y, float w, float h, uint32_t seed, Vec2 p[4]);
/* (2026-10-24) la pennellata trash polka DIETRO un bottone a carta: un
 * leggero sbaffo del colore dello stile dell'HUD, sfalsato e inclinato,
 * piu' pieno col fuoco. 'textureSlot' = pennellata_bottone.png della
 * tavolozza (texture_manager_load), -1 = niente */
void draw_hud_trash_swipe(APP *app, float x, float y, float w, float h, bool focused, uint32_t seed,
                          int textureSlot);
void draw_hud_paper_panel(APP *app, float x, float y, float w, float h,
                          const float fill[4], const float accent[3],
                          bool focused, uint32_t seed);
void draw_hud_star_badge(APP *app, float x, float y, float radius,
                         const float color[3]);

// Primitive HUD generiche (integrazione Animation Lab): linea a spessore
// arbitrario (quad ruotato sottile) e cerchio (triangle fan / anello di
// segmenti) — flat color, textureID -1, zero shader nuovi.
void draw_hud_line(APP *app, float x0, float y0, float x1, float y1,
                   float thickness, float r, float g, float b);
/* Schermo diviso: da qui in poi l'HUD si disegna in coordinate dell'intero
 * schermo e finisce in (ox,oy) scalato di 'scale' (anche i ritagli). 0,0,1
 * la toglie. */
void text_hud_transform(float ox, float oy, float scale);

/* Triangolo HUD con colore e alfa PER VERTICE (r,g,b,a ciascuno): sfumature
 * -- aloni dei fasci di mira, dischi a terra -- senza shader nuovi. */
void draw_hud_tri_rgba(APP *app, Vec2 p0, const float c0[4], Vec2 p1,
                       const float c1[4], Vec2 p2, const float c2[4]);
void draw_hud_circle(APP *app, float cx, float cy, float radius,
                     bool filled, float thickness, float r, float g,
                     float b);

// Quad texturizzato con angoli espliciti — sprite attaccati a ossa che
// ruotano. UV: p0->(u0,v0), p1->(u1,v0), p2->(u1,v1), p3->(u0,v1).
void draw_textured_quad_points(APP *app, Vec2 p0, Vec2 p1, Vec2 p2,
                               Vec2 p3, int textureSlot, float u0, float v0,
                               float u1, float v1);

// Come sopra ma con opacita'. Serve alle immagini di riferimento che si
// ricalcano nell'editor: devono stare SOTTO lo scheletro e lasciarlo
// leggibile.
//
// Nessuno shader nuovo: il ramo texturizzato generico di tri.frag calcola
// gia' `texture(...) * fragColor`, alpha compresa. draw_textured_quad_points
// cabla semplicemente alpha = 1, quindi questa variante non cambia nulla per
// i chiamanti esistenti -- espone un parametro che la pipeline onorava gia'.
// Quad texturizzato con UV PER ANGOLO e tinta (rgba): un ritaglio ruotato
// di un'immagine (la minimappa che gira con la camera). p/uv in giro.
void draw_textured_quad_uv4(APP *app, const Vec2 p[4], const Vec2 uv[4],
                            int textureSlot, const float rgba[4]);

/* Circular crop of an atlas cell, with RGB tint and opacity. */
void draw_textured_disc(APP *app, float cx, float cy, float radius,
                        int textureSlot, float u0, float v0, float u1,
                        float v1, const float rgba[4]);

void draw_textured_quad_points_alpha(APP *app, Vec2 p0, Vec2 p1, Vec2 p2,
                                     Vec2 p3, int textureSlot, float u0,
                                     float v0, float u1, float v1,
                                     float alpha);
