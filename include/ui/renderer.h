#pragma once
#include "text/font_types.h"
#include <stdbool.h>
#include <stdint.h>

/* Backend sincrono: callback obbligatorie salvo decorazioni e servizi immagine indicati. La tabella viene
 * copiata, user e risorse rimangono del chiamante. Nessun tipo APP o Vulkan. */
typedef struct {
  void (*draw_quad)(void *user, float x, float y, float w, float h, float r, float g, float b);
  void (*draw_quad_alpha)(void *user, float x, float y, float w, float h, float r, float g, float b, float a);
  void (*draw_text_colored)(void *user, Font *font, const char *text, float x, float y, float scale, float r, float g, float b);
  void (*draw_textured_quad)(void *user, float x, float y, float w, float h, int textureSlot, float u0, float v0, float u1, float v1);
  /* Optional skin extensions: NULL uses portable rectangle/image fallbacks. */
  void (*draw_hud_paper_panel)(void *user, float x, float y, float w, float h, const float fill[4], const float accent[3], bool focused, uint32_t seed);
  void (*draw_hud_trash_swipe)(void *user, float x, float y, float w, float h, bool focused, uint32_t seed, int textureSlot);
  /* Scaled baseline ascent in UI units; synchronous, like width measurement. */
  float (*font_ascent)(void *user, Font *font, float scale);
  float (*measure_text_width)(void *user, Font *font, const char *text, float scale);
  /* Failed push/pop must leave the backend clip stack unchanged. */
  bool (*text_clip_push)(void *user, float x, float y, float w, float h);
  bool (*text_clip_pop)(void *user);
  int (*text_clip_depth)(void *user);
  /* Optional queries: absence returns false without changing output dimensions.
   * Preview load/clear must be both present or both absent. Load also requires
   * get_size. Absence means unsupported (-1 / no-op), not a draw-list error. */
  bool (*texture_manager_get_size)(void *user, int slot, int *outW, int *outH);
  int (*texture_manager_preview_load)(void *user, const char *path);
  void (*texture_manager_preview_clear)(void *user);
} UIRenderer;
