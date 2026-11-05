#include "ui/draw_list.h"
#include "ui_renderer_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#define UI_LIST_MAX_COMMANDS 65536u
#define UI_LIST_MAX_TEXT_BYTES (16u * 1024u * 1024u)
#define UI_LIST_MAX_CLIPS 31
/* Commands contain values, borrowed fonts and offsets into the owned text arena.
 * Offsets survive relocation of either the command buffer or the arena. */
typedef enum {
  CMD_DRAW_QUAD,
  CMD_DRAW_QUAD_ALPHA,
  CMD_DRAW_TEXT_COLORED,
  CMD_DRAW_TEXTURED_QUAD,
  CMD_DRAW_HUD_PAPER_PANEL,
  CMD_DRAW_HUD_TRASH_SWIPE,
  CMD_TEXT_CLIP_PUSH,
  CMD_TEXT_CLIP_POP
} UICommandKind;
typedef struct {
  UICommandKind kind;
  union {
    struct {
      float x;
      float y;
      float w;
      float h;
      float r;
      float g;
      float b;
    } draw_quad;
    struct {
      float x;
      float y;
      float w;
      float h;
      float r;
      float g;
      float b;
      float a;
    } draw_quad_alpha;
    struct {
      Font *font;
      size_t text_offset;
      float x;
      float y;
      float scale;
      float r;
      float g;
      float b;
    } draw_text_colored;
    struct {
      float x;
      float y;
      float w;
      float h;
      int textureSlot;
      float u0;
      float v0;
      float u1;
      float v1;
    } draw_textured_quad;
    struct {
      float x;
      float y;
      float w;
      float h;
      float fill[4];
      float accent[3];
      bool focused;
      uint32_t seed;
    } draw_hud_paper_panel;
    struct {
      float x;
      float y;
      float w;
      float h;
      bool focused;
      uint32_t seed;
      int textureSlot;
    } draw_hud_trash_swipe;
    struct {
      float x;
      float y;
      float w;
      float h;
    } text_clip_push;
    struct {
      int unused;
    } text_clip_pop;
  } data;
} UICommand;
struct UIDrawList {
  UIRenderer target;
  void *user;
  UICommand *commands;
  size_t count, capacity, text_bytes;
  char *text;
  size_t text_capacity;
  int base_depth, depth;
  UIDrawListError error;
  bool recording, replaying, images_recorded, images_replayed, pending_clear;
};
/* Preserve the first failure until the caller explicitly starts a new frame. */
static void list_fail(UIDrawList *list, UIDrawListError error) {
  if (list && list->error==UI_DRAW_LIST_OK) list->error=error;
}
static void discard_commands(UIDrawList *list) {
  list->count=0;
  list->text_bytes=0;
  list->images_recorded=false;
}
static void discard(UIDrawList *list) {
  discard_commands(list);
  list->images_replayed=false;
  if (list->pending_clear) {
    list->target.texture_manager_preview_clear(list->user);
    list->pending_clear=false;
  }
}
static UICommand *append(UIDrawList *list, UICommandKind kind) {
  if (!list || list->error) return NULL;
  if (!list->recording || list->replaying) { list_fail(list,UI_DRAW_LIST_STATE); return NULL; }
  if (list->count==UI_LIST_MAX_COMMANDS) { list_fail(list,UI_DRAW_LIST_LIMIT); return NULL; }
  if (list->count==list->capacity) {
    size_t capacity=list->capacity ? list->capacity*2 : 64;
    UICommand *commands=realloc(list->commands,capacity*sizeof(*commands));
    if (!commands) { list_fail(list,UI_DRAW_LIST_MEMORY); return NULL; }
    list->commands=commands; list->capacity=capacity;
  }
  UICommand *command=&list->commands[list->count++];
  memset(command,0,sizeof(*command)); command->kind=kind;
  return command;
}
static void list_record_draw_quad(void *user, float x, float y, float w, float h, float r, float g, float b) {
  UIDrawList *list=user;
  UICommand *c=append(list,CMD_DRAW_QUAD);
  if (!c) return;
  c->data.draw_quad.x=x;
  c->data.draw_quad.y=y;
  c->data.draw_quad.w=w;
  c->data.draw_quad.h=h;
  c->data.draw_quad.r=r;
  c->data.draw_quad.g=g;
  c->data.draw_quad.b=b;
}
static void list_record_draw_quad_alpha(void *user, float x, float y, float w, float h, float r, float g, float b, float a) {
  UIDrawList *list=user;
  UICommand *c=append(list,CMD_DRAW_QUAD_ALPHA);
  if (!c) return;
  c->data.draw_quad_alpha.x=x;
  c->data.draw_quad_alpha.y=y;
  c->data.draw_quad_alpha.w=w;
  c->data.draw_quad_alpha.h=h;
  c->data.draw_quad_alpha.r=r;
  c->data.draw_quad_alpha.g=g;
  c->data.draw_quad_alpha.b=b;
  c->data.draw_quad_alpha.a=a;
}
static void list_record_draw_text_colored(void *user, Font *font, const char *text, float x, float y, float scale, float r, float g, float b) {
  UIDrawList *list=user;
  UICommand *c=append(list,CMD_DRAW_TEXT_COLORED);
  if (!c) return;
  c->data.draw_text_colored.font=font;
  size_t length=strlen(text)+1;
  if (length>UI_LIST_MAX_TEXT_BYTES-list->text_bytes) { list_fail(list,UI_DRAW_LIST_LIMIT); return; }
  size_t needed=list->text_bytes+length;
  if (needed>list->text_capacity) {
    size_t capacity=list->text_capacity ? list->text_capacity : 256;
    while (capacity<needed) capacity*=2; /* bounded above by 16 MiB */
    char *arena=realloc(list->text,capacity);
    if (!arena) { list_fail(list,UI_DRAW_LIST_MEMORY); return; }
    list->text=arena;
    list->text_capacity=capacity;
  }
  c->data.draw_text_colored.text_offset=list->text_bytes;
  memcpy(list->text+list->text_bytes,text,length);
  list->text_bytes+=length;
  c->data.draw_text_colored.x=x;
  c->data.draw_text_colored.y=y;
  c->data.draw_text_colored.scale=scale;
  c->data.draw_text_colored.r=r;
  c->data.draw_text_colored.g=g;
  c->data.draw_text_colored.b=b;
}
static void list_record_draw_textured_quad(void *user, float x, float y, float w, float h, int textureSlot, float u0, float v0, float u1, float v1) {
  UIDrawList *list=user;
  UICommand *c=append(list,CMD_DRAW_TEXTURED_QUAD);
  if (!c) return;
  c->data.draw_textured_quad.x=x;
  c->data.draw_textured_quad.y=y;
  c->data.draw_textured_quad.w=w;
  c->data.draw_textured_quad.h=h;
  c->data.draw_textured_quad.textureSlot=textureSlot;
  c->data.draw_textured_quad.u0=u0;
  c->data.draw_textured_quad.v0=v0;
  c->data.draw_textured_quad.u1=u1;
  c->data.draw_textured_quad.v1=v1;
  list->images_recorded=true;
}
static void list_record_draw_hud_paper_panel(void *user, float x, float y, float w, float h, const float fill[4], const float accent[3], bool focused, uint32_t seed) {
  UIDrawList *list=user;
  UICommand *c=append(list,CMD_DRAW_HUD_PAPER_PANEL);
  if (!c) return;
  c->data.draw_hud_paper_panel.x=x;
  c->data.draw_hud_paper_panel.y=y;
  c->data.draw_hud_paper_panel.w=w;
  c->data.draw_hud_paper_panel.h=h;
  memcpy(c->data.draw_hud_paper_panel.fill,fill,sizeof(c->data.draw_hud_paper_panel.fill));
  memcpy(c->data.draw_hud_paper_panel.accent,accent,sizeof(c->data.draw_hud_paper_panel.accent));
  c->data.draw_hud_paper_panel.focused=focused;
  c->data.draw_hud_paper_panel.seed=seed;
}
static void list_record_draw_hud_trash_swipe(void *user, float x, float y, float w, float h, bool focused, uint32_t seed, int textureSlot) {
  UIDrawList *list=user;
  UICommand *c=append(list,CMD_DRAW_HUD_TRASH_SWIPE);
  if (!c) return;
  c->data.draw_hud_trash_swipe.x=x;
  c->data.draw_hud_trash_swipe.y=y;
  c->data.draw_hud_trash_swipe.w=w;
  c->data.draw_hud_trash_swipe.h=h;
  c->data.draw_hud_trash_swipe.focused=focused;
  c->data.draw_hud_trash_swipe.seed=seed;
  c->data.draw_hud_trash_swipe.textureSlot=textureSlot;
  list->images_recorded=true;
}
static bool list_record_text_clip_push(void *user, float x, float y, float w, float h) {
  UIDrawList *list=user;
  if (!list || !isfinite(x) || !isfinite(y) || !isfinite(w) || !isfinite(h) || w<=0 || h<=0 || list->depth>=UI_LIST_MAX_CLIPS) {
    if (list) list_fail(list,UI_DRAW_LIST_CLIP);
    return false;
  }
  UICommand *c=append(list,CMD_TEXT_CLIP_PUSH);
  if (!c) return false;
  c->data.text_clip_push.x=x;
  c->data.text_clip_push.y=y;
  c->data.text_clip_push.w=w;
  c->data.text_clip_push.h=h;
  ++list->depth; return true;
}
static bool list_record_text_clip_pop(void *user) {
  UIDrawList *list=user;
  if (!list || list->depth<=0) { if (list) list_fail(list,UI_DRAW_LIST_CLIP); return false; }
  UICommand *c=append(list,CMD_TEXT_CLIP_POP);
  if (!c) return false;
  --list->depth; return true;
}
static float list_record_font_ascent(void *user, Font *font, float scale) {
  UIDrawList *list=user;
  return list->target.font_ascent(list->user,font,scale);
}
static float list_record_measure_text_width(void *user, Font *font, const char *text, float scale) {
  UIDrawList *list=user;
  return list->target.measure_text_width(list->user,font,text,scale);
}
static int list_record_text_clip_depth(void *user) {
  UIDrawList *list=user;
  return list->base_depth+list->depth;
}
static bool list_record_texture_manager_get_size(void *user,int slot,int *w,int *h) {
  UIDrawList *list=user;
  return list->target.texture_manager_get_size &&
         list->target.texture_manager_get_size(list->user,slot,w,h);
}
static int list_record_texture_manager_preview_load(void *user,const char *path) {
  UIDrawList *list=user;
  if (!list || list->error) return -1;
  if (!list->target.texture_manager_preview_load) return -1;
  /* Replacing the single preview slot would invalidate queued image commands. */
  if (list->images_recorded || list->images_replayed || list->pending_clear) { list_fail(list,UI_DRAW_LIST_RESOURCE); return -1; }
  return list->target.texture_manager_preview_load(list->user,path);
}
static void list_record_texture_manager_preview_clear(void *user) {
  UIDrawList *list=user;
  if (!list->target.texture_manager_preview_clear) return;
  if (list->images_recorded || list->images_replayed) list->pending_clear=true;
  else list->target.texture_manager_preview_clear(list->user);
}
const UIRenderer *ui_draw_list_renderer(void) {
  static const UIRenderer renderer={
    .draw_quad=list_record_draw_quad,
    .draw_quad_alpha=list_record_draw_quad_alpha,
    .draw_text_colored=list_record_draw_text_colored,
    .draw_textured_quad=list_record_draw_textured_quad,
    .draw_hud_paper_panel=list_record_draw_hud_paper_panel,
    .draw_hud_trash_swipe=list_record_draw_hud_trash_swipe,
    .measure_text_width=list_record_measure_text_width,
    .font_ascent=list_record_font_ascent,
    .text_clip_push=list_record_text_clip_push,
    .text_clip_pop=list_record_text_clip_pop,
    .text_clip_depth=list_record_text_clip_depth,
    .texture_manager_get_size=list_record_texture_manager_get_size,
    .texture_manager_preview_load=list_record_texture_manager_preview_load,
    .texture_manager_preview_clear=list_record_texture_manager_preview_clear,
  };
  return &renderer;
}
UIDrawList *ui_draw_list_create(const UIRenderer *target,void *user) {
  if (!ui_renderer_valid(target)) return NULL;
  UIDrawList *list=calloc(1,sizeof(*list));
  if (list) { list->target=*target; list->user=user; }
  return list;
}
void ui_draw_list_destroy(UIDrawList *list) {
  if (!list) return;
  discard(list); free(list->commands); free(list->text); free(list);
}
bool ui_draw_list_discard(UIDrawList *list) {
  if (!list || list->replaying) return false;
  discard(list);
  list->recording=false;
  list->depth=0;
  /* Recorded clips never changed the target; do not pop its own clips. */
  return true;
}
bool ui_draw_list_begin(UIDrawList *list) {
  if (!list || list->replaying) return false;
  discard(list);
  list->error=UI_DRAW_LIST_OK;
  list->depth=0;
  list->base_depth=list->target.text_clip_depth(list->user);
  if (list->base_depth<0 || list->base_depth>INT_MAX-UI_LIST_MAX_CLIPS) {
    list_fail(list,UI_DRAW_LIST_CLIP); list->recording=false; return false;
  }
  list->recording=true;
  return true;
}
UIDrawListError ui_draw_list_error(const UIDrawList *list) {
  return list ? list->error : UI_DRAW_LIST_STATE;
}
size_t ui_draw_list_count(const UIDrawList *list) { return list ? list->count : 0; }
bool ui_draw_list_replay(UIDrawList *list) {
  if (!list || list->error) return false;
  if (!list->recording || list->replaying) { list_fail(list,UI_DRAW_LIST_STATE); return false; }
  if (list->depth || list->target.text_clip_depth(list->user)!=list->base_depth) {
    list_fail(list,UI_DRAW_LIST_CLIP); return false;
  }
  list->replaying=true; list->recording=false;
  int pushed=0;
  for (size_t i=0;i<list->count && !list->error;++i) {
    UICommand *c=&list->commands[i];
    switch(c->kind) {
    case CMD_DRAW_QUAD:
      list->target.draw_quad(list->user, c->data.draw_quad.x, c->data.draw_quad.y, c->data.draw_quad.w, c->data.draw_quad.h, c->data.draw_quad.r, c->data.draw_quad.g, c->data.draw_quad.b);
      break;
    case CMD_DRAW_QUAD_ALPHA:
      list->target.draw_quad_alpha(list->user, c->data.draw_quad_alpha.x, c->data.draw_quad_alpha.y, c->data.draw_quad_alpha.w, c->data.draw_quad_alpha.h, c->data.draw_quad_alpha.r, c->data.draw_quad_alpha.g, c->data.draw_quad_alpha.b, c->data.draw_quad_alpha.a);
      break;
    case CMD_DRAW_TEXT_COLORED:
      list->target.draw_text_colored(list->user, c->data.draw_text_colored.font, list->text+c->data.draw_text_colored.text_offset, c->data.draw_text_colored.x, c->data.draw_text_colored.y, c->data.draw_text_colored.scale, c->data.draw_text_colored.r, c->data.draw_text_colored.g, c->data.draw_text_colored.b);
      break;
    case CMD_DRAW_TEXTURED_QUAD:
      list->target.draw_textured_quad(list->user, c->data.draw_textured_quad.x, c->data.draw_textured_quad.y, c->data.draw_textured_quad.w, c->data.draw_textured_quad.h, c->data.draw_textured_quad.textureSlot, c->data.draw_textured_quad.u0, c->data.draw_textured_quad.v0, c->data.draw_textured_quad.u1, c->data.draw_textured_quad.v1);
      break;
    case CMD_DRAW_HUD_PAPER_PANEL:
      ui_renderer_paper(&list->target, list->user, c->data.draw_hud_paper_panel.x, c->data.draw_hud_paper_panel.y, c->data.draw_hud_paper_panel.w, c->data.draw_hud_paper_panel.h, c->data.draw_hud_paper_panel.fill, c->data.draw_hud_paper_panel.accent, c->data.draw_hud_paper_panel.focused, c->data.draw_hud_paper_panel.seed);
      break;
    case CMD_DRAW_HUD_TRASH_SWIPE:
      ui_renderer_swipe(&list->target, list->user, c->data.draw_hud_trash_swipe.x, c->data.draw_hud_trash_swipe.y, c->data.draw_hud_trash_swipe.w, c->data.draw_hud_trash_swipe.h, c->data.draw_hud_trash_swipe.focused, c->data.draw_hud_trash_swipe.seed, c->data.draw_hud_trash_swipe.textureSlot);
      break;
    case CMD_TEXT_CLIP_PUSH:
      if (!list->target.text_clip_push(list->user, c->data.text_clip_push.x, c->data.text_clip_push.y, c->data.text_clip_push.w, c->data.text_clip_push.h)) list_fail(list,UI_DRAW_LIST_CLIP); else ++pushed;
      break;
    case CMD_TEXT_CLIP_POP:
      if (!list->target.text_clip_pop(list->user)) list_fail(list,UI_DRAW_LIST_CLIP); else --pushed;
      break;
    }
  }
  while(pushed>0) { if (!list->target.text_clip_pop(list->user)) break; --pushed; }
  list->replaying=false;
  bool ok=list->error==UI_DRAW_LIST_OK;
  list->images_replayed=list->images_recorded;
  discard_commands(list);
  return ok;
}
