#include "render/font.h"
#include "render/gfx_vulkan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Glyph *font_get(Font *font, uint32_t unicode) {
  if (unicode >= MAX_GLYPHS)
    return NULL;

  if (font->glyphs[unicode].unicode == 0)
    return NULL;

  return &font->glyphs[unicode];
}

int font_load(Font *font, const char *json_file, uint32_t atlas_width,
              uint32_t atlas_height) {
  return font_load_at(font, json_file, atlas_width, atlas_height, 0.0f);
}

/* 2026-10-16: due font nella STESSA texture (il testo usa sempre FONT_SLOT,
 * l'unico percorso MSDF dello shader): questo sta a offset_x pixel da
 * sinistra (src/fonts/font_atlas.png: font2 a sinistra, il fumetto a destra) */
int font_load_at(Font *font, const char *json_file, uint32_t atlas_width,
                 uint32_t atlas_height, float offset_x) {
  FILE *f = fopen(json_file, "rb");
  if (!f)
    return 0;

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  rewind(f);
  char *data = malloc(size + 1);
  fread(data, 1, size, f);
  data[size] = 0;
  fclose(f);

  font->atlas_width = atlas_width;
  font->atlas_height = atlas_height;

  // --- Lettura ascender dai metrics globali del font ---
  font->ascender = 0.93f; // default di sicurezza

  char *metrics = strstr(data, "\"metrics\"");
  if (metrics) {
    char *asc = strstr(metrics, "\"ascender\"");
    if (asc) {
      sscanf(asc, "\"ascender\":%f", &font->ascender);
    }
  }

  char *p = data;

  while ((p = strstr(p, "\"unicode\"")) != NULL) {
    Glyph g = {0};
    sscanf(p, "\"unicode\":%u", &g.unicode);

    char *next_entry = strstr(p + 9, "\"unicode\"");
    char *obj_end = next_entry ? next_entry : (data + size);

    char *advance = strstr(p, "\"advance\"");
    if (advance && advance < obj_end) {
      sscanf(advance, "\"advance\":%f", &g.advance);
    }

    char *plane = strstr(p, "\"planeBounds\"");
    char *atlas = strstr(p, "\"atlasBounds\"");

    if (plane && plane < obj_end && atlas && atlas < obj_end) {
      sscanf(
          plane,
          "\"planeBounds\":{\"left\":%f,\"bottom\":%f,\"right\":%f,\"top\":%f}",
          &g.plane_left, &g.plane_bottom, &g.plane_right, &g.plane_top);

      float left, bottom, right, top;
      sscanf(
          atlas,
          "\"atlasBounds\":{\"left\":%f,\"bottom\":%f,\"right\":%f,\"top\":%f}",
          &left, &bottom, &right, &top);

      if (right - left > 0.0f) {
        g.u0 = (left + offset_x) / (float)atlas_width;
        g.u1 = (right + offset_x) / (float)atlas_width;

        g.v0 = 1.0f - (top / (float)atlas_height);
        g.v1 = 1.0f - (bottom / (float)atlas_height);
      }
    }

    if (g.unicode < MAX_GLYPHS) {
      font->glyphs[g.unicode] = g;
    }

    p += 9;
  }

  free(data);
  return 1;
}
