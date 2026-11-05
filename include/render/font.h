#ifndef FONT_H
#define FONT_H

#include <stdint.h>
#include "text/font_types.h"

#define MAX_GLYPHS 512


typedef struct {

    uint32_t unicode;

    // avanzamento cursore
    float advance;


    // posizione del carattere nello spazio
    // (planeBounds del JSON)
    float plane_left;
    float plane_bottom;
    float plane_right;
    float plane_top;


    // coordinate texture atlas
    float u0;
    float v0;
    float u1;
    float v1;

} Glyph;


struct Font {

    /* Sorted by Unicode scalar, not indexed by codepoint. */
    Glyph glyphs[MAX_GLYPHS];
    uint32_t glyph_count;

    uint32_t atlas_width;
    uint32_t atlas_height;
    float ascender;   // <-- nuovo
};


Glyph *font_get(Font *font, uint32_t unicode);
/* Exact glyph, then U+FFFD, then question mark. */
Glyph *font_get_fallback(Font *font, uint32_t unicode);

int font_load(Font *font, const char *json_file,
              uint32_t atlas_width,
              uint32_t atlas_height);
int font_load_at(Font *font, const char *json_file, uint32_t atlas_width,
                 uint32_t atlas_height, float offset_x);


#endif
