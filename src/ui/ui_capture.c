#include "ui_state_text_fields_internal.h"
#include "ui_state_sliders_internal.h"
#include "ui_state_picker_internal.h"
#include "ui_state_panels_internal.h"
#include "ui_state_numeric_internal.h"
#include "ui_state_navigation_internal.h"
#include "ui_state_modals_internal.h"
#include "ui_state_menu_internal.h"
#include "ui_state_frame_internal.h"
#include "ui_state_dropdowns_internal.h"
#include "ui_state_debug_internal.h"
#include "ui_state_buttons_internal.h"
#include "ui/ui.h"
#include "ui_overlay_internal.h"
#include "ui_capture_internal.h"
#include "ui_widget_internal.h"
#include "ui_navigation_internal.h"
#include "log/log.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool point_in_rect(float px,float py,float x,float y,float w,float h) {
  return px>=x && px<=x+w && py>=y && py<=y+h;
}
bool ui_pointer_hit(float x, float y, float w, float h) {
  UIModalsState *modals_view=ui_state_modals(ui_overlay_context());
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

  if (modals_view->g_modal_scope_depth < modals_view->g_modal_open_count_prev)
    return false;
  if(!isfinite(x)||!isfinite(y)||!isfinite(w)||!isfinite(h)||w<=0||h<=0) return false;
  if(!point_in_rect(frame_view->g_mouse_x,frame_view->g_mouse_y,x,y,w,h)) return false;
  for(int i=0;i<frame_view->layout_input_clip_count;++i) {
    UIRect clip=frame_view->layout_input_clips[i];
    if(!point_in_rect(frame_view->g_mouse_x,frame_view->g_mouse_y,clip.x,clip.y,clip.w,clip.h)) return false;
  }
  return true;
}
bool ui_pointer_blocked_by_modal(void) {
  UIModalsState *modals_view=ui_state_modals(ui_overlay_context());

  return modals_view->g_modal_scope_depth < modals_view->g_modal_open_count_prev;
}
/* Field-like composed widgets retain the established press-edge behavior. */
bool ui_input_hover_click(UIRect rect,bool *clicked) {
  bool hover=ui_pointer_hit(rect.x,rect.y,rect.w,rect.h);
  if(hover) ui_state_frame(ui_overlay_context())->g_any_hover=true;
  if(clicked) *clicked=hover && ui_capture_widget_press(ui_overlay_context());
  return hover;
}
/* Shared gesture policy. Hit testing is handled by ui_pointer_hit; capture
 * can continue outside a hit region, but can only start on a press edge. */
static bool ui_pointer_interaction_allowed(bool enabled) {
  return enabled && !ui_state_navigation(ui_overlay_context())->nav_cancelled && ui_nav_allowed(ui_overlay_context()) &&
    !ui_pointer_blocked_by_modal() && !ui_capture_click_claimed(ui_overlay_context()) && !ui_menu_hierarchy_is_open();
}
bool ui_pointer_press(bool hover,bool enabled) {
  return hover && ui_state_frame(ui_overlay_context())->g_mouse_clicked && ui_pointer_interaction_allowed(enabled);
}
bool ui_pointer_activate(bool *pressed,bool hover,bool enabled) {
  if(!ui_pointer_interaction_allowed(enabled)) { *pressed=false; return false; }
  if(ui_pointer_press(hover,enabled)) *pressed=true;
  if(ui_state_frame(ui_overlay_context())->g_mouse_down) return false;
  bool activated=*pressed&&hover; *pressed=false; return activated;
}
bool ui_pointer_drag(bool *dragging,bool hover,bool enabled) {
  if(!ui_pointer_interaction_allowed(enabled)||!ui_state_frame(ui_overlay_context())->g_mouse_down) {
    *dragging=false; return false;
  }
  bool started=!*dragging&&ui_pointer_press(hover,enabled);
  if(started) *dragging=true;
  return started;
}
bool ui_key_pressed(int key) {
  UIContext *ctx=ui_overlay_context();

  UIFrameState *frame_view=ui_state_frame(ctx);

  size_t count=frame_view->key_event_count; InputKeyEvent *events=frame_view->key_events;
  for (size_t i=0;i<count;++i) {
    InputKeyEvent *e=&events[i];
    if (!e->consumed && e->kind==INPUT_KEY && e->key==key && e->action==PLATFORM_PRESS) {
      e->consumed=true; return true;
    }
  }
  return false;
}
void ui_set_scroll_delta(float delta) {
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

  frame_view->g_scroll_delta = isfinite(delta) ? delta : 0.0f;
  frame_view->g_scroll_consumed = false;
}

