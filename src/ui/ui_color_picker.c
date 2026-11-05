#include "ui/ui.h"
#include "ui_paint_internal.h"
#include "ui_widget_internal.h"
#include "ui_color_internal.h"
#include "ui_geometry_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static float unit(float x) { return isfinite(x)?fmaxf(0,fminf(1,x)):0; }
static float srgb(float linear) { return powf(unit(linear),1.0f/2.2f); }
static UIColor normalize(UIColor c) { return (UIColor){unit(c.r),unit(c.g),unit(c.b)}; }
static bool equal(UIColor a,UIColor b) { return a.r==b.r&&a.g==b.g&&a.b==b.b; }
UIColorHSV ui_color_to_hsv(UIColor color) {
  float r=srgb(color.r),g=srgb(color.g),b=srgb(color.b);
  float hi=fmaxf(r,fmaxf(g,b)),lo=fminf(r,fminf(g,b)),d=hi-lo;
  UIColorHSV hsv={.s=hi>0?d/hi:0,.v=hi};
  if(d>0) {
    if(hi==r) hsv.h=60*fmodf((g-b)/d,6);
    else if(hi==g) hsv.h=60*((b-r)/d+2);
    else hsv.h=60*((r-g)/d+4);
    if(hsv.h<0) hsv.h+=360;
  }
  return hsv;
}
UIColor ui_color_from_hsv(UIColorHSV hsv) {
  float h=isfinite(hsv.h)?fmodf(hsv.h,360):0;
  if(h<0) h+=360;
  float s=unit(hsv.s),v=unit(hsv.v),c=v*s,x=c*(1-fabsf(fmodf(h/60,2)-1)),m=v-c;
  float r=0,g=0,b=0;
  if(h<60) { r=c; g=x; } else if(h<120) { r=x; g=c; }
  else if(h<180) { g=c; b=x; } else if(h<240) { g=x; b=c; }
  else if(h<300) { r=x; b=c; } else { r=c; b=x; }
  return (UIColor){powf(r+m,2.2f),powf(g+m,2.2f),powf(b+m,2.2f)};
}
static bool channel(UIPropertyLayout *layout,const char *id,Font *font,
                     const char *label,float *value,float min,float max,
                     int idle_decimals,bool *started) {
  float y=ui_property_layout_next_row(layout); bool start=false;
  bool changed=ui_slider_format(ui_scoped_id(id),font,ANCHOR_TOP_LEFT,0,layout->scale,
                                layout->x,y,layout->width,value,min,max,label,&start,idle_decimals);
  if(started) *started|=start;
  return changed;
}
static void preview(UIPropertyLayout *layout,UIColor color,float alpha) {
  float y=ui_property_layout_next_row(layout);
  UIRect r={layout->x,y,layout->width,layout->row_height};
  if(!ui_clip_push(r)) return;
  float tile=fmaxf(8*layout->scale,fmaxf(r.w/64,r.h/16));
  int columns=(int)ceilf(r.w/tile),rows=(int)ceilf(r.h/tile);
  for(int iy=0;iy<rows;++iy) for(int ix=0;ix<columns;++ix) {
    float cx=ix*tile,cy=iy*tile;
    float gray=((ix+iy)&1)?.55f:.25f;
    ui_paint_draw_quad(r.x+cx,r.y+cy,fminf(tile,r.w-cx),fminf(tile,r.h-cy),
                       color.r*alpha+gray*(1-alpha),color.g*alpha+gray*(1-alpha),color.b*alpha+gray*(1-alpha));
  }
  ui_clip_pop();
}
static bool picker(UIPropertyLayout *layout,const char *id,Font *font,
                    const char *label,UIColor *color,float *alpha,
                    const UIColorPickerOptions *options,bool *out_drag_started) {
  if(out_drag_started) *out_drag_started=false;
  if(!layout||!font||!label||!color||!isfinite(layout->width)||layout->width<=0||
     !isfinite(layout->scale)||layout->scale<=0||
     !ui_layout_rect_valid((UIRect){layout->x,layout->cursor_y,layout->width,layout->row_height})||
     !isfinite(layout->cursor_y+600*layout->scale+8*layout->row_height)||
     (options&&(options->mode<UI_COLOR_RGB||options->mode>UI_COLOR_HSV))||!ui_push_id(id)) return false;
  UIColor old=*color,working=normalize(old); float old_alpha=alpha?*alpha:1,a=unit(old_alpha);
  bool normalize_rgb=!equal(old,working);
  float rgb[]={srgb(working.r)*255,srgb(working.g)*255,srgb(working.b)*255};
  bool changed=normalize_rgb,preset_picked=false;
  bool hsv_mode=options&&options->mode==UI_COLOR_HSV;
  UIColorHSV hsv={0}; char hsv_id[UI_ID_CAPACITY]={0};
  if(hsv_mode) {
    snprintf(hsv_id,sizeof(hsv_id),"%s",ui_scoped_id("hsv"));
    if(!hsv_id[0]||!ui_color_hsv_begin(hsv_id,working,&hsv)) { ui_pop_id(); return false; }
  }
  static const uint32_t palette[]={0xe0524f,0xf0842c,0xf2c94c,0x48bd78,0x38bfc9,0x4488ee,0xab34eb,0xffffff};
  static const char *names[]={"R","O","Y","G","C","B","V","W"};
  float y=ui_property_layout_next_row(layout),gap=4*layout->scale,width=(layout->width-7*gap)/8;
  if(width>0) for(int i=0;i<8;++i) {
    char local[24]; snprintf(local,sizeof(local),"preset/%d",i); UIColor preset=ui_color_srgb(palette[i]);
    UIButtonStyle style={.min_width=width,.max_width=width,.min_height=layout->row_height,
      .has_custom_color=true,.color_normal=preset,.color_hover=preset,.color_pressed=preset,
      .color_border=ui_get_theme().text,.has_custom_text=true,
      .color_text=(preset.r*.2126f+preset.g*.7152f+preset.b*.0722f)>.179f?(UIColor){0,0,0}:(UIColor){1,1,1}};
    if(ui_button_labeled(ui_scoped_id(local),names[i],font,ANCHOR_TOP_LEFT,0,12*layout->scale,
                         layout->x+i*(width+gap),y,&style)) {
      rgb[0]=(palette[i]>>16)&255; rgb[1]=(palette[i]>>8)&255; rgb[2]=palette[i]&255;
      working=preset; changed=preset_picked=true;
    }
  }
  bool started=false;
  if(hsv_mode) {
    if(preset_picked) hsv=ui_color_to_hsv(working);
    UIColorHSV before=hsv;
    ui_color_sv_pad(layout,&hsv,&started);
    UIRect hue={layout->x,layout->cursor_y,layout->width,8*layout->scale};
    if(ui_clip_push(hue)) {
      for(int i=0;i<36;++i) {
        UIColor c=ui_color_from_hsv((UIColorHSV){i*10,1,1});
        ui_paint_draw_quad(hue.x+i*hue.w/36,hue.y,hue.w/36+.1f,hue.h,c.r,c.g,c.b);
      }
      ui_clip_pop();
    }
    layout->cursor_y+=hue.h+ui_get_theme().row_spacing*layout->scale;
    channel(layout,"hue",font,"H",&hsv.h,0,360,1,&started);
    channel(layout,"saturation",font,"S",&hsv.s,0,1,3,&started);
    channel(layout,"value",font,"V",&hsv.v,0,1,3,&started);
    if(hsv.h>=360) hsv.h=0;
    if(hsv.s!=before.s||hsv.v!=before.v||(hsv.h!=before.h&&hsv.s>0&&hsv.v>0)) working=ui_color_from_hsv(hsv);
    ui_color_hsv_end(hsv_id,working,hsv);
    rgb[0]=srgb(working.r)*255; rgb[1]=srgb(working.g)*255; rgb[2]=srgb(working.b)*255;
  } else {
    int idle_decimals=options&&options->integer_channels?0:-1;
    changed|=channel(layout,"red",font,"Rosso",&rgb[0],0,255,idle_decimals,&started);
    changed|=channel(layout,"green",font,"Verde",&rgb[1],0,255,idle_decimals,&started);
    changed|=channel(layout,"blue",font,"Blu",&rgb[2],0,255,idle_decimals,&started);
    if(changed) working=ui_color_srgb(((uint32_t)lroundf(rgb[0])<<16)|((uint32_t)lroundf(rgb[1])<<8)|(uint32_t)lroundf(rgb[2]));
  }
  if(alpha) channel(layout,"alpha",font,"Alpha",&a,0,1,3,&started);
  if(out_drag_started) *out_drag_started=started;
  *color=working; if(alpha) *alpha=a;
  uint32_t hex=((uint32_t)lroundf(rgb[0])<<16)|((uint32_t)lroundf(rgb[1])<<8)|(uint32_t)lroundf(rgb[2]);
  char value[16];
  if(alpha) snprintf(value,sizeof(value),"#%06X%02X",(unsigned)hex,(unsigned)lroundf(a*255));
  else snprintf(value,sizeof(value),"#%06X",(unsigned)hex);
  ui_property_value(layout,font,label,value);
  if(alpha) preview(layout,working,a);
  else {
    y=ui_property_layout_next_row(layout);
    ui_paint_draw_quad(layout->x,y,layout->width,layout->row_height,working.r,working.g,working.b);
  }
  ui_pop_id(); return !equal(old,*color)||(alpha&&old_alpha!=*alpha);
}
bool ui_property_color_ex(UIPropertyLayout *layout,const char *id,Font *font,
                          const char *label,UIColor *color,
                          const UIColorPickerOptions *options,bool *out_drag_started) {
  return picker(layout,id,font,label,color,NULL,options,out_drag_started);
}
bool ui_property_color_rgba(UIPropertyLayout *layout,const char *id,Font *font,
                            const char *label,UIColorRGBA *color,
                            const UIColorPickerOptions *options,bool *out_drag_started) {
  if(!color) { if(out_drag_started) *out_drag_started=false; return false; }
  UIColor rgb={color->r,color->g,color->b}; float alpha=color->a;
  bool changed=picker(layout,id,font,label,&rgb,&alpha,options,out_drag_started);
  if(changed) *color=(UIColorRGBA){rgb.r,rgb.g,rgb.b,alpha};
  return changed;
}
bool ui_property_color(UIPropertyLayout *layout,const char *id,Font *font,
                       const char *label,UIColor *color) {
  return ui_property_color_ex(layout,id,font,label,color,NULL,NULL);
}
