#pragma once
#include "ui/ui.h"

/* Private paint facade. The core resolves the current context and renderer;
 * drawing operations expose neither context state nor a concrete backend. */
void ui_paint_draw_quad(float x, float y, float w, float h, float r, float g, float b);
void ui_paint_draw_text_colored(Font *font, const char *text, float x, float y, float scale, float r, float g, float b);
void ui_paint_draw_textured_quad(float x, float y, float w, float h, int textureSlot, float u0, float v0, float u1, float v1);
float ui_paint_measure_text_width(Font *font, const char *text, float scale);
bool ui_paint_texture_manager_get_size(int slot, int *outW, int *outH);
int ui_paint_texture_manager_preview_load(const char *path);
float ui_paint_font_ascent(Font *font, float scale);

/* Text fitting shares the renderer's measurement and UTF-8 boundary rules. */
void ui_ellipsize_left(Font *font,const char *text,float scale,
                       float max_width,char *out,size_t out_size);
void ui_ellipsize_right(Font *font,const char *text,float scale,
                        float max_width,char *out,size_t out_size);

void ui_paint_draw_quad_alpha(float x,float y,float w,float h,float r,float g,float b,float a);
void ui_paint_draw_hud_paper_panel(float x,float y,float w,float h,
                                  const float fill[4],const float accent[3],bool focused,uint32_t seed);

void ui_paint_draw_hud_trash_swipe(float x,float y,float w,float h,bool focused,uint32_t seed,int textureSlot);
bool ui_paint_text_clip_push(float x,float y,float w,float h);
bool ui_paint_text_clip_pop(void);
