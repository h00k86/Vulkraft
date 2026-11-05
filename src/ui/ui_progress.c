#include "ui/ui.h"
#include "ui_paint_internal.h"
#include "ui_geometry_internal.h"
#include <stdio.h>

void ui_progress_bar(Font *font,UIRect r,float fraction,bool indeterminate,float phase,const char *label) {
  if(!font||!ui_layout_rect_valid(r)) return;
  UITheme t=ui_get_theme();
  fraction=isfinite(fraction)?fmaxf(0,fminf(1,fraction)):0;
  ui_paint_draw_quad(r.x,r.y,r.w,r.h,t.field.r,t.field.g,t.field.b);
  float x=r.x,w=r.w*fraction;
  if(indeterminate) { w=r.w*.3f; x+=(r.w-w)*(.5f+.5f*sinf(isfinite(phase)?fmodf(phase,2.0943951f)*3:0)); }
  ui_paint_draw_quad(x,r.y,w,r.h,t.accent.r,t.accent.g,t.accent.b);
  char percent[16]; snprintf(percent,sizeof(percent),"%.0f%%",fraction*100);
  const char *text=label?label:(indeterminate?"":percent);
  if(ui_clip_push(r)) {
    float size=fminf(14,r.h*.65f),width=ui_paint_measure_text_width(font,text,size);
    ui_paint_draw_text_colored(font,text,r.x+fmaxf(2,(r.w-width)*.5f),r.y+(r.h+ui_paint_font_ascent(font,size))*.5f,size,t.text.r,t.text.g,t.text.b);
    ui_clip_pop();
  }
}
