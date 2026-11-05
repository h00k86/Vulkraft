#include "ui_widget_internal.h"
#include "ui_state_text_fields_internal.h"
#include "ui_state_panels_internal.h"
#include "ui_state_numeric_internal.h"
#include "ui_state_navigation_internal.h"
#include "ui_state_modals_internal.h"
#include "ui_state_menu_internal.h"
#include "ui_state_frame_internal.h"
#include "ui_state_dropdowns_internal.h"
#include "ui_navigation_internal.h"
#include "ui_capture_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Previous-frame declarations route events before immediate-mode widgets run;
 * current declarations validate focus and determine the next traversal order. */
const char *ui_nav_scope(UIContext *c) {
  UINavigationState *navigation_view=ui_state_navigation(c);

  return navigation_view->nav_scope_depth?navigation_view->nav_scopes[navigation_view->nav_scope_depth-1]:"";
}
bool ui_nav_allowed(UIContext *c) {
  UINavigationState *navigation_view=ui_state_navigation(c);

  return !navigation_view->nav_cancelled && !strcmp(ui_nav_scope(c),navigation_view->nav_modal);
}
void ui_nav_set(UIContext *c, const char *id,bool keyboard) {
  UINavigationState *navigation_view=ui_state_navigation(c);

  if(strcmp(navigation_view->nav_focus,id)) navigation_view->nav_edit=true;
  snprintf(navigation_view->nav_focus,sizeof(navigation_view->nav_focus),"%s",id);
  navigation_view->nav_enabled=true; navigation_view->nav_keyboard=keyboard;
}
int ui_nav_find(UIContext *c, const UINavigationEntry *entries,int count,const char *id) {
  for(int n=0;n<count;++n)
    if(!strcmp(entries[n].id,id) && !strcmp(entries[n].modal,ui_state_navigation(c)->nav_modal)) return n;
  return -1;
}
static void ui_nav_step(UIContext *c, const UINavigationEntry *entries,int count,int direction) {
  UINavigationState *navigation_view=ui_state_navigation(c);

  int start=ui_nav_find(c, entries,count,navigation_view->nav_focus);
  if(start<0) start=direction>0?-1:count;
  for(int n=1;n<=count;++n) {
    int at=(start+direction*n+count*2)%count;
    if(!strcmp(entries[at].modal,navigation_view->nav_modal)) {
      ui_nav_set(c, entries[at].id,true); return;
    }
  }
  navigation_view->nav_pending_step=direction;
}
bool ui_nav_reserve_events(UIContext *c,size_t count) {
  UINavigationState *state=ui_state_navigation(c);
  if(count>INPUT_KEYBOARD_CAPACITY) return false;
  if(count<=state->nav_event_capacity) return true;
  size_t capacity=state->nav_event_capacity?state->nav_event_capacity:1;
  while(capacity<count) capacity*=2;
  char (*owners)[UI_ID_CAPACITY]=realloc(state->nav_event_owner,capacity*sizeof(*owners));
  if(!owners) return false;
  state->nav_event_owner=owners; state->nav_event_capacity=capacity;
  return true;
}
bool ui_nav_begin(UIContext *c, bool cancelled, bool menu_open) {
  UINavigationState *navigation_view=ui_state_navigation(c);
  UIFrameState *frame_view=ui_state_frame(c);

  if(!ui_nav_reserve_events(c,frame_view->key_event_count)) return false;
  navigation_view->nav_count=0; navigation_view->nav_scope_depth=0; navigation_view->nav_scroll_depth=0;
  navigation_view->nav_edit=false; navigation_view->nav_cancelled=cancelled; navigation_view->nav_pending_step=0;
  navigation_view->nav_pointer_claimed=false;
  if(cancelled) { navigation_view->nav_focus[0]=0; navigation_view->nav_enabled=false; navigation_view->nav_keyboard=false; }
  for(size_t n=0;n<frame_view->key_event_count;++n) {
    InputKeyEvent *e=&frame_view->key_events[n];
    if(!e->consumed && e->kind==INPUT_KEY && e->key==PLATFORM_KEY_TAB &&
       (e->action==PLATFORM_PRESS || e->action==PLATFORM_REPEAT) &&
       !(e->mods&(PLATFORM_MOD_CONTROL|PLATFORM_MOD_ALT|PLATFORM_MOD_SUPER)) &&
       !menu_open) {
      e->consumed=true;
      ui_nav_step(c, navigation_view->nav_previous,navigation_view->nav_previous_count,(e->mods&PLATFORM_MOD_SHIFT)?-1:1);
    }
    snprintf(navigation_view->nav_event_owner[n],sizeof(navigation_view->nav_event_owner[n]),"%s",navigation_view->nav_focus);
  }
  return true;
}
bool ui_nav_event(UIContext *c, const char *id,size_t n) {
  UINavigationState *navigation_view=ui_state_navigation(c);

  if(!ui_state_frame(c)->frame_active) return ui_state_menu(c)->g_menu.depth==0; /* Legacy direct-widget adapters. */
  return ui_capture_keyboard_allowed(c) && (!navigation_view->nav_enabled || !strcmp(navigation_view->nav_event_owner[n],id));
}
bool ui_nav_has_events(UIContext *c, const char *id) {
  UIFrameState *frame_view=ui_state_frame(c);

  if(!ui_state_navigation(c)->nav_enabled || !frame_view->frame_active) return false;

  for(size_t n=0;n<frame_view->key_event_count;++n)
    if(!frame_view->key_events[n].consumed && ui_nav_event(c, id,n) &&
       (frame_view->key_events[n].kind==INPUT_CHARACTER || frame_view->key_events[n].action==PLATFORM_PRESS ||
        frame_view->key_events[n].action==PLATFORM_REPEAT)) return true;
  return false;
}
bool ui_nav_focused(UIContext *c, const char *id) {
  return ui_nav_allowed(c) && !strcmp(ui_state_navigation(c)->nav_focus,id);
}
bool ui_nav_key(UIContext *c, const char *id,int key,bool repeat) {
  UIFrameState *frame_view=ui_state_frame(c);

  if(!frame_view->frame_active) return false;

  for(size_t n=0;n<frame_view->key_event_count;++n) {
    InputKeyEvent *e=&frame_view->key_events[n];
    if(!e->consumed && e->kind==INPUT_KEY && e->key==key &&
       (e->action==PLATFORM_PRESS || (repeat && e->action==PLATFORM_REPEAT)) &&
       ui_state_navigation(c)->nav_enabled && ui_nav_event(c, id,n)) { e->consumed=true; return true; }
  }
  return false;
}
bool ui_nav_activate(UIContext *c, const char *id) {
  return ui_nav_key(c, id,PLATFORM_KEY_ENTER,false) || ui_nav_key(c, id,PLATFORM_KEY_KP_ENTER,false) ||
         ui_nav_key(c, id,PLATFORM_KEY_SPACE,false);
}
bool ui_nav_register(UIContext *c, const char *id,UIRect rect,int kind,bool enabled,bool hover) {
  UIFrameState *frame_view=ui_state_frame(c);
  UINavigationState *navigation_view=ui_state_navigation(c);

  if(!enabled || !ui_valid_id(id) || !frame_view->frame_active || rect.w<=0 || rect.h<=0) return false;

  if(navigation_view->nav_count<MAX_UI_ELEMENTS && ui_nav_find(c, navigation_view->nav_current,navigation_view->nav_count,id)<0) {
    UINavigationEntry *entry=&navigation_view->nav_current[navigation_view->nav_count++];
    *entry=(UINavigationEntry){.kind=kind,.scroll_index=-1,.height=rect.h};
    snprintf(entry->id,sizeof(entry->id),"%s",id);
    snprintf(entry->modal,sizeof(entry->modal),"%s",ui_nav_scope(c));
    if(navigation_view->nav_scroll_depth) {
      int depth=navigation_view->nav_scroll_depth-1, index=navigation_view->nav_scroll_stack[depth];
      entry->scroll_index=index;
      entry->content_y=rect.y-navigation_view->nav_scroll_viewports[depth].y+ui_state_panels(c)->g_scroll_states[index].offset;
    }
  }
  if(!ui_nav_allowed(c)) return false;
  if(navigation_view->nav_auto) { ui_nav_set(c, id,true); navigation_view->nav_auto=false; }
  if(hover && ui_capture_widget_press(c)) {
    navigation_view->nav_pointer_claimed=true;
    ui_nav_set(c, id,false);
    for(size_t n=0;n<frame_view->key_event_count;++n)
      if(!frame_view->key_events[n].consumed)
        snprintf(navigation_view->nav_event_owner[n],sizeof(navigation_view->nav_event_owner[n]),"%s",id);
  }
  return ui_nav_focused(c, id);
}
void ui_nav_restore(UIContext *c, UIModalOpenState *modal) {
  UINavigationState *navigation_view=ui_state_navigation(c);

  if(strcmp(navigation_view->nav_modal,modal->id)) return;

  snprintf(navigation_view->nav_modal,sizeof(navigation_view->nav_modal),"%s",modal->return_modal);
  if(!navigation_view->nav_cancelled) ui_nav_set(c, modal->return_focus,true);
  else { navigation_view->nav_focus[0]=0; navigation_view->nav_enabled=false; }
  navigation_view->nav_auto=false;
}
void ui_nav_modal_enter(UIContext *c, UIModalOpenState *modal) {
  UINavigationState *navigation_view=ui_state_navigation(c);

  if(!modal->wasOpen) {
    snprintf(modal->return_focus,sizeof(modal->return_focus),"%s",navigation_view->nav_focus);
    snprintf(modal->return_modal,sizeof(modal->return_modal),"%s",navigation_view->nav_modal);
    snprintf(navigation_view->nav_modal,sizeof(navigation_view->nav_modal),"%s",modal->id);
    navigation_view->nav_focus[0]=0; navigation_view->nav_auto=!navigation_view->nav_cancelled;
    ui_nav_clear_editor_focus(c);
  }
  if(navigation_view->nav_scope_depth<MAX_UI_MODALS)
    snprintf(navigation_view->nav_scopes[navigation_view->nav_scope_depth++],UI_ID_CAPACITY,"%s",modal->id);
}
bool ui_nav_editor_escape(UIContext *c) {
  UITextFieldsState *text_fields_view=ui_state_text_fields(c);
  UINavigationState *navigation_view=ui_state_navigation(c);
  UINumericState *numeric_view=ui_state_numeric(c);
  UIDropdownsState *dropdowns_view=ui_state_dropdowns(c);

  for(int n=0;n<text_fields_view->text_input_state_count;++n)
    if(text_fields_view->text_input_states[n].focused && (!navigation_view->nav_enabled || !strcmp(navigation_view->nav_focus,text_fields_view->text_input_states[n].id))) return true;
  for(int n=0;n<numeric_view->float_field_state_count;++n)
    if(numeric_view->float_field_states[n].focused && (!navigation_view->nav_enabled || !strcmp(navigation_view->nav_focus,numeric_view->float_field_states[n].id))) return true;
  int at=ui_nav_find(c, navigation_view->nav_previous,navigation_view->nav_previous_count,navigation_view->nav_focus);
  if(navigation_view->nav_edit && at>=0 && (navigation_view->nav_previous[at].kind==UI_NAV_TEXT || navigation_view->nav_previous[at].kind==UI_NAV_NUMBER)) return true;
  for(int n=0;n<dropdowns_view->dropdown_state_count;++n) if(dropdowns_view->dropdown_states[n].open) return true;
  return false;
}
void ui_nav_end(UIContext *c) {
  UINavigationState *navigation_view=ui_state_navigation(c);
  UIModalsState *modals_view=ui_state_modals(c);
  UITextFieldsState *text_fields_view=ui_state_text_fields(c);
  UINumericState *numeric_view=ui_state_numeric(c);

  if(ui_state_frame(c)->g_mouse_clicked && !navigation_view->nav_pointer_claimed && !navigation_view->nav_modal[0]) {
    navigation_view->nav_focus[0]=0; navigation_view->nav_enabled=false; navigation_view->nav_keyboard=false;
    ui_nav_clear_editor_focus(c);
  }
  /* Also handle a modal removed by the caller without another begin/end. */
  for(int pass=0;pass<MAX_UI_MODALS;++pass) {
    bool restored=false;
    for(int n=0;n<modals_view->g_modal_state_count;++n) {
      UIModalOpenState *m=&modals_view->g_modal_states[n];
      if(!m->wasOpen && navigation_view->nav_modal[0] && !strcmp(navigation_view->nav_modal,m->id)) {
        ui_nav_restore(c, m); restored=true; break;
      }
    }
    if(!restored) break;
  }
  if(navigation_view->nav_pending_step) ui_nav_step(c, navigation_view->nav_current,navigation_view->nav_count,navigation_view->nav_pending_step);
  if(navigation_view->nav_focus[0] && ui_nav_find(c, navigation_view->nav_current,navigation_view->nav_count,navigation_view->nav_focus)<0) {
    /* Preserve position when a focused widget disappears or becomes disabled. */
    int old=ui_nav_find(c, navigation_view->nav_previous,navigation_view->nav_previous_count,navigation_view->nav_focus);
    navigation_view->nav_focus[0]=0;
    for(int n=old+1;n<navigation_view->nav_previous_count;++n) {
      int at=ui_nav_find(c, navigation_view->nav_current,navigation_view->nav_count,navigation_view->nav_previous[n].id);
      if(at>=0) { ui_nav_set(c, navigation_view->nav_current[at].id,true); break; }
    }
    if(!navigation_view->nav_focus[0]) ui_nav_step(c, navigation_view->nav_current,navigation_view->nav_count,1);
  }
  if(navigation_view->nav_enabled) {
    for(int n=0;n<text_fields_view->text_input_state_count;++n) {
      UITextInputState *s=&text_fields_view->text_input_states[n];
      if(strcmp(s->id,navigation_view->nav_focus)) {
        s->focused=false; free(s->original_long); s->original_long=NULL;
      }
    }
    for(int n=0;n<numeric_view->float_field_state_count;++n)
      if(strcmp(numeric_view->float_field_states[n].id,navigation_view->nav_focus)) numeric_view->float_field_states[n].focused=false;
  }
  memcpy(navigation_view->nav_previous,navigation_view->nav_current,(size_t)navigation_view->nav_count*sizeof(*navigation_view->nav_current));
  navigation_view->nav_previous_count=navigation_view->nav_count;
}

void ui_nav_clear_editor_focus(UIContext *c) {
  UINumericState *numeric_view=ui_state_numeric(c);
  UITextFieldsState *text_fields_view=ui_state_text_fields(c);

  for (int i=0;i<numeric_view->float_field_state_count;++i) numeric_view->float_field_states[i].focused=false;
  for (int i=0;i<text_fields_view->text_input_state_count;++i) {
    text_fields_view->text_input_states[i].focused=false;
    free(text_fields_view->text_input_states[i].original_long); text_fields_view->text_input_states[i].original_long=NULL;
  }
}
