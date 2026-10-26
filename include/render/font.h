#ifndef FONT_H
#define FONT_H

#include <stdint.h>

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


typedef struct {

    Glyph glyphs[MAX_GLYPHS];

    uint32_t atlas_width;
    uint32_t atlas_height;
    float ascender;   // <-- nuovo
} Font;


Glyph *font_get(Font *font, uint32_t unicode);

int font_load(Font *font, const char *json_file,
              uint32_t atlas_width,
              uint32_t atlas_height);
int font_load_at(Font *font, const char *json_file, uint32_t atlas_width,
                 uint32_t atlas_height, float offset_x);


#endif