float ui_scroll_delta(void) {
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

 return frame_view->g_scroll_consumed ? 0.0f : frame_view->g_scroll_delta; }
bool ui_consume_scroll_delta(void) {
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

  if (frame_view->g_scroll_consumed || frame_view->g_scroll_delta == 0.0f) return false;
  frame_view->g_scroll_consumed = true;
  return true;
}

bool ui_any_slider_dragging(void) {
  UISlidersState *sliders_view=ui_state_sliders(ui_overlay_context());

  for (int i = 0; i < sliders_view->slider_state_count; ++i)
    if (sliders_view->slider_states[i].dragging) return true;
  return false;
}
bool ui_any_dropdown_is_open(void) {
  UIDropdownsState *dropdowns_view=ui_state_dropdowns(ui_overlay_context());

  for (int i = 0; i < dropdowns_view->dropdown_state_count; ++i) {
    if (dropdowns_view->dropdown_states[i].open)
      return true;
  }
  return false;
}

bool ui_scroll_wheel_blocked(void) {
  return ui_any_slider_dragging() || ui_any_dropdown_is_open() || ui_menu_hierarchy_is_open();
}

void ui_release_input_capture(void) {
  UINavigationState *navigation_view=ui_state_navigation(ui_overlay_context());
  UIButtonsState *buttons_view=ui_state_buttons(ui_overlay_context());
  UISlidersState *sliders_view=ui_state_sliders(ui_overlay_context());
  UIPanelsState *panels_view=ui_state_panels(ui_overlay_context());
  UITextFieldsState *text_fields_view=ui_state_text_fields(ui_overlay_context());
  UINumericState *numeric_view=ui_state_numeric(ui_overlay_context());

  navigation_view->nav_focus[0]=0;
  navigation_view->nav_enabled=false;
  navigation_view->nav_keyboard=false;
  for (int i = 0; i < buttons_view->ui_state_count; ++i)
    buttons_view->ui_states[i].is_pressed = false;
  for (int i = 0; i < sliders_view->slider_state_count; ++i)
    sliders_view->slider_states[i].dragging = false;
  for (int i = 0; i < panels_view->g_scroll_state_count; ++i)
    panels_view->g_scroll_states[i].dragging = false;
  for (int i = 0; i < text_fields_view->text_input_state_count; ++i) {
    text_fields_view->text_input_states[i].focused = false;
    free(text_fields_view->text_input_states[i].original_long); text_fields_view->text_input_states[i].original_long=NULL;
  }
  for (int i = 0; i < numeric_view->float_field_state_count; ++i)
    numeric_view->float_field_states[i].focused = false;
  ui_close_all_dropdowns();
}

/* Dropdown e campi editabili sono capture transitorie, non dialoghi: F1/F2
 * e Ctrl+S/Ctrl+L possono chiuderli in modo esplicito. Questi tre sistemi,
 * invece, rappresentano una scelta ancora pendente e restano esclusivi. */
bool ui_has_blocking_overlay(void) {
  if (ui_any_modal_open() || ui_file_picker_capture_active())
    return true;
  return ui_menu_hierarchy_is_open();
}

bool ui_has_input_capture(void) {
  return ui_has_pointer_capture() || ui_has_keyboard_capture();
}

bool ui_has_pointer_capture(void) {
  UIButtonsState *buttons_view=ui_state_buttons(ui_overlay_context());
  UISlidersState *sliders_view=ui_state_sliders(ui_overlay_context());
  UIPanelsState *panels_view=ui_state_panels(ui_overlay_context());

  bool active_widget = false;
  for (int i = 0; i < buttons_view->ui_state_count; ++i)
    active_widget |= buttons_view->ui_states[i].is_pressed;
  for (int i = 0; i < sliders_view->slider_state_count; ++i)
    active_widget |= sliders_view->slider_states[i].dragging;
  for (int i = 0; i < panels_view->g_scroll_state_count; ++i)
    active_widget |= panels_view->g_scroll_states[i].dragging;
  if (active_widget || ui_any_dropdown_is_open() || ui_any_modal_open() ||
      ui_file_picker_capture_active())
    return true;
  return ui_menu_hierarchy_is_open();
}

