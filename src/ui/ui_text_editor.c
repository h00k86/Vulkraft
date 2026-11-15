/* Portable multiline widget. The host owns documents, paths and compilation;
 * this module owns only a viewport and gestures over the shared UTF-8 editor. */
#include "ui/ui.h"
#include "ui_state_text_fields_internal.h"
#include "ui_state_frame_internal.h"
#include "ui_state_navigation_internal.h"
#include "ui_state_clipboard_internal.h"
#include "ui_overlay_internal.h"
#include "ui_navigation_internal.h"
#include "ui_capture_internal.h"
#include "ui_widget_internal.h"
#include "ui_interaction_internal.h"
#include "ui_paint_internal.h"
#include "ui_error_internal.h"
#include "text/utf8.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float clamp(float x,float maximum) { return fmaxf(0,fminf(x,maximum)); }
static uint64_t hash(const char *text) {
  uint64_t h=UINT64_C(14695981039346656037);
  for(const unsigned char *p=(const unsigned char *)text;*p;++p) h=(h^*p)*UINT64_C(1099511628211);
  return h;
}
/* A span is temporarily terminated, just like the existing text field painter.
 * Tab stops are measured in pixels and shared by painting and hit testing. */
static float span(Font *font,char *text,int start,int end,float scale,float tab,
                  bool draw,float x,float baseline,UIColor color) {
  float width=0;
  for(int at=start;at<end;) {
    int next=at; while(next<end && text[next]!='\t') ++next;
    char saved=text[next]; text[next]=0;
    if(draw && next>at) ui_paint_draw_text_colored(font,text+at,x+width,baseline,scale,color.r,color.g,color.b);
    width+=ui_paint_measure_text_width(font,text+at,scale); text[next]=saved;
    if(next<end) { width=(floorf(width/tab)+1)*tab; ++next; }
    at=next;
  }
  return width;
}
static int line_offset(const char *text,int line) {
  int at=0;
  while(line-->1) { int end=ui_text_line_end(text,at); if(!text[end]) return at; at=end+1; }
  return at;
}
static int line_number(const char *text,int at) {
  int line=1; for(int i=0;i<at;++i) if(text[i]=='\n') ++line;
  return line;
}
static float prefix(Font *font,char *text,int start,int end,float scale,float tab) {
  return span(font,text,start,end,scale,tab,false,0,0,(UIColor){0});
}
static int hit(Font *font,char *text,int start,float x,float scale,float tab) {
  int end=ui_text_line_end(text,start),lo=start,hi=end;
  if(x<=0) return start;
  /* Binary search avoids quadratic prefix measurement on a long source line. */
  while(lo<hi) {
    int mid=(int)utf8_floor(text,(size_t)end,(size_t)(lo+(hi-lo)/2));
    if(prefix(font,text,start,mid,scale,tab)<x)
      lo=(int)utf8_next(text,(size_t)end,(size_t)mid);
    else hi=mid;
  }
  if(lo==start) return lo;
  int prev=(int)utf8_prev(text,(size_t)lo);
  return x-prefix(font,text,start,prev,scale,tab)<prefix(font,text,start,lo,scale,tab)-x?prev:lo;
}
static bool readonly_event(const InputKeyEvent *e) {
  if(e->kind!=INPUT_KEY) return false;
  bool ctrl=(e->mods&PLATFORM_MOD_CONTROL) && !(e->mods&PLATFORM_MOD_ALT);
  return e->key==PLATFORM_KEY_LEFT || e->key==PLATFORM_KEY_RIGHT ||
    e->key==PLATFORM_KEY_UP || e->key==PLATFORM_KEY_DOWN ||
    e->key==PLATFORM_KEY_HOME || e->key==PLATFORM_KEY_END ||
    e->key==PLATFORM_KEY_PAGE_UP || e->key==PLATFORM_KEY_PAGE_DOWN ||
    (ctrl && (e->key==PLATFORM_KEY_A || e->key==PLATFORM_KEY_C));
}

