#include "ui/ui.h"
#include "ui_widget_internal.h"
#include "ui_paint_internal.h"
#include "ui_navigation_internal.h"
#include "ui_interaction_internal.h"
#include "ui_geometry_internal.h"
#include "ui_overlay_internal.h"
#include "ui_state_buttons_internal.h"
#include "ui_state_panels_internal.h"
#include "ui_state_frame_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct { const UIListItem *list; UITreeNode *tree; int count; } UICollectionRows;
static const char *row_id(UICollectionRows r,int i) { return r.tree?r.tree[i].id:r.list[i].id; }
static const char *row_label(UICollectionRows r,int i) { return r.tree?r.tree[i].label:r.list[i].label; }
static bool row_disabled(UICollectionRows r,int i) { return r.tree?r.tree[i].disabled:r.list[i].disabled; }
static int row_depth(UICollectionRows r,int i) { return r.tree?r.tree[i].depth:0; }
static bool row_branch(UICollectionRows r,int i) { return r.tree&&i+1<r.count&&r.tree[i+1].depth>r.tree[i].depth; }
/* A collapsed subtree is skipped in one pass, including disabled descendants. */
static bool row_visible(UICollectionRows r,int i,int *hidden) {
  int depth=row_depth(r,i);
  if(*hidden>=0&&depth>*hidden) return false;
  *hidden=-1;
  if(row_branch(r,i)&&!r.tree[i].expanded) *hidden=depth;
  return true;
}
static void neighbors(UICollectionRows rows,int selected,int *first,int *last,int *previous,int *next) {
  *first=*last=*previous=*next=-1; int hidden=-1;
  for(int i=0;i<rows.count;++i) if(row_visible(rows,i,&hidden)&&!row_disabled(rows,i)) {
    if(*first<0) *first=i;
    *last=i;
    if(i<selected) *previous=i;
    if(i>selected&&*next<0) *next=i;
  }
}
static UICollectionResult collection(const char *id,Font *font,UIRect bounds,
                                      UICollectionRows rows,int *selected,const UITreeOptions *options) {
  UICollectionResult result={.activated_index=-1,.clicked_index=-1,.accessory_index=-1};
  if(!font||!selected||!ui_valid_id(id)||!ui_layout_rect_valid(bounds)||rows.count<0||
     (rows.count&&!rows.list&&!rows.tree)) return result;
  for(int i=0;i<rows.count;++i) {
    if(!row_id(rows,i)||!row_label(rows,i)||!ui_valid_id(row_id(rows,i))) return result;
    int depth=row_depth(rows,i);
    if(depth<0||depth>64||(i==0&&depth!=0)||(i&&depth>row_depth(rows,i-1)+1)) return result;
  }
  if(options) {
    if(options->accessory_count<0||(options->accessory_count&&!options->accessories)||
       !isfinite(options->accessory_width)||options->accessory_width<0) return result;
    for(int i=0;i<options->accessory_count;++i) {
      const UITreeAccessory *a=&options->accessories[i];
      if(a->node_index<0||a->node_index>=rows.count||!ui_valid_id(a->id)||
         a->icon.kind<UI_ICON_NONE||a->icon.kind>=UI_ICON_COUNT) return result;
      if(a->icon.kind==UI_ICON_TEXTURE&&(a->icon.texture_slot<0||
         !isfinite(a->icon.u0)||!isfinite(a->icon.v0)||!isfinite(a->icon.u1)||!isfinite(a->icon.v1)||
         a->icon.u0<0||a->icon.v0<0||a->icon.u1>1||a->icon.v1>1||a->icon.u1<=a->icon.u0||a->icon.v1<=a->icon.v0)) return result;
    }
  }
  if(!ui_push_id(id)) return result;
  char view_id[UI_ID_CAPACITY]; snprintf(view_id,sizeof(view_id),"%s",ui_scoped_id("view"));
  char scroll_id[UI_ID_CAPACITY]; snprintf(scroll_id,sizeof(scroll_id),"%s",ui_scoped_id("scroll"));
  if(!view_id[0]||!scroll_id[0]) { ui_pop_id(); return result; }
  int old=*selected;
  if(*selected< -1||*selected>=rows.count||(*selected>=0&&row_disabled(rows,*selected))) *selected=-1;
  int first=-1,last=-1,previous=-1,next=-1,hidden=-1,parent=-1;
  int visible_count=0,selected_row=-1;
  for(int i=0;i<rows.count;++i) {
    if(!row_visible(rows,i,&hidden)) {
      if(i==*selected) *selected=parent>=0&&!row_disabled(rows,parent)?parent:-1;
      continue;
    }
    parent=i;
    if(i==*selected) selected_row=visible_count;
    ++visible_count;
    if(row_disabled(rows,i)) continue;
    if(first<0) first=i;
    last=i;
    if(i<*selected) previous=i;
    if(i>*selected&&next<0) next=i;
  }
  UIContext *ctx=ui_overlay_context();
  bool hover=ui_pointer_hit(bounds.x,bounds.y,bounds.w,bounds.h);
  ui_nav_register(ctx,view_id,bounds,UI_NAV_BUTTON,first>=0,hover);
  bool keyboard=false;
  size_t event_count; InputKeyEvent *events=ui_frame_events(&event_count);
  for(size_t n=0;n<event_count&&first>=0;++n) {
    InputKeyEvent *event=&events[n];
    if(event->consumed||event->kind!=INPUT_KEY||
       (event->action!=PLATFORM_PRESS&&event->action!=PLATFORM_REPEAT)) continue;
    int key=event->key;
    bool activate=key==PLATFORM_KEY_ENTER||key==PLATFORM_KEY_KP_ENTER||key==PLATFORM_KEY_SPACE;
    bool recognized=activate||key==PLATFORM_KEY_UP||key==PLATFORM_KEY_DOWN||
      key==PLATFORM_KEY_HOME||key==PLATFORM_KEY_END||
      (rows.tree&&(key==PLATFORM_KEY_LEFT||key==PLATFORM_KEY_RIGHT));
    if(!recognized||!ui_nav_key(ctx,view_id,key,!activate)) continue;
    neighbors(rows,*selected,&first,&last,&previous,&next);
    if(activate) { if(*selected>=0) result.activated_index=*selected; continue; }
    keyboard=true;
    if(key==PLATFORM_KEY_UP) *selected=previous>=0?previous:first;
    if(key==PLATFORM_KEY_DOWN) *selected=next>=0?next:last;
    if(key==PLATFORM_KEY_HOME) *selected=first;
    if(key==PLATFORM_KEY_END) *selected=last;
    if(rows.tree&&*selected>=0) {
      int i=*selected;
      if(key==PLATFORM_KEY_LEFT) {
        if(row_branch(rows,i)&&rows.tree[i].expanded) { rows.tree[i].expanded=false; result.expansion_changed=true; }
        else for(int j=i-1;j>=0;--j) if(rows.tree[j].depth<rows.tree[i].depth&&!rows.tree[j].disabled) {
          *selected=j; break;
        }
      }
      if(key==PLATFORM_KEY_RIGHT&&row_branch(rows,i)) {
        if(!rows.tree[i].expanded) { rows.tree[i].expanded=true; result.expansion_changed=true; }
        else for(int j=i+1;j<rows.count&&rows.tree[j].depth>rows.tree[i].depth;++j)
          if(!rows.tree[j].disabled) { *selected=j; break; }
      }
    }
  }
  UITheme theme=ui_get_theme(); float height=theme.row_height;
  if(!isfinite(height)||height<=0) height=26;
  visible_count=0; hidden=-1;
  for(int i=0;i<rows.count;++i) if(row_visible(rows,i,&hidden)) {
    if(i==*selected) selected_row=visible_count;
    ++visible_count;
  }
  /* Prime content metrics before begin so first-frame wheel/scrollbar and
   * keyboard reveal use this frame's rows rather than a stale list length. */
  UIPanelsState *panels=ui_state_panels(ctx);
  for(int i=0;i<panels->g_scroll_state_count;++i) if(!strcmp(panels->g_scroll_states[i].id,scroll_id))
    panels->g_scroll_states[i].content_height=visible_count*height;
  UIScrollScope scroll=ui_scroll_begin("scroll",bounds,1);
  if(scroll.state_index>=0) {
    UIScrollState *s=&panels->g_scroll_states[scroll.state_index];
    s->content_height=visible_count*height;
    if(keyboard&&selected_row>=0) {
      float top=selected_row*height;
      if(top<s->offset) s->offset=top;
      if(top+height>s->offset+bounds.h) s->offset=top+height-bounds.h;
    }
    s->offset=fmaxf(0,fminf(fmaxf(0,s->content_height-bounds.h),s->offset));
    scroll.offset=s->offset;
  }
  int row=0; hidden=-1;
  for(int i=0;i<rows.count;++i) {
    if(!row_visible(rows,i,&hidden)) continue;
    UIRect r={bounds.x,bounds.y+(row++)*height-scroll.offset,scroll.layout.bounds.w,height};
    if(!scroll.clip_pushed||r.y+r.h<=bounds.y||r.y>=bounds.y+bounds.h) continue;
    if(!ui_push_id(row_id(rows,i))) continue;
    bool enabled=!row_disabled(rows,i),branch=row_branch(rows,i);
    float indent=fminf(row_depth(rows,i)*theme.indent,fmaxf(0,r.w-24));
    UIRect arrow={r.x+indent,r.y,fminf(20,r.w),r.h};
    bool over=enabled&&ui_pointer_hit(r.x,r.y,r.w,r.h)&&ui_nav_allowed(ctx);
    int action_count=0;
    if(options) for(int a=0;a<options->accessory_count;++a)
      if(options->accessories[a].node_index==i) ++action_count;
    float action_width=options&&options->accessory_width>0?options->accessory_width:height;
    if(action_count) action_width=fminf(action_width,r.w/action_count);
    float action_left=r.x+r.w-action_count*action_width;
    bool over_body=over&&ui_state_frame(ctx)->g_mouse_x<action_left;
    bool over_arrow=over_body&&branch&&ui_pointer_hit(arrow.x,arrow.y,arrow.w,arrow.h);
    UIState *body=ui_button_state(ui_scoped_id("body"));
    UIState *toggle=branch?ui_button_state(ui_scoped_id("toggle")):NULL;
    if(over) ui_state_frame(ctx)->g_any_hover=true;
    if(toggle&&ui_pointer_activate(&toggle->is_pressed,over_arrow,enabled)) {
      rows.tree[i].expanded=!rows.tree[i].expanded; result.expansion_changed=true;
      if(!rows.tree[i].expanded&&*selected>i) {
        int j=i+1; while(j<rows.count&&rows.tree[j].depth>rows.tree[i].depth) ++j;
        if(*selected<j) *selected=i;
      }
    }
    if(body&&ui_pointer_activate(&body->is_pressed,over_body&&!over_arrow,enabled)) {
      *selected=i; result.clicked_index=i;
    }
    UIColor bg=i==*selected?theme.selected:over?theme.button_hover:theme.panel;
    ui_paint_draw_quad(r.x,r.y,r.w,r.h,bg.r,bg.g,bg.b);
    UIColor fg=enabled?theme.text:theme.text_muted;
    UIRect body_clip={r.x,r.y,fmaxf(0,action_left-r.x),r.h};
    bool body_visible=ui_clip_push(body_clip);
    if(branch&&body_visible) {
      float cx=arrow.x+arrow.w*.5f,cy=r.y+r.h*.5f;
      ui_paint_draw_quad(cx-4,cy-1,8,2,fg.r,fg.g,fg.b);
      if(!rows.tree[i].expanded) ui_paint_draw_quad(cx-1,cy-4,2,8,fg.r,fg.g,fg.b);
    }
    float tx=r.x+indent+(rows.tree?22:6),available=fmaxf(0,action_left-tx-6);
    char label[512]={0};
    if(available>0) ui_ellipsize_right(font,row_label(rows,i),14,available,label,sizeof(label));
    if(body_visible) {
      ui_paint_draw_text_colored(font,label,tx,r.y+(r.h+ui_paint_font_ascent(font,14))*.5f,14,fg.r,fg.g,fg.b);
      ui_clip_pop();
    }
    if(options&&action_count&&ui_push_id("actions")) {
      int position=0;
      for(int a=0;a<options->accessory_count;++a) {
        const UITreeAccessory *accessory=&options->accessories[a];
        if(accessory->node_index!=i) continue;
        UIRect button={action_left+(position++)*action_width,r.y,action_width,r.h};
        UIButtonStyle style={.disabled=!enabled||accessory->disabled,.selected=accessory->selected};
        char action_id[UI_ID_CAPACITY]; snprintf(action_id,sizeof(action_id),"%s",ui_scoped_id(accessory->id));
        if(ui_icon_button(action_id,font,button,&accessory->icon,accessory->label,&style)) result.accessory_index=a;
        if(accessory->tooltip&&!style.disabled) ui_tooltip(action_id,font,button,accessory->tooltip,.5f);
      }
      ui_pop_id();
    }
    ui_pop_id();
    /* Expansion may change during drawing; update hiding immediately. */
    if(branch) hidden=rows.tree[i].expanded?-1:rows.tree[i].depth;
  }
  scroll.layout.cursor_y=bounds.y+row*height-scroll.offset;
  ui_scroll_end(&scroll); ui_nav_ring(view_id,bounds); ui_pop_id();
  result.selection_changed=old!=*selected; return result;
}
UICollectionResult ui_list(const char *id,Font *font,UIRect bounds,
                            const UIListItem *items,int count,int *selected) {
  return collection(id,font,bounds,(UICollectionRows){.list=items,.count=count},selected,NULL);
}
UICollectionResult ui_tree(const char *id,Font *font,UIRect bounds,
                            UITreeNode *nodes,int count,int *selected) {
  return collection(id,font,bounds,(UICollectionRows){.tree=nodes,.count=count},selected,NULL);
}

UICollectionResult ui_tree_ex(const char *id,Font *font,UIRect bounds,
                               UITreeNode *nodes,int count,int *selected,
                               const UITreeOptions *options) {
  return collection(id,font,bounds,(UICollectionRows){.tree=nodes,.count=count},selected,options);
}