bool ui_has_keyboard_capture(void) {
  UINavigationState *navigation_view=ui_state_navigation(ui_overlay_context());
  UINumericState *numeric_view=ui_state_numeric(ui_overlay_context());

  if(navigation_view->nav_enabled && navigation_view->nav_focus[0]) return true;

  bool float_focused = false;
  for (int i = 0; i < numeric_view->float_field_state_count; ++i)
    float_focused |= numeric_view->float_field_states[i].focused;
  if (ui_any_text_input_focused() || float_focused ||
      ui_any_dropdown_is_open() || ui_any_modal_open() ||
      ui_file_picker_capture_active())
    return true;
  return ui_menu_hierarchy_is_open();
}

bool ui_get_input_diagnostics(UIInputDiagnostics *out) {
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());
  UIPickerState *picker_view=ui_state_picker(ui_overlay_context());
  UIMenuState *menu_view=ui_state_menu(ui_overlay_context());
  UIButtonsState *buttons_view=ui_state_buttons(ui_overlay_context());
  UISlidersState *sliders_view=ui_state_sliders(ui_overlay_context());
  UITextFieldsState *text_fields_view=ui_state_text_fields(ui_overlay_context());
  UINumericState *numeric_view=ui_state_numeric(ui_overlay_context());
  UIModalsState *modals_view=ui_state_modals(ui_overlay_context());
  UIDropdownsState *dropdowns_view=ui_state_dropdowns(ui_overlay_context());
  UIPanelsState *panels_view=ui_state_panels(ui_overlay_context());

  if (!out)
    return false;
  memset(out, 0, sizeof(*out));
  out->mouse_x = frame_view->g_mouse_x;
  out->mouse_y = frame_view->g_mouse_y;
  out->mouse_down = frame_view->g_mouse_down;
  out->mouse_pressed = frame_view->g_mouse_clicked;
  out->mouse_released = frame_view->g_mouse_released;
  out->pointer_capture = ui_has_pointer_capture();
  out->keyboard_capture = ui_has_keyboard_capture();
  out->any_capture = out->pointer_capture || out->keyboard_capture;
  out->escape_pressed = frame_view->g_escape_pressed;
  out->escape_consumed = frame_view->g_escape_consumed;
  out->file_picker_open = picker_view->file_picker_open;
  out->file_picker_id = picker_view->file_picker_open ? picker_view->file_picker_id : NULL;
  out->menu_depth = menu_view->g_menu.depth;
  out->input_region_count = frame_view->g_input_region_count;

  for (int i = buttons_view->ui_state_count - 1; i >= 0; --i) {
    if (!out->hovered_widget_id && buttons_view->ui_states[i].is_hover)
      out->hovered_widget_id = buttons_view->ui_states[i].id;
    if (!out->active_widget_id && buttons_view->ui_states[i].is_pressed)
      out->active_widget_id = buttons_view->ui_states[i].id;
  }
  for (int i = sliders_view->slider_state_count - 1; i >= 0; --i)
    if (sliders_view->slider_states[i].dragging) {
      out->active_widget_id = sliders_view->slider_states[i].id;
      break;
    }
  for (int i = text_fields_view->text_input_state_count - 1; i >= 0; --i)
    if (text_fields_view->text_input_states[i].focused) {
      out->focused_widget_id = text_fields_view->text_input_states[i].id;
      break;
    }
  if (!out->focused_widget_id)
    for (int i = numeric_view->float_field_state_count - 1; i >= 0; --i)
      if (numeric_view->float_field_states[i].focused) {
        out->focused_widget_id = numeric_view->float_field_states[i].id;
        break;
      }
  for (int i = modals_view->g_modal_state_count - 1; i >= 0; --i)
    if (modals_view->g_modal_states[i].wasOpen) {
      out->active_modal_id = modals_view->g_modal_states[i].id;
      break;
    }
  for (int i = dropdowns_view->dropdown_state_count - 1; i >= 0; --i)
    if (dropdowns_view->dropdown_states[i].open) {
      out->open_dropdown_id = dropdowns_view->dropdown_states[i].id;
      break;
    }
  for (int i = panels_view->g_scroll_state_count - 1; i >= 0; --i) {
    if (!out->scrollbar_drag_owner && panels_view->g_scroll_states[i].dragging)
      out->scrollbar_drag_owner = panels_view->g_scroll_states[i].id;
    if (!out->scroll_consumer && panels_view->g_scroll_states[i].wheel_consumed)
      out->scroll_consumer = panels_view->g_scroll_states[i].id;
  }
  for (int i = frame_view->g_input_region_count - 1; i >= 0; --i) {
    UIRect rect = frame_view->g_input_regions[i].rect;
    if (ui_pointer_hit(rect.x, rect.y, rect.w, rect.h)) {
      out->pointer_region_owner = frame_view->g_input_regions[i].id;
      break;
    }
  }
  out->highest_priority_owner =
      out->active_modal_id ? out->active_modal_id
      : out->file_picker_id ? out->file_picker_id
      : out->menu_depth > 0 ? menu_view->g_menu.levels[0].id
      : out->open_dropdown_id ? out->open_dropdown_id
      : out->active_widget_id ? out->active_widget_id
      : out->focused_widget_id ? out->focused_widget_id
      : out->pointer_region_owner;
  return true;
}