UITextEditorResult ui_text_editor(const char *id,Font *font,UIRect bounds,
                                  char *buffer,int capacity,const UITextEditorOptions *options) {
  UITextEditorResult result={0};
  UIContext *ctx=ui_overlay_context();
  UIFrameState *frame=ui_state_frame(ctx);
  UINavigationState *nav=ui_state_navigation(ctx);
  UIClipboardState *clip=ui_state_clipboard(ctx);
  UITheme theme=ui_get_theme();
  UITextEditorOptions opt=options?*options:(UITextEditorOptions){0};
  if(!ui_valid_id(id) || !font || !buffer || capacity<1 || capacity>UI_TEXT_EDITOR_MAX_CAPACITY ||
     !isfinite(bounds.x) || !isfinite(bounds.y) || !isfinite(bounds.w) || !isfinite(bounds.h) ||
     bounds.w<32 || bounds.h<32 || !isfinite(opt.scale) || opt.scale<0 ||
     opt.highlight_line<0 || opt.reveal_line<0) {
    ui_report_error(ctx,UI_ERROR_ARGUMENT,UI_OPERATION_TEXT_EDITOR,id); return result;
  }
  if(!frame->frame_active) {
    ui_report_error(ctx,UI_ERROR_STATE,UI_OPERATION_TEXT_EDITOR,id); return result;
  }
  UITextInputState *state=ui_text_field_state(id); int length;
  if(!state) return result;
  if(!ui_text_edit_prepare(state,buffer,capacity,&length) || strchr(buffer,'\r')) {
    ui_report_error(ctx,UI_ERROR_ARGUMENT,UI_OPERATION_TEXT_EDITOR,id); return result;
  }
  uint64_t before=hash(buffer);
  if(state->observed && state->observed_hash!=before) {
    state->editor_scroll_x=state->editor_scroll_y=0;
    state->editor_selecting=false; state->editor_scrollbar_drag=0;
  }
  float scale=opt.scale>0?opt.scale:theme.text_scale;
  float ascent=ui_paint_font_ascent(font,scale),row=fmaxf(1,ascent+theme.row_spacing);
  float tab=fmaxf(1,ui_paint_measure_text_width(font,"    ",scale));
  if(!isfinite(ascent) || !isfinite(row) || !isfinite(tab)) {
    ui_report_error(ctx,UI_ERROR_ARGUMENT,UI_OPERATION_TEXT_EDITOR,id); return result;
  }
  int lines=line_number(buffer,length);
  char digits[32]; snprintf(digits,sizeof(digits),"%d",lines);
  float padding=4,bar=fminf(12,fmaxf(4,theme.scrollbar_width));
  float gutter=opt.line_numbers?ui_paint_measure_text_width(font,digits,scale)+padding*3:0;
  gutter=fminf(gutter,fmaxf(0,bounds.w-bar-padding*2-4));
  UIRect view={bounds.x+padding+gutter,bounds.y+padding,
               fmaxf(1,bounds.w-padding*2-gutter-bar),fmaxf(1,bounds.h-padding*2-bar)};
  bool allowed=ui_nav_allowed(ctx);
  bool hover=allowed && ui_pointer_hit(bounds.x,bounds.y,bounds.w,bounds.h);
  bool body=allowed && ui_pointer_hit(view.x,view.y,view.w,view.h);
  bool focused=ui_nav_register(ctx,id,bounds,UI_NAV_TEXT_EDITOR,true,hover);
  if(hover) frame->g_any_hover=true;
  if(!allowed || (nav->nav_enabled && !focused)) state->focused=false;
  bool reveal=false;
  if(focused && !state->focused && (nav->nav_edit || ui_nav_has_events(ctx,id))) {
    state->focused=true; nav->nav_editor_finished=false; reveal=true;
  }
  /* No whole-document Escape baseline: Escape never discards source edits. */
  state->needs_snapshot=false; free(state->original_long); state->original_long=NULL;
  if(ui_capture_widget_press(ctx)) {
    if(hover) { state->focused=true; nav->nav_editor_finished=false; }
    else state->focused=false;
  }
  if(!state->focused) { state->editor_selecting=false; state->editor_scrollbar_drag=0; }
  /* A pointer press establishes the insertion point before the frame's text
   * batch. Otherwise click+type in one frame would edit at the old caret. */
  bool start=ui_pointer_press(body,allowed);
  if(start) {
    int line=(int)clamp(floorf((frame->g_mouse_y-view.y+state->editor_scroll_y)/row),(float)(lines-1))+1;
    state->caret=state->selection_anchor=hit(font,buffer,line_offset(buffer,line),
      frame->g_mouse_x-view.x+state->editor_scroll_x,scale,tab);
    state->editor_selecting=true; state->preferred_column_valid=false; reveal=true;
  }
  size_t count; InputKeyEvent *events=ui_frame_events(&count);
  for(size_t i=0;i<count && state->focused;++i) {
    InputKeyEvent *e=&events[i];
    if(e->consumed || !ui_nav_event(ctx,id,i)) continue;
    if(e->kind==INPUT_KEY && e->action!=PLATFORM_PRESS && e->action!=PLATFORM_REPEAT) continue;
    e->consumed=true;
    bool ctrl=(e->mods&PLATFORM_MOD_CONTROL) && !(e->mods&PLATFORM_MOD_ALT);
    if(e->kind==INPUT_KEY && e->key==PLATFORM_KEY_ESCAPE && ui_capture_editor_escape_allowed(ctx)) {
      state->focused=false; ui_nav_editor_finish(ctx,id); ui_capture_mark_escape(ctx);
    } else if(e->kind==INPUT_KEY && ctrl && e->key==PLATFORM_KEY_S) result.save_requested=true;
    else if(!opt.read_only || readonly_event(e)) {
      ui_text_edit_multiline_event(state,buffer,capacity,e,&clip->clipboard,clip->clipboard_user,
                                   (int)fminf(UI_TEXT_EDITOR_MAX_CAPACITY,fmaxf(1,floorf(view.h/row)-1)));
      reveal=true;
    }
  }
  length=(int)strlen(buffer); lines=line_number(buffer,length);
  if(opt.reveal_line>0) {
    state->caret=state->selection_anchor=line_offset(buffer,opt.reveal_line);
    state->preferred_column_valid=false; reveal=true;
  }
  float widest=0;
  for(int at=0;;) {
    int end=ui_text_line_end(buffer,at);
    widest=fmaxf(widest,prefix(font,buffer,at,end,scale,tab));
    if(!buffer[end]) break;
    at=end+1;
  }
  float max_x=fmaxf(0,widest+padding-view.w),max_y=fmaxf(0,lines*row-view.h);
  state->editor_scroll_x=clamp(state->editor_scroll_x,max_x);
  state->editor_scroll_y=clamp(state->editor_scroll_y,max_y);
  if(hover && ui_capture_scroll_allowed(bounds) && !ui_scroll_wheel_blocked()) {
    float wheel=ui_scroll_delta();
    if(wheel && ui_consume_scroll_delta()) state->editor_scroll_y=clamp(state->editor_scroll_y-wheel*row*3,max_y);
  }
  /* Scrollbar track clicks center the thumb; dragging keeps its grab offset. */
  UIRect tracks[2]={{bounds.x+bounds.w-bar,view.y,bar,view.h},
                    {view.x,bounds.y+bounds.h-bar,view.w,bar}};
  float maxima[2]={max_y,max_x};
  for(int axis=0;axis<2;++axis) {
    UIRect track=tracks[axis]; float extent=axis?track.w:track.h;
    float visible=axis?view.w:view.h,maximum=maxima[axis];
    float thumb=fminf(extent,fmaxf(12,extent*visible/(maximum+visible)));
    float *offset=axis?&state->editor_scroll_x:&state->editor_scroll_y;
    float position=axis?frame->g_mouse_x-track.x:frame->g_mouse_y-track.y;
    float top=maximum>0?*offset/maximum*(extent-thumb):0;
    if(maximum>0 && ui_pointer_press(ui_pointer_hit(track.x,track.y,track.w,track.h),allowed)) {
      state->editor_scrollbar_drag=axis+1;
      state->editor_drag_offset=position>=top && position<=top+thumb?position-top:thumb*.5f;
    }
    if(state->editor_scrollbar_drag==axis+1) {
      if(!allowed || !frame->g_mouse_down || !state->focused || ui_capture_click_claimed(ctx)) state->editor_scrollbar_drag=0;
      else if(extent>thumb) *offset=clamp((position-state->editor_drag_offset)/(extent-thumb)*maximum,maximum);
    }
  }
  if(!frame->g_mouse_down || !state->focused || !allowed || ui_capture_click_claimed(ctx)) state->editor_selecting=false;
  if(state->editor_selecting && !start) {
    /* Drag outside the viewport scrolls while retaining a valid caret. */
    {
      float dy=frame->g_mouse_y<view.y?-row:frame->g_mouse_y>view.y+view.h?row:0;
      state->editor_scroll_y=clamp(state->editor_scroll_y+dy,max_y);
      float dx=frame->g_mouse_x<view.x?-tab:frame->g_mouse_x>view.x+view.w?tab:0;
      state->editor_scroll_x=clamp(state->editor_scroll_x+dx,max_x);
    }
    int line=(int)clamp(floorf((frame->g_mouse_y-view.y+state->editor_scroll_y)/row),(float)(lines-1))+1;
    int at=line_offset(buffer,line);
    state->caret=hit(font,buffer,at,frame->g_mouse_x-view.x+state->editor_scroll_x,scale,tab);
    reveal=true;
  }
  int caret_line=line_number(buffer,state->caret);
  if(reveal) {
    float top=(caret_line-1)*row;
    if(top<state->editor_scroll_y) state->editor_scroll_y=top;
    if(top+row>state->editor_scroll_y+view.h) state->editor_scroll_y=top+row-view.h;
    float x=prefix(font,buffer,ui_text_line_start(buffer,state->caret),state->caret,scale,tab);
    if(x<state->editor_scroll_x) state->editor_scroll_x=x;
    if(x+padding>state->editor_scroll_x+view.w) state->editor_scroll_x=x+padding-view.w;
    state->editor_scroll_x=clamp(state->editor_scroll_x,max_x);
    state->editor_scroll_y=clamp(state->editor_scroll_y,max_y);
  }
  ui_paint_draw_quad(bounds.x-1,bounds.y-1,bounds.w+2,bounds.h+2,theme.border.r,theme.border.g,theme.border.b);
  ui_paint_draw_quad(bounds.x,bounds.y,bounds.w,bounds.h,theme.field.r,theme.field.g,theme.field.b);
  bool painted=ui_paint_text_clip_push(view.x,view.y,view.w,view.h);
  if(painted) {
    int low=state->caret<state->selection_anchor?state->caret:state->selection_anchor;
    int high=state->caret>state->selection_anchor?state->caret:state->selection_anchor;
    int line=1;
    for(int at=0;;++line) {
      int end=ui_text_line_end(buffer,at);
      float y=view.y+(line-1)*row-state->editor_scroll_y;
      if(y+row>view.y && y<view.y+view.h) {
        if(opt.highlight_line==line)
          ui_paint_draw_quad_alpha(view.x,y,view.w,row,theme.warning.r,theme.warning.g,theme.warning.b,.18f);
        if(state->focused && high>at && low<=end && low!=high) {
          int a=low>at?low:at,b=high<end?high:end;
          float left=prefix(font,buffer,at,a,scale,tab);
          float right=prefix(font,buffer,at,b,scale,tab)+(high>end?tab*.25f:0);
          ui_paint_draw_quad_alpha(view.x+left-state->editor_scroll_x,y,right-left,row,
                                  theme.accent.r,theme.accent.g,theme.accent.b,.35f);
        }
        span(font,buffer,at,end,scale,tab,true,view.x-state->editor_scroll_x,y+ascent,theme.text);
        if(state->focused && caret_line==line) {
          float x=prefix(font,buffer,at,state->caret,scale,tab);
          ui_paint_draw_quad(view.x+x-state->editor_scroll_x,y,1,row,theme.accent.r,theme.accent.g,theme.accent.b);
        }
      }
      if(!buffer[end]) break;
      at=end+1;
    }
    ui_paint_text_clip_pop();
  }
  if(opt.line_numbers && ui_paint_text_clip_push(bounds.x,view.y,gutter+padding,view.h)) {
    int first=(int)(state->editor_scroll_y/row)+1;
    for(int line=first;line<=lines;++line) {
      float y=view.y+(line-1)*row-state->editor_scroll_y;
      if(y>=view.y+view.h) break;
      snprintf(digits,sizeof(digits),"%d",line);
      ui_paint_draw_text_colored(font,digits,bounds.x+padding,y+ascent,scale,
                                theme.text_muted.r,theme.text_muted.g,theme.text_muted.b);
    }
    ui_paint_text_clip_pop();
  }
  for(int axis=0;axis<2;++axis) if(maxima[axis]>0) {
    UIRect track=tracks[axis]; float extent=axis?track.w:track.h,visible=axis?view.w:view.h;
    float thumb=fminf(extent,fmaxf(12,extent*visible/(maxima[axis]+visible)));
    float offset=axis?state->editor_scroll_x:state->editor_scroll_y;
    float top=offset/maxima[axis]*(extent-thumb);
    if(axis) { track.x+=top; track.w=thumb; } else { track.y+=top; track.h=thumb; }
    ui_paint_draw_quad(track.x,track.y,track.w,track.h,theme.text_muted.r,theme.text_muted.g,theme.text_muted.b);
  }
  ui_text_edit_observe(state,buffer);
  result.changed=before!=hash(buffer); result.focused=state->focused;
  result.caret=state->caret; result.selection_anchor=state->selection_anchor;
  result.line=caret_line; result.column=ui_text_line_column(buffer,state->caret)+1; result.line_count=lines;
  ui_nav_ring(id,bounds);
  return result;
}
