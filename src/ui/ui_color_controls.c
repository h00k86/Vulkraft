#include "ui_color_internal.h"
#include "ui_paint_internal.h"
#include "ui_widget_internal.h"
#include "ui_navigation_internal.h"
#include "ui_overlay_internal.h"
#include "ui_state_sliders_internal.h"
#include "ui_state_frame_internal.h"
#include "ui_state_colors_internal.h"
#include "ui_error_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static float unit(float x) { return isfinite(x)?fmaxf(0,fminf(1,x)):0; }
static bool equal(UIColor a,UIColor b) { return a.r==b.r&&a.g==b.g&&a.b==b.b; }
static UIColorMemory *memory(const char *id,UIColor color) {
  UIContext *ctx=ui_overlay_context(); UIColorsState *state=ui_state_colors(ctx);
  unsigned long long frame=ui_state_frame(ctx)->g_ui_frame_serial;
  UIColorMemory *entry=NULL;
  for(int i=0;i<state->count;++i) if(!strcmp(state->entries[i].id,id)) { entry=&state->entries[i]; break; }
  if(!entry) {
    if(state->count<UI_COLOR_MEMORY_CAPACITY) entry=&state->entries[state->count++];
    else for(int i=0;i<state->count;++i)
      if(state->entries[i].last_seen_frame!=frame&&(!entry||state->entries[i].last_seen_frame<entry->last_seen_frame)) entry=&state->entries[i];
    if(!entry) { ui_report_error(ctx,UI_ERROR_LIMIT,UI_OPERATION_COLOR_PICKER,id); return NULL; }
    memset(entry,0,sizeof(*entry)); snprintf(entry->id,sizeof(entry->id),"%s",id);
    entry->hsv=ui_color_to_hsv(color); entry->last=color;
  } else if(!equal(entry->last,color)) {
    UIColorHSV converted=ui_color_to_hsv(color);
    if(converted.v>0&&converted.s>0) entry->hsv.h=converted.h;
    if(converted.v>0) entry->hsv.s=converted.s;
    entry->hsv.v=converted.v;
  }
  entry->last_seen_frame=frame; return entry;
}

bool ui_color_hsv_begin(const char *id,UIColor color,UIColorHSV *hsv) {
  UIColorMemory *entry=memory(id,color); if(!entry) return false;
  *hsv=entry->hsv; return true;
}
void ui_color_hsv_end(const char *id,UIColor color,UIColorHSV hsv) {
  UIColorsState *state=ui_state_colors(ui_overlay_context());
  for(int i=0;i<state->count;++i) if(!strcmp(state->entries[i].id,id)) {
    state->entries[i].last=color; state->entries[i].hsv=hsv; return;
  }
}
bool ui_color_sv_pad(UIPropertyLayout *layout,UIColorHSV *hsv,bool *started) {
  UIContext *ctx=ui_overlay_context(); UITheme theme=ui_get_theme();
  UIRect r={layout->x,layout->cursor_y,layout->width,120*layout->scale};
  layout->cursor_y+=r.h+theme.row_spacing*layout->scale;
  const char *id=ui_scoped_id("sv"); UISliderState *state=ui_slider_state(id);
  if(!state) return false;
  bool hover=ui_pointer_hit(r.x,r.y,r.w,r.h);
  ui_nav_register(ctx,id,r,UI_NAV_SLIDER,true,hover);
  if(hover) ui_state_frame(ctx)->g_any_hover=true;
  UIColorHSV old=*hsv;
  if(ui_nav_key(ctx,id,PLATFORM_KEY_LEFT,true)) hsv->s=unit(hsv->s-.01f);
  if(ui_nav_key(ctx,id,PLATFORM_KEY_RIGHT,true)) hsv->s=unit(hsv->s+.01f);
  if(ui_nav_key(ctx,id,PLATFORM_KEY_UP,true)) hsv->v=unit(hsv->v+.01f);
  if(ui_nav_key(ctx,id,PLATFORM_KEY_DOWN,true)) hsv->v=unit(hsv->v-.01f);
  if(ui_nav_key(ctx,id,PLATFORM_KEY_HOME,false)) { hsv->s=0; hsv->v=1; }
  if(ui_nav_key(ctx,id,PLATFORM_KEY_END,false)) { hsv->s=1; hsv->v=1; }
  if(ui_pointer_drag(&state->dragging,hover,true)&&started) *started=true;
  if(state->dragging) {
    hsv->s=unit((ui_state_frame(ctx)->g_mouse_x-r.x)/r.w);
    hsv->v=unit(1-(ui_state_frame(ctx)->g_mouse_y-r.y)/r.h);
  }
  if(ui_clip_push(r)) {
    for(int y=0;y<48;++y) for(int x=0;x<64;++x) {
      UIColor c=ui_color_from_hsv((UIColorHSV){hsv->h,(x+.5f)/64,1-(y+.5f)/48});
      /* Integer pixel coverage avoids seams in software rasterizers that
       * round rectangle origins and extents independently. Parent clips keep
       * overlapping edge coverage inside the pad. */
      float left=floorf(r.x+x*r.w/64),top=floorf(r.y+y*r.h/48);
      float right=ceilf(r.x+(x+1)*r.w/64),bottom=ceilf(r.y+(y+1)*r.h/48);
      ui_paint_draw_quad(left,top,right-left,bottom-top,c.r,c.g,c.b);
    }
    float x=r.x+hsv->s*r.w,y=r.y+(1-hsv->v)*r.h;
    ui_paint_draw_quad(x-5,y-1,10,2,1,1,1); ui_paint_draw_quad(x-1,y-5,2,10,1,1,1);
    ui_clip_pop();
  }
  ui_nav_ring(id,r); return old.s!=hsv->s||old.v!=hsv->v;
}
