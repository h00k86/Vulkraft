#include "backends/ui_sdl2.h"
#include "text/utf8.h"
#include <SDL_image.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GLYPH_CACHE 512
/* Font representation is private to this backend, not the MSDF Font. */
typedef struct {
  FT_UInt index;
  unsigned size;
  SDL_Texture *texture;
  int width,height,left,top;
  float advance;
  bool valid;
} CachedGlyph;
struct Font { FT_Face face; struct UISDL2 *owner; CachedGlyph glyphs[GLYPH_CACHE]; };
struct UISDL2 {
  SDL_Surface *surface,*preview;
  SDL_Renderer *renderer;
  SDL_Texture *preview_texture;
  FT_Library ft;
  Font font;
  SDL_Rect clips[32];
  int depth;
  bool frame_active;
  char error[192],preview_path[4096];
};
static void fail(UISDL2 *b,const char *message) {
  if (!b->error[0]) snprintf(b->error,sizeof(b->error),"%s",message);
}
static void checked(UISDL2 *b,int result) { if(result<0) fail(b,SDL_GetError()); }
static Uint8 color(float v) { return (Uint8)lroundf(fmaxf(0,fminf(1,v))*255); }
/* UI palette stores RGB in linear space (power 2.2 convention). Alpha stays linear. */
static Uint8 rgb(float v) { return color(powf(fmaxf(0,fminf(1,v)),1.0f/2.2f)); }
static bool rect_valid(float x,float y,float w,float h) {
  return isfinite(x)&&isfinite(y)&&isfinite(w)&&isfinite(h)&&
         fabsf(x)<1000000&&fabsf(y)<1000000&&w>=0&&h>=0&&w<1000000&&h<1000000;
}
static bool empty_clip(const UISDL2 *b) {
  return b->depth && (!b->clips[b->depth-1].w || !b->clips[b->depth-1].h);
}
static void quad_alpha(void *user,float x,float y,float w,float h,float r,float g,float bl,float a) {
  UISDL2 *b=user;
  if(b->error[0]||empty_clip(b)) return;
  if(!rect_valid(x,y,w,h)) { fail(b,"Invalid rectangle"); return; }
  SDL_FRect rect={x,y,w,h};
  checked(b,SDL_SetRenderDrawBlendMode(b->renderer,SDL_BLENDMODE_BLEND));
  checked(b,SDL_SetRenderDrawColor(b->renderer,rgb(r),rgb(g),rgb(bl),color(a)));
  checked(b,SDL_RenderFillRectF(b->renderer,&rect));
}
static void quad(void *u,float x,float y,float w,float h,float r,float g,float b) {
  quad_alpha(u,x,y,w,h,r,g,b,1);
}
static bool font_size(UISDL2 *b,Font *font,float scale,unsigned *size) {
  if(b->error[0]) return false;
  if(font!=&b->font || !isfinite(scale) || scale<=0 || scale>256) {
    fail(b,"Invalid font or size (supported: 0 < size <= 256)"); return false;
  }
  *size=(unsigned)fmaxf(1,roundf(scale));
  if(FT_Set_Pixel_Sizes(font->face,0,*size)) { fail(b,"FreeType size failed"); return false; }
  return true;
}
static CachedGlyph *glyph(UISDL2 *b,FT_UInt index,unsigned size) {
  CachedGlyph *g=&b->font.glyphs[(index*31u+size)%GLYPH_CACHE];
  if(g->valid && g->index==index && g->size==size) return g;
  SDL_DestroyTexture(g->texture); *g=(CachedGlyph){0};
  if(FT_Load_Glyph(b->font.face,index,FT_LOAD_RENDER)) { fail(b,"FreeType glyph failed"); return NULL; }
  FT_GlyphSlot slot=b->font.face->glyph;
  g->advance=slot->advance.x/64.0f; g->left=slot->bitmap_left; g->top=slot->bitmap_top;
  g->width=(int)slot->bitmap.width; g->height=(int)slot->bitmap.rows;
  if(g->width && g->height) {
    if(slot->bitmap.pixel_mode!=FT_PIXEL_MODE_GRAY) { fail(b,"Unsupported glyph bitmap format"); return NULL; }
    SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,g->width,g->height,32,SDL_PIXELFORMAT_RGBA32);
    if(!s) { fail(b,SDL_GetError()); return NULL; }
    for(int y=0;y<g->height;++y) {
      int pitch=slot->bitmap.pitch;
      const unsigned char *row=slot->bitmap.buffer+(pitch>=0?y:g->height-1-y)*abs(pitch);
      Uint32 *pixels=(Uint32 *)((unsigned char *)s->pixels+y*s->pitch);
      for(int x=0;x<g->width;++x) pixels[x]=SDL_MapRGBA(s->format,255,255,255,row[x]);
    }
    g->texture=SDL_CreateTextureFromSurface(b->renderer,s); SDL_FreeSurface(s);
    if(!g->texture) { fail(b,SDL_GetError()); return NULL; }
    checked(b,SDL_SetTextureBlendMode(g->texture,SDL_BLENDMODE_BLEND));
  }
  g->index=index; g->size=size; g->valid=true; return g;
}
static float text_run(UISDL2 *b,Font *font,const char *text,float x,float y,float scale,
                      bool paint,float r,float g,float bl) {
  unsigned size;
  if(!text || !font_size(b,font,scale,&size)) return 0;
  float pen=x; FT_UInt previous=0; size_t n=strlen(text);
  while(n && !b->error[0]) {
    uint32_t cp; size_t k=utf8_decode(text,n,&cp); text+=k; n-=k;
    FT_UInt index=FT_Get_Char_Index(font->face,cp);
    if(!index) index=FT_Get_Char_Index(font->face,0xfffd);
    if(!index) index=FT_Get_Char_Index(font->face,'?');
    FT_Vector kern={0};
    if(previous && index && FT_HAS_KERNING(font->face))
      FT_Get_Kerning(font->face,previous,index,FT_KERNING_DEFAULT,&kern);
    pen+=kern.x/64.0f;
    CachedGlyph *item=glyph(b,index,size); if(!item) break;
    if(paint && item->texture && !empty_clip(b)) {
      checked(b,SDL_SetTextureColorMod(item->texture,rgb(r),rgb(g),rgb(bl)));
      SDL_FRect dst={pen+item->left,y-item->top,(float)item->width,(float)item->height};
      checked(b,SDL_RenderCopyF(b->renderer,item->texture,NULL,&dst));
    }
    pen+=item->advance; previous=index;
  }
  return pen-x;
}
static void text(void *u,Font *font,const char *s,float x,float y,float scale,float r,float g,float b) {
  UISDL2 *backend=u;
  if(!isfinite(x)||!isfinite(y)) { fail(backend,"Invalid text position"); return; }
  text_run(backend,font,s,x,y,scale,true,r,g,b);
}
static float measure(void *u,Font *font,const char *s,float scale) {
  return text_run(u,font,s,0,0,scale,false,1,1,1);
}
static float ascent(void *u,Font *font,float scale) {
  UISDL2 *b=u; unsigned size;
  return font_size(b,font,scale,&size)?font->face->size->metrics.ascender/64.0f:0;
}
static bool clip_push(void *u,float x,float y,float w,float h) {
  UISDL2 *b=u;
  if(b->error[0]||b->depth==32||!rect_valid(x,y,w,h)||w<=0||h<=0) return false;
  SDL_Rect rect={(int)floorf(x),(int)floorf(y),(int)ceilf(x+w)-(int)floorf(x),(int)ceilf(y+h)-(int)floorf(y)};
  SDL_Rect parent=b->depth?b->clips[b->depth-1]:(SDL_Rect){0,0,b->surface->w,b->surface->h};
  SDL_Rect clipped={0}; if(!SDL_IntersectRect(&rect,&parent,&clipped)) clipped=(SDL_Rect){0};
  if(SDL_RenderSetClipRect(b->renderer,&clipped)<0) { fail(b,SDL_GetError()); return false; }
  b->clips[b->depth++]=clipped; return true;
}
static bool clip_pop(void *u) {
  UISDL2 *b=u;
  if(!b->depth) return false;
  if(SDL_RenderSetClipRect(b->renderer,b->depth>1?&b->clips[b->depth-2]:NULL)<0) {
    fail(b,SDL_GetError()); return false;
  }
  --b->depth; return true;
}
static int clip_depth(void *u) { return ((UISDL2 *)u)->depth; }
static void preview_clear(void *u) {
  UISDL2 *b=u; SDL_DestroyTexture(b->preview_texture); SDL_FreeSurface(b->preview);
  b->preview_texture=NULL; b->preview=NULL; b->preview_path[0]=0;
}
static int preview_load(void *u,const char *path) {
  UISDL2 *b=u;
  if(b->error[0]||!path||strlen(path)>=sizeof(b->preview_path)) return -1;
  if(b->preview && !strcmp(path,b->preview_path)) return 0;
  SDL_Surface *s=IMG_Load(path); if(!s) return -1;
  preview_clear(b); b->preview=s; snprintf(b->preview_path,sizeof(b->preview_path),"%s",path);
  return 0;
}
static bool image_size(void *u,int slot,int *w,int *h) {
  UISDL2 *b=u;
  if(slot!=0||!b->preview||!w||!h) return false;
  *w=b->preview->w; *h=b->preview->h; return true;
}
static void image(void *u,float x,float y,float w,float h,int slot,float u0,float v0,float u1,float v1) {
  UISDL2 *b=u;
  if(b->error[0]||empty_clip(b)) return;
  if(slot!=0||!b->preview||!rect_valid(x,y,w,h)||!isfinite(u0)||!isfinite(u1)||
     !isfinite(v0)||!isfinite(v1)||u0<0||v0<0||u1>1||v1>1||u1<u0||v1<v0) {
    fail(b,"Invalid image/UV"); return;
  }
  if(!b->preview_texture) {
    b->preview_texture=SDL_CreateTextureFromSurface(b->renderer,b->preview);
    if(!b->preview_texture) { fail(b,SDL_GetError()); return; }
  }
  SDL_Rect src={(int)(u0*b->preview->w),(int)(v0*b->preview->h),
               (int)((u1-u0)*b->preview->w),(int)((v1-v0)*b->preview->h)};
  SDL_FRect dst={x,y,w,h}; checked(b,SDL_RenderCopyF(b->renderer,b->preview_texture,&src,&dst));
}
static const UIRenderer callbacks={
  .draw_quad=quad,.draw_quad_alpha=quad_alpha,.draw_text_colored=text,.draw_textured_quad=image,
  .font_ascent=ascent,.measure_text_width=measure,.text_clip_push=clip_push,
  .text_clip_pop=clip_pop,.text_clip_depth=clip_depth,.texture_manager_get_size=image_size,
  .texture_manager_preview_load=preview_load,.texture_manager_preview_clear=preview_clear
};
static void clear_textures(UISDL2 *b) {
  for(int i=0;i<GLYPH_CACHE;++i) {
    SDL_DestroyTexture(b->font.glyphs[i].texture); b->font.glyphs[i]=(CachedGlyph){0};
  }
  SDL_DestroyTexture(b->preview_texture); b->preview_texture=NULL;
}
bool ui_sdl2_resize(UISDL2 *b,int w,int h) {
  if(!b||w<=0||h<=0||w>8192||h>8192||b->frame_active||b->depth) return false;
  if(b->surface && b->surface->w==w && b->surface->h==h) return true;
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32);
  if(!s) return false;
  SDL_Renderer *r=SDL_CreateSoftwareRenderer(s);
  if(!r) { SDL_FreeSurface(s); return false; }
  clear_textures(b); SDL_DestroyRenderer(b->renderer); SDL_FreeSurface(b->surface);
  b->renderer=r; b->surface=s; return true;
}
UISDL2 *ui_sdl2_create(int w,int h,const char *path) {
  UISDL2 *b=calloc(1,sizeof(*b)); if(!b) return NULL;
  b->font.owner=b;
  if(!path||FT_Init_FreeType(&b->ft)||FT_New_Face(b->ft,path,0,&b->font.face)||
     FT_Select_Charmap(b->font.face,FT_ENCODING_UNICODE)||!ui_sdl2_resize(b,w,h)) {
    ui_sdl2_destroy(b); return NULL;
  }
  return b;
}
void ui_sdl2_destroy(UISDL2 *b) {
  if(!b) return;
  clear_textures(b); preview_clear(b); SDL_DestroyRenderer(b->renderer); SDL_FreeSurface(b->surface);
  if(b->font.face) FT_Done_Face(b->font.face);
  if(b->ft) FT_Done_FreeType(b->ft);
  free(b);
}
bool ui_sdl2_begin(UISDL2 *b) {
  if(!b||b->frame_active||b->depth) return false;
  b->error[0]=0;
  checked(b,SDL_RenderSetClipRect(b->renderer,NULL));
  checked(b,SDL_SetRenderDrawColor(b->renderer,20,23,29,255));
  checked(b,SDL_RenderClear(b->renderer));
  b->frame_active=!b->error[0]; return b->frame_active;
}
bool ui_sdl2_end(UISDL2 *b) {
  if(!b||!b->frame_active||b->depth||b->error[0]) return false;
  SDL_RenderPresent(b->renderer); b->frame_active=false; return true;
}
void ui_sdl2_discard(UISDL2 *b) {
  if(!b) return;
  /* Preserve the failure for inspection until the next begin. The discarded
   * surface must not be presented by the host; begin clears it before reuse. */
  checked(b,SDL_RenderSetClipRect(b->renderer,NULL));
  b->depth=0;
  b->frame_active=false;
}
const UIRenderer *ui_sdl2_renderer(void) { return &callbacks; }
Font *ui_sdl2_font(UISDL2 *b) { return b?&b->font:NULL; }
SDL_Surface *ui_sdl2_surface(UISDL2 *b) { return b?b->surface:NULL; }
const char *ui_sdl2_error(const UISDL2 *b) { return b?b->error:"No backend"; }
