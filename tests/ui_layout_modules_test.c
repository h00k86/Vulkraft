/* Public API consumer linked normally: exercise the extracted modules together. */
#include "ui/ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"layout modules:%d: %s\n",__LINE__,#x); exit(1); } } while(0)
typedef struct { int depth,quads; bool reject; } Recorder;
static void quad(void *u,float x,float y,float w,float h,float r,float g,float b) {
  CHECK(isfinite(x)&&isfinite(y)&&isfinite(w)&&isfinite(h)); ++((Recorder *)u)->quads;
}
static void alpha(void *u,float x,float y,float w,float h,float r,float g,float b,float a) { quad(u,x,y,w,h,r,g,b); }
static void text(void *u,Font *f,const char *s,float x,float y,float z,float r,float g,float b) {}
static void image(void *u,float x,float y,float w,float h,int slot,float a,float b,float c,float d) {}
static float measure(void *u,Font *f,const char *s,float z) { return strlen(s)*z*.5f; }
static float ascent(void *u,Font *f,float z) { return z; }
static bool push(void *u,float x,float y,float w,float h) {
  Recorder *r=u; if(r->reject) return false; ++r->depth; return true;
}
static bool pop(void *u) { Recorder *r=u; CHECK(r->depth>0); --r->depth; return true; }
static int depth(void *u) { return ((Recorder *)u)->depth; }
static const UIRenderer renderer={.draw_quad=quad,.draw_quad_alpha=alpha,.draw_text_colored=text,
  .draw_textured_quad=image,.measure_text_width=measure,.font_ascent=ascent,
  .text_clip_push=push,.text_clip_pop=pop,.text_clip_depth=depth};