void ui_debug_log_input_events(void) {

#ifdef DEBUG

  UIDebugState *debug_view=ui_state_debug(ui_overlay_context());

  if (debug_view->debug_enabled < 0)
    debug_view->debug_enabled = getenv("UI_INPUT_DEBUG") ? 1 : 0;
  if (!debug_view->debug_enabled)
    return;

  bool key_down = ui_state_frame(ui_overlay_context())->debug_keys_down;
  bool key_pressed = key_down && !debug_view->debug_key_was_down;
  debug_view->debug_key_was_down = key_down;
  UIInputDiagnostics state;
  if (!ui_get_input_diagnostics(&state))
    return;
  const char *owner = state.highest_priority_owner
                          ? state.highest_priority_owner
                          : "world";
  bool transitioned = state.any_capture != debug_view->debug_previous_capture ||
                      strcmp(owner, debug_view->debug_previous_owner) != 0;
  if (state.mouse_pressed || state.mouse_released || key_pressed ||
      state.escape_pressed || transitioned) {
    LOG_INFO("[UI INPUT] mouse=(%.1f,%.1f) press=%d release=%d key=%d "
             "hover=%s active=%s focus=%s capture=%d/%d modal=%s picker=%s "
             "menu=%d dropdown=%s scroll-drag=%s scroll=%s regions=%d "
             "region=%s owner=%s escape=%d/%d",
             state.mouse_x, state.mouse_y, state.mouse_pressed,
             state.mouse_released, key_pressed,
             state.hovered_widget_id ? state.hovered_widget_id : "-",
             state.active_widget_id ? state.active_widget_id : "-",
             state.focused_widget_id ? state.focused_widget_id : "-",
             state.pointer_capture, state.keyboard_capture,
             state.active_modal_id ? state.active_modal_id : "-",
             state.file_picker_id ? state.file_picker_id : "-",
             state.menu_depth,
             state.open_dropdown_id ? state.open_dropdown_id : "-",
             state.scrollbar_drag_owner ? state.scrollbar_drag_owner : "-",
             state.scroll_consumer ? state.scroll_consumer : "-",
             state.input_region_count,
             state.pointer_region_owner ? state.pointer_region_owner : "-",
             owner, state.escape_pressed, state.escape_consumed);
  }
  debug_view->debug_previous_capture = state.any_capture;
  snprintf(debug_view->debug_previous_owner, sizeof(debug_view->debug_previous_owner), "%s", owner);
#endif
}

