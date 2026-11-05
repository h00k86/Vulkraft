#include "ui/ui.h"
#include "ui_paint_internal.h"
#include <math.h>
#include <stdio.h>
/* Color editing uses sRGB channel values; UIColor remains linear for rendering. */
static float ui_color_channel_srgb(float linear) {
  linear=isfinite(linear)?fmaxf(0,fminf(1,linear)):0;
  /* Inverse of ui_color_srgb: preserve the framework's existing gamma 2.2 mapping. */
  return powf(linear,1.0f/2.2f);
}
bool ui_property_color(UIPropertyLayout *layout,const char *id,Font *font,
                       const char *label,UIColor *color) {
  if(!layout||!font||!label||!color||!isfinite(layout->width)||layout->width<=0||
     !isfinite(layout->scale)||layout->scale<=0||!ui_push_id(id)) return false;
  UIColor old=*color;
  float rgb[]={ui_color_channel_srgb(color->r)*255,ui_color_channel_srgb(color->g)*255,
               ui_color_channel_srgb(color->b)*255};
  bool changed=!isfinite(color->r)||!isfinite(color->g)||!isfinite(color->b)||
    color->r<0||color->r>1||color->g<0||color->g>1||color->b<0||color->b>1;
  static const uint32_t palette[]={0xe0524f,0xf0842c,0xf2c94c,0x48bd78,0x38bfc9,0x4488ee,0xab34eb,0xffffff};
  static const char *names[]={"R","O","Y","G","C","B","V","W"};
  float y=ui_property_layout_next_row(layout),gap=4*layout->scale;
  float width=(layout->width-7*gap)/8;
  if(width>0) for(int i=0;i<8;++i) {
    char local[24]; snprintf(local,sizeof(local),"preset/%d",i);
    UIColor preset=ui_color_srgb(palette[i]);
    UIButtonStyle style={.min_width=width,.max_width=width,.min_height=layout->row_height,
      .has_custom_color=true,.color_normal=preset,.color_hover=preset,.color_pressed=preset,
      .color_border=ui_get_theme().text,.has_custom_text=true,
      .color_text=(preset.r*.2126f+preset.g*.7152f+preset.b*.0722f)>.179f?(UIColor){0,0,0}:(UIColor){1,1,1}};
    if(ui_button_labeled(ui_scoped_id(local),names[i],font,ANCHOR_TOP_LEFT,0,12*layout->scale,
                         layout->x+i*(width+gap),y,&style)) {
      rgb[0]=(palette[i]>>16)&255; rgb[1]=(palette[i]>>8)&255; rgb[2]=palette[i]&255; changed=true;
    }
  }
  changed|=ui_property_slider(layout,"red",font,"Rosso",&rgb[0],0,255,NULL);
  changed|=ui_property_slider(layout,"green",font,"Verde",&rgb[1],0,255,NULL);
  changed|=ui_property_slider(layout,"blue",font,"Blu",&rgb[2],0,255,NULL);
  uint32_t hex=((uint32_t)lroundf(rgb[0])<<16)|((uint32_t)lroundf(rgb[1])<<8)|(uint32_t)lroundf(rgb[2]);
  if(changed) *color=ui_color_srgb(hex);
  char value[16]; snprintf(value,sizeof(value),"#%06X",(unsigned)hex);
  ui_property_value(layout,font,label,value);
  y=ui_property_layout_next_row(layout);
  ui_paint_draw_quad(layout->x,y,layout->width,layout->row_height,color->r,color->g,color->b);
  ui_pop_id();
  return old.r!=color->r||old.g!=color->g||old.b!=color->b;
}
