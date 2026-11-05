#define main navigation_suite_main
#include "ui_navigation_test.c"
#undef main
#include "../src/ui/ui_state_colors_internal.h"
#include "../src/ui/ui_color_internal.h"
static int images;
static void icon_image(void *u,float x,float y,float w,float h,int slot,float a,float b,float c,float d) {
  ++images; CHECK(slot==7&&a==0&&b==0&&c==1&&d==1);
}
static void placeholders(void) {
  UIContext *c=context(); char value[64]="";
  UITextInputOptions options={.placeholder="Cerca nella scena è"};
  begin(NULL,0); label_count=0;
  ui_text_input_ex("search",font,ANCHOR_TOP_LEFT,0,14,20,20,220,value,sizeof(value),&options);
  CHECK(!value[0]&&strstr(label_trace[0],"Cerca")); ui_end_frame();
  ui_text_input_focus("search"); begin(NULL,0); label_count=0;
  ui_text_input_ex("search",font,ANCHOR_TOP_LEFT,0,14,20,20,220,value,sizeof(value),&options);
  CHECK(!value[0]&&strstr(label_trace[0],"Cerca")&&ui_has_keyboard_capture()); ui_end_frame();
  InputKeyEvent e=character(0xe8); begin(&e,1); label_count=0;
  ui_text_input_ex("search",font,ANCHOR_TOP_LEFT,0,14,20,20,220,value,sizeof(value),&options);
  CHECK(!strcmp(value,"è")&&!strcmp(label_trace[0],"è")); ui_end_frame();
  e=key(PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL); begin(&e,1); label_count=0;
  ui_text_input_ex("search",font,ANCHOR_TOP_LEFT,0,14,20,20,220,value,sizeof(value),&options);
  CHECK(!value[0]&&strstr(label_trace[0],"Cerca")); ui_end_frame();
  UIButtonStyle disabled={.disabled=true}; options.style=&disabled;
  begin(NULL,0); ui_text_input_ex("search",font,ANCHOR_TOP_LEFT,0,14,20,20,220,value,sizeof(value),&options);
  CHECK(!ui_text_input_is_focused("search")); ui_end_frame();
  CHECK(ui_context_destroy(c));
}
static void icons(void) {
  UIContext *c=context(); UIIcon icon={.kind=UI_ICON_CAMERA}; UIRect r={20,20,100,30};
  begin(NULL,0); CHECK(!ui_icon_button("camera",font,r,&icon,"Camera",NULL)); ui_end_frame();
  InputKeyEvent tab=key(PLATFORM_KEY_TAB,0); begin(&tab,1);
  ui_icon_button("camera",font,r,&icon,"Camera",NULL); ui_end_frame(); focus("camera");
  InputKeyEvent enter=key(PLATFORM_KEY_ENTER,0); begin(&enter,1);
  CHECK(ui_icon_button("camera",font,r,&icon,"Camera",NULL)); ui_end_frame();
  UIButtonStyle disabled={.disabled=true}; begin(&enter,1);
  CHECK(!ui_icon_button("camera",font,r,&icon,"Camera",&disabled)); ui_end_frame();
  UIFrameInput f={.width=640,.height=480,.mouse_x=30,.mouse_y=30};
  CHECK(ui_begin_frame_input(&f)); ui_icon_button("camera",font,r,&icon,"Camera",NULL); ui_end_frame();
  f.mouse_down=true; CHECK(ui_begin_frame_input(&f)); CHECK(!ui_icon_button("camera",font,r,&icon,"Camera",NULL)); ui_end_frame();
  f.mouse_down=false; CHECK(ui_begin_frame_input(&f)); CHECK(ui_icon_button("camera",font,r,&icon,"Camera",NULL)); ui_end_frame();
  CHECK(!ui_has_input_capture());
  begin(NULL,0); for(int i=UI_ICON_NONE;i<UI_ICON_TEXTURE;++i) {
    icon.kind=(UIIconKind)i; ui_icon_button("glyph",NULL,r,&icon,NULL,NULL);
  } ui_end_frame();
  UIRenderer custom=renderer; custom.draw_textured_quad=icon_image;
  CHECK(ui_context_set_renderer(c,&custom,NULL));
  icon=(UIIcon){.kind=UI_ICON_TEXTURE,.texture_slot=7,.u1=1,.v1=1}; images=0;
  begin(NULL,0); ui_icon_button("texture",font,r,&icon,NULL,NULL); CHECK(images==1);
  icon.u1=NAN; ui_icon_button("invalid",font,r,&icon,NULL,NULL); CHECK(images==1); ui_end_frame();
  CHECK(ui_context_destroy(c));
}
static void collections(void) {
  UIContext *c=context(); int selected=-1; UIRect bounds={20,20,240,52};
  UIListItem items[]={{"a","Prima",false},{"disabled","No",true},{"b","Ultima",false}};
  begin(NULL,0); UICollectionResult result=ui_list("list",font,bounds,items,3,&selected);
  CHECK(!result.selection_changed&&result.activated_index==-1); ui_end_frame();
  InputKeyEvent e=key(PLATFORM_KEY_TAB,0); begin(&e,1); ui_list("list",font,bounds,items,3,&selected); ui_end_frame(); focus("list/view");
  e=key(PLATFORM_KEY_DOWN,0); begin(&e,1); result=ui_list("list",font,bounds,items,3,&selected); ui_end_frame(); CHECK(selected==0&&result.selection_changed);
  begin(&e,1); ui_list("list",font,bounds,items,3,&selected); ui_end_frame(); CHECK(selected==2);
  UIScrollDiagnostics d; CHECK(ui_scroll_get_diagnostics("list/scroll",&d)&&d.offset>0);
  e=key(PLATFORM_KEY_ENTER,0); begin(&e,1); result=ui_list("list",font,bounds,items,3,&selected); ui_end_frame(); CHECK(result.activated_index==2);
  e=key(PLATFORM_KEY_HOME,0); begin(&e,1); ui_list("list",font,bounds,items,3,&selected); ui_end_frame(); CHECK(selected==0);
  InputKeyEvent ordered[]={key(PLATFORM_KEY_DOWN,0),key(PLATFORM_KEY_UP,0)};
  begin(ordered,2); ui_list("list",font,bounds,items,3,&selected); ui_end_frame(); CHECK(selected==0);
  CHECK(ui_context_reset(c));
  UITreeNode nodes[]={{"root","Scena",0,false,false},{"bone","Ossa",1,true,false},
                      {"leaf","Osso è",2,false,false},{"other","Altro",0,false,false}};
  selected=0; begin(NULL,0); label_count=0; ui_tree("tree",font,bounds,nodes,4,&selected); CHECK(label_count==2); ui_end_frame();
  e=key(PLATFORM_KEY_TAB,0); begin(&e,1); ui_tree("tree",font,bounds,nodes,4,&selected); ui_end_frame();
  e=key(PLATFORM_KEY_RIGHT,0); begin(&e,1); result=ui_tree("tree",font,bounds,nodes,4,&selected); ui_end_frame(); CHECK(nodes[0].expanded&&result.expansion_changed);
  begin(&e,1); ui_tree("tree",font,bounds,nodes,4,&selected); ui_end_frame(); CHECK(selected==1);
  begin(&e,1); ui_tree("tree",font,bounds,nodes,4,&selected); ui_end_frame(); CHECK(selected==2);
  e=key(PLATFORM_KEY_LEFT,0); begin(&e,1); ui_tree("tree",font,bounds,nodes,4,&selected); ui_end_frame(); CHECK(selected==1);
  begin(&e,1); result=ui_tree("tree",font,bounds,nodes,4,&selected); ui_end_frame(); CHECK(!nodes[1].expanded&&result.expansion_changed);
  nodes[0].expanded=false; selected=2;
  begin(NULL,0); result=ui_tree("tree",font,bounds,nodes,4,&selected); ui_end_frame(); CHECK(selected==0&&result.selection_changed);
  UITreeNode invalid[]={{"a","A",0,false,false},{"b","B",2,false,false}};
  begin(NULL,0); result=ui_tree("bad",font,bounds,invalid,2,&selected); CHECK(!result.selection_changed&&selected==0);
  result=ui_list("empty",font,bounds,NULL,0,&selected); CHECK(result.selection_changed&&selected==-1); ui_end_frame();
  CHECK(ui_context_destroy(c));
}
static bool near_color(UIColor a,UIColor b) { return fabsf(a.r-b.r)<.00001f&&fabsf(a.g-b.g)<.00001f&&fabsf(a.b-b.b)<.00001f; }
static bool rgba_frame(UIColorRGBA *value,UIColorPickerMode mode,bool *started,
                        float x,float y,bool down,const InputKeyEvent *event) {
  UIFrameInput f={.width=640,.height=800,.mouse_x=x,.mouse_y=y,.mouse_down=down,.events=event,.event_count=event?1:0};
  CHECK(ui_begin_frame_input(&f));
  UIColorPickerOptions options={.mode=mode,.integer_channels=true};
  UIPropertyLayout layout=ui_property_layout_begin(20,20,500,1);
  bool changed=ui_property_color_rgba(&layout,"rgba",font,"RGBA",value,&options,started);
  ui_end_frame(); return changed;
}
static void colors(void) {
  for(int i=0;i<360;i+=15) {
    UIColorHSV a={(float)i,.7f,.8f}; UIColor rgb=ui_color_from_hsv(a); UIColorHSV b=ui_color_to_hsv(rgb);
    CHECK(fabsf(a.h-b.h)<.001f&&fabsf(a.s-b.s)<.00001f&&fabsf(a.v-b.v)<.00001f);
  }
  CHECK(near_color(ui_color_from_hsv((UIColorHSV){-120,1,1}),(UIColor){0,0,1}));
  CHECK(near_color(ui_color_from_hsv((UIColorHSV){360,1,1}),(UIColor){1,0,0}));
  UIColorHSV invalid=ui_color_to_hsv((UIColor){NAN,2,-1}); CHECK(invalid.h==120&&invalid.s==1&&invalid.v==1);
  UIContext *c=context(); bool started=true;
  UIColorRGBA value={.1234567f,.2345678f,.3456789f,.54321f},original=value;
  CHECK(!rgba_frame(&value,UI_COLOR_RGB,&started,-20,-20,false,NULL)&&!started);
  CHECK(!memcmp(&value,&original,sizeof(value)));
  float alpha_y=20+4*(ui_get_theme().field_height+ui_get_theme().row_spacing)+ui_get_theme().field_height+2;
  CHECK(rgba_frame(&value,UI_COLOR_RGB,&started,100,alpha_y,true,NULL)&&started);
  CHECK(value.r==original.r&&value.g==original.g&&value.b==original.b&&value.a!=original.a);
  rgba_frame(&value,UI_COLOR_RGB,&started,100,alpha_y,false,NULL); CHECK(!started);
  value=original; CHECK(!rgba_frame(&value,UI_COLOR_HSV,&started,-20,-20,false,NULL)&&!started);
  CHECK(!memcmp(&value,&original,sizeof(value)));
  CHECK(rgba_frame(&value,UI_COLOR_HSV,&started,300,90,true,NULL)&&started);
  CHECK(value.a==original.a); rgba_frame(&value,UI_COLOR_HSV,&started,300,90,false,NULL); CHECK(!started);
  /* Hue chosen on gray survives frames, but reset/context switch isolate it. */
  value=(UIColorRGBA){.3f,.3f,.3f,1}; rgba_frame(&value,UI_COLOR_HSV,&started,-20,-20,false,NULL);
  UIColorsState *state=ui_state_colors(c); CHECK(state->count==1);
  float hue_track=20+ui_get_theme().field_height+ui_get_theme().row_spacing+120+ui_get_theme().row_spacing+
    8+ui_get_theme().row_spacing+ui_get_theme().field_height+2;
  rgba_frame(&value,UI_COLOR_HSV,&started,296,hue_track,true,NULL); CHECK(started);
  float chosen_hue=state->entries[0].hsv.h; CHECK(chosen_hue>200&&chosen_hue<280);
  rgba_frame(&value,UI_COLOR_HSV,&started,-20,-20,false,NULL); CHECK(state->entries[0].hsv.h==chosen_hue);
  UIContext *other=context(); rgba_frame(&value,UI_COLOR_HSV,&started,-20,-20,false,NULL);
  CHECK(ui_state_colors(other)->entries[0].hsv.h==0);
  CHECK(ui_context_destroy(other)&&ui_context_set_current(c)); CHECK(ui_context_reset(c)&&!ui_state_colors(c)->count);
  begin(NULL,0); UIColorHSV hsv;
  for(int i=0;i<UI_COLOR_MEMORY_CAPACITY;++i) {
    char id[32]; snprintf(id,sizeof(id),"memory%d",i);
    CHECK(ui_color_hsv_begin(id,(UIColor){.2f,.2f,.2f},&hsv));
  }
  CHECK(!ui_color_hsv_begin("overflow",(UIColor){0},&hsv));
  CHECK(ui_context_error(c).code==UI_ERROR_LIMIT); ui_end_frame();
  begin(NULL,0); CHECK(ui_color_hsv_begin("reused",(UIColor){0},&hsv)); ui_end_frame();
  CHECK(ui_context_destroy(c));
}
int main(void) {
  placeholders(); icons(); collections(); colors(); CHECK(clips==0);
  puts("Components: PASS (placeholder/history, geometric/texture icons, selection/tree/scroll, RGBA precision, HSV conversion/pad/context memory)");
}