bool ui_file_picker_capture_active(void) { return ui_state_picker(ui_overlay_context())->file_picker_open; }
void ui_capture_begin_frame(UIContext *ctx) {
  UIFrameState *frame_view=ui_state_frame(ctx);
  UIDropdownsState *dropdowns_view=ui_state_dropdowns(ctx);
  UIMenuState *menu_view=ui_state_menu(ctx);

  frame_view->g_click_claimed_by_dropdown_list=false;
  dropdowns_view->g_dropdown_click_owner=NULL;
  frame_view->g_click_claimed_by_menu=frame_view->g_mouse_clicked && menu_view->g_menu.depth>0;
  if(!frame_view->g_mouse_clicked || menu_view->g_menu.depth>0) return;
  int order=-1;
  for(int n=0;n<dropdowns_view->dropdown_state_count;++n) {
    UIDropdownState *s=&dropdowns_view->dropdown_states[n];
    if(s->open && s->lastListH>0 && s->popupOrder>order) {
      order=s->popupOrder; dropdowns_view->g_dropdown_click_owner=s;
    }
  }
  frame_view->g_click_claimed_by_dropdown_list=dropdowns_view->g_dropdown_click_owner!=NULL;
}
bool ui_capture_click_claimed(const UIContext *ctx) {
  const UIFrameState *frame_view_read=ui_state_frame_read(ctx);

  return frame_view_read->g_click_claimed_by_dropdown_list || frame_view_read->g_click_claimed_by_menu;
}
bool ui_capture_keyboard_allowed(UIContext *ctx) {
  return !ui_state_navigation(ctx)->nav_cancelled && ui_state_menu(ctx)->g_menu.depth==0 && ui_nav_allowed(ctx);
}
bool ui_capture_dropdown_press(UIContext *ctx,const UIDropdownState *state,bool enabled) {
  UIModalsState *modals_view=ui_state_modals(ctx);
  UIDropdownsState *dropdowns_view=ui_state_dropdowns(ctx);

  return enabled && !ui_state_navigation(ctx)->nav_cancelled && ui_state_menu(ctx)->g_menu.depth==0 &&
    modals_view->g_modal_scope_depth>=modals_view->g_modal_open_count_prev && ui_state_frame(ctx)->g_mouse_clicked &&
    (!dropdowns_view->g_dropdown_click_owner || dropdowns_view->g_dropdown_click_owner==state);
}
void ui_capture_claim_dropdown(UIContext *ctx,UIDropdownState *state) {
  ui_state_dropdowns(ctx)->g_dropdown_click_owner=state; ui_state_frame(ctx)->g_click_claimed_by_dropdown_list=true;
}
void ui_capture_forget_dropdown(UIContext *ctx,const UIDropdownState *state) {
  UIDropdownsState *dropdowns_view=ui_state_dropdowns(ctx);

  if(!state || dropdowns_view->g_dropdown_click_owner==state) dropdowns_view->g_dropdown_click_owner=NULL;
}
void ui_capture_claim_menu(UIContext *ctx) {
  UIFrameState *frame_view=ui_state_frame(ctx);

  frame_view->g_mouse_clicked=false; frame_view->g_click_claimed_by_menu=true;
}
bool ui_capture_consume_escape(UIContext *ctx) {
  UIFrameState *frame_view=ui_state_frame(ctx);

  if(!frame_view->g_escape_pressed || frame_view->g_escape_consumed) return false;

  ui_capture_mark_escape(ctx);
  for(size_t n=0;n<frame_view->key_event_count;++n) {
    InputKeyEvent *e=&frame_view->key_events[n];
    if(!e->consumed && e->kind==INPUT_KEY && e->key==PLATFORM_KEY_ESCAPE && e->action==PLATFORM_PRESS) {
      e->consumed=true; break;
    }
  }
  return true;
}
void ui_capture_mark_escape(UIContext *ctx) { ui_state_frame(ctx)->g_escape_consumed=true; }
bool ui_capture_editor_escape_allowed(const UIContext *ctx) { return ui_state_menu_read(ctx)->g_menu.depth==0; }
bool ui_capture_modal_escape_allowed(UIContext *ctx,const char *id) {
  UINavigationState *navigation_view=ui_state_navigation(ctx);

  return (!navigation_view->nav_modal[0] || !strcmp(navigation_view->nav_modal,id)) && !ui_nav_editor_escape(ctx);
}

bool ui_capture_widget_press(UIContext *ctx) {
  return ui_state_frame(ctx)->g_mouse_clicked && !ui_state_navigation(ctx)->nav_cancelled && ui_state_menu(ctx)->g_menu.depth==0 &&
    !ui_capture_click_claimed(ctx);
}
bool ui_capture_scroll_allowed(UIRect viewport) {
  UIContext *ctx=ui_overlay_context();

  UIModalsState *modals_view=ui_state_modals(ctx);

  if(ui_scroll_wheel_blocked()) return false;
  if (modals_view->g_modal_rect_valid) {
    float cx = viewport.x + viewport.w * 0.5f, cy = viewport.y + viewport.h * 0.5f;
    bool inside = cx >= modals_view->g_modal_rect.x && cx <= modals_view->g_modal_rect.x + modals_view->g_modal_rect.w &&
                  cy >= modals_view->g_modal_rect.y && cy <= modals_view->g_modal_rect.y + modals_view->g_modal_rect.h;
    if (!inside) return false;
  }
  return true;
}