static Font *font=(Font *)1;
static void begin(float x,float y,bool down,float wheel,bool lost) {
  UIFrameInput f={.width=640,.height=480,.mouse_x=x,.mouse_y=y,.mouse_down=down,
    .scroll_delta=wheel,.focus_lost=lost,.delta_time=.016f};
  CHECK(ui_begin_frame_input(&f));
}
static void draw_scope(void) {
  UIPanel panel=ui_panel_begin("panel","Panel",font,(UIRect){0,0,320,220},1);
  UIScrollScope scroll=ui_scroll_begin("rows",(UIRect){20,40,180,100},1);
  UIPropertyLayout p=ui_property_layout_begin(scroll.layout.bounds.x,scroll.layout.cursor_y,scroll.layout.bounds.w,1);
  for(int n=0;n<12;++n) ui_property_value(&p,font,"Label","Value — UTF-8");
  ui_property_layout_commit(&scroll.layout,&p);
  ui_scroll_end(&scroll); ui_panel_end(&panel);
  ui_scroll_end(&scroll); ui_panel_end(&panel); /* Ending a scope twice is harmless. */
}
static UIScrollDiagnostics diagnostics(void) {
  UIScrollDiagnostics d; CHECK(ui_scroll_get_diagnostics("rows",&d)); return d;
}
static void scrolling(Recorder *rec) {
  begin(-20,-20,false,0,false); draw_scope(); ui_end_frame();
  CHECK(rec->depth==0&&diagnostics().max_scroll>0);
  begin(50,60,false,-2,false); draw_scope(); ui_end_frame();
  CHECK(diagnostics().offset>0&&diagnostics().wheel_consumed);
  begin(50,60,false,100,false); draw_scope(); ui_end_frame(); CHECK(diagnostics().offset==0);

  /* Entering a thumb with the button already held cannot start a drag. */
  begin(500,400,true,0,false); draw_scope(); ui_end_frame();
  begin(194,50,true,0,false); draw_scope(); ui_end_frame(); CHECK(!diagnostics().scrollbar_dragging);
  begin(194,50,false,0,false); draw_scope(); ui_end_frame();
  begin(194,50,true,0,false); draw_scope(); ui_end_frame(); CHECK(diagnostics().scrollbar_dragging);
  begin(194,130,true,0,false); draw_scope(); ui_end_frame(); CHECK(diagnostics().offset>0);
  begin(194,130,true,0,true); draw_scope(); ui_end_frame(); CHECK(!diagnostics().scrollbar_dragging);

  /* An outer clip excludes the thumb from input, including press edges. */
  begin(50,60,false,100,false); draw_scope(); ui_end_frame(); CHECK(diagnostics().offset==0);
  begin(194,50,true,0,false); CHECK(ui_clip_push((UIRect){0,0,5,5}));
  draw_scope(); CHECK(ui_clip_pop()); ui_end_frame(); CHECK(!diagnostics().scrollbar_dragging);

  /* Failed backend clips must not create an unmatched pop. */
  rec->reject=true;
  begin(-20,-20,false,0,false); draw_scope(); ui_end_frame(); CHECK(rec->depth==0);
  rec->reject=false;
}
static void property_composition(Recorder *rec) {
  float number=.5f; int integer=16777217,choice=0; bool toggle=false,revealed=false;
  Vec2 vector={.x=.2f,.y=.3f}; char value[64]="Caffè €",asset[64]="assets/texture.png";
  const char *options[]={"First","Second"},*actions[]={"A","B"};
  begin(-20,-20,false,0,false);
  UILayout parent=ui_layout_begin((UIRect){10,10,600,460},1);
  UIRow row=ui_row_begin(&parent,3); UIRect a=ui_row_next(&row),b=ui_row_next(&row),c=ui_row_next(&row);
  CHECK(a.w>0&&a.x+a.w<b.x&&b.x+b.w<c.x&&ui_row_next(&row).w==0);
  UISection section=ui_section_begin(&parent,"section","Section",font);
  CHECK(section.open);
  UIPropertyLayout p=ui_property_layout_begin(10,section.layout.cursor_y,580,1);
  ui_property_float(&p,"float",font,"Float",&number,0,1);
  ui_property_int(&p,"integer",font,"Integer",&integer,0,2147483647);
  ui_property_vec2(&p,"vector",font,"Vector",&vector,0,1);
  ui_property_slider(&p,"slider",font,"Slider",&number,0,1,NULL);
  ui_property_toggle(&p,"toggle",font,"Toggle",&toggle);
  ui_property_checkbox(&p,"checkbox",font,"Checkbox",&toggle);
  ui_property_dropdown(&p,"dropdown",font,"Dropdown",options,2,&choice);
  ui_property_text(&p,"text",font,"Text",value,sizeof(value));
  ui_property_password(&p,"password",font,"Password",value,sizeof(value),&revealed);
  ui_property_action(&p,"action",font,"Action",false);
  ui_property_action_row(&p,"actions",font,actions,2);
  ui_property_asset(&p,"asset",font,"Asset",asset,sizeof(asset),NULL);
  ui_property_layout_commit(&section.layout,&p); ui_section_end(&section);
  CHECK(parent.cursor_y==p.cursor_y&&integer==16777217&&!strcmp(value,"Caffè €"));
  ui_end_frame(); CHECK(rec->depth==0);

  /* The read-only asset field retains press activation and input clipping. */
  bool clicked;
  begin(140,20,true,0,false); CHECK(ui_clip_push((UIRect){0,0,5,5}));
  p=ui_property_layout_begin(10,10,300,1);
  CHECK(!ui_property_asset(&p,"asset",font,"Asset",asset,sizeof(asset),&clicked)&&!clicked);
  CHECK(ui_clip_pop()); ui_end_frame();
  begin(140,20,false,0,false); ui_end_frame();
  begin(140,20,true,0,false); p=ui_property_layout_begin(10,10,300,1);
  CHECK(ui_property_asset(&p,"asset",font,"Asset",asset,sizeof(asset),&clicked)&&clicked);
  ui_end_frame(); CHECK(rec->depth==0);
}
int main(void) {
  Recorder rec={0}; UIContext *ctx=ui_context_create();
  CHECK(ctx&&ui_context_set_current(ctx)&&ui_context_set_renderer(ctx,&renderer,&rec));
  UITheme theme=ui_theme_dark(); theme.row_height=theme.field_height=20; theme.row_spacing=4;
  theme.panel_padding=5; theme.panel_header_height=18; theme.scrollbar_width=12; ui_set_theme(theme);
  scrolling(&rec); CHECK(ui_context_reset(ctx)); ui_set_theme(theme);
  property_composition(&rec);
  CHECK(rec.depth==0&&rec.quads>0&&ui_context_destroy(ctx));
  puts("Layout modules: PASS (linked public API, property composition, sections, wheel, scrollbar press/clips/cancel, clip failure, asset activation)");
  return 0;
}
