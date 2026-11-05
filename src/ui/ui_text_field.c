#include "ui_state_text_fields_internal.h"
#include "ui_state_navigation_internal.h"
#include "ui_state_frame_internal.h"
#include "ui_state_clipboard_internal.h"
#include "ui/ui.h"
#include "ui_error_internal.h"
#include "ui_overlay_internal.h"
#include "ui_capture_internal.h"
#include "ui_paint_internal.h"
#include "ui_widget_internal.h"
#include "ui_interaction_internal.h"
#include "ui_navigation_internal.h"
#include "ui_geometry_internal.h"
#include "text/utf8.h"
#include "log/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <errno.h>
UITextInputState *ui_text_field_state(const char *id) {
  UITextFieldsState *text_fields_view=ui_state_text_fields(ui_overlay_context());
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

  if (!ui_valid_id(id)) {
    ui_report_error(ui_overlay_context(),UI_ERROR_ARGUMENT,UI_OPERATION_TEXT_FIELD,id); return NULL;
  }
  for (int i = 0; i < text_fields_view->text_input_state_count; i++) {
    if (strcmp(text_fields_view->text_input_states[i].id, id) == 0) {
      text_fields_view->text_input_states[i].last_seen_frame = frame_view->g_ui_frame_serial;
      return &text_fields_view->text_input_states[i];
    }
  }
  if (text_fields_view->text_input_state_count < MAX_UI_TEXT_INPUTS) {
    UITextInputState *s = &text_fields_view->text_input_states[text_fields_view->text_input_state_count++];
    memset(s, 0, sizeof(*s));
    snprintf(s->id, sizeof(s->id), "%s", id);
    s->last_seen_frame = frame_view->g_ui_frame_serial;
    return s;
  }
  UITextInputState *reuse = NULL;
  for (int i = 0; i < text_fields_view->text_input_state_count; ++i) {
    UITextInputState *candidate = &text_fields_view->text_input_states[i];
    if (candidate->focused ||
        candidate->last_seen_frame == frame_view->g_ui_frame_serial)
      continue;
    if (!reuse || candidate->last_seen_frame < reuse->last_seen_frame)
      reuse = candidate;
  }
  if (reuse) {
    free(reuse->original_long);
    ui_text_history_clear(reuse);
    memset(reuse, 0, sizeof(*reuse));
    snprintf(reuse->id, sizeof(reuse->id), "%s", id);
    reuse->last_seen_frame = frame_view->g_ui_frame_serial;
    return reuse;
  }

  ui_report_error(ui_overlay_context(),UI_ERROR_LIMIT,UI_OPERATION_TEXT_FIELD,id);
  if (!text_fields_view->warned_text_capacity) {
    LOG_WARN("UI: troppi campi testo simultanei (max %d) — id '%s' ignorato",
             MAX_UI_TEXT_INPUTS, id);
    text_fields_view->warned_text_capacity = true;
  }
  return NULL;
}

void ui_text_input_reset(void) {
  UITextFieldsState *text_fields_view=ui_state_text_fields(ui_overlay_context());

  for (int i=0;i<text_fields_view->text_input_state_count;++i) {
    free(text_fields_view->text_input_states[i].original_long);
    ui_text_history_clear(&text_fields_view->text_input_states[i]);
  }
  memset(text_fields_view->text_input_states,0,sizeof(text_fields_view->text_input_states));
  text_fields_view->text_input_state_count=0;
}

bool ui_text_input_is_focused(const char *id) {
  UITextFieldsState *text_fields_view=ui_state_text_fields(ui_overlay_context());

  if (!ui_valid_id(id)) return false;
  for (int i = 0; i < text_fields_view->text_input_state_count; i++) {
    if (strcmp(text_fields_view->text_input_states[i].id, id) == 0)
      return text_fields_view->text_input_states[i].focused;
  }
  return false;
}

// Vero se QUALUNQUE campo di testo ha il focus in questo momento,
// senza dover conoscere il suo ID specifico — ui_text_input_is_focused
// sopra richiede di sapere quale campo controllare, impraticabile da
// chiamare per OGNI singolo campo prima di ogni scorciatoia da
// tastiera sparsa nell'editor (ce ne sono decine, in file diversi).
// Pensata per un controllo centrale unico, non uno sparso per file.
bool ui_any_text_input_focused(void) {
  UITextFieldsState *text_fields_view=ui_state_text_fields(ui_overlay_context());

  for (int i = 0; i < text_fields_view->text_input_state_count; i++) {
    if (text_fields_view->text_input_states[i].focused)
      return true;
  }
  return false;
}

void ui_text_input_focus(const char *id) {
  if(ui_valid_id(id)) { ui_nav_set(ui_overlay_context(), id,ui_state_navigation(ui_overlay_context())->nav_keyboard); ui_state_navigation(ui_overlay_context())->nav_edit=true; }
  UITextInputState *state = ui_text_field_state(id);
  if (state && !state->focused) {
    state->focused = true;
    state->needs_snapshot = true;
  }
}

/* Widget monoriga: il buffer appartiene al chiamante e cambia durante
 * l'editing. Il risultato true segnala conferma, non ogni modifica. Lo stato
 * persistente e individuato da id, che deve rimanere stabile fra i frame. */
bool ui_text_input_mode(const char *id, Font *font, TextAnchor anchor,
                   float margin, float scale, float offset_x, float offset_y,
                   float width, char *buffer, int buffer_size,
                   const UIButtonStyle *style,bool password,bool reveal) {
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());
  UINavigationState *navigation_view=ui_state_navigation(ui_overlay_context());
  UIClipboardState *clipboard_view=ui_state_clipboard(ui_overlay_context());

  UITextInputState *state = ui_text_field_state(id);
  if (!state) return false;
  if (!buffer || buffer_size <= 0) {
    ui_report_error(ui_overlay_context(),UI_ERROR_ARGUMENT,UI_OPERATION_TEXT_FIELD,id); return false;
  }

  int length;
  if(!ui_text_edit_prepare(state,buffer,buffer_size,&length)) return false;

  UITheme theme = ui_get_theme();
  float padding_x = (style && style->padding_x > 0)
                        ? style->padding_x
                        : theme.label_value_gap;
  float padding_y = (style && style->padding_y > 0)
                        ? style->padding_y
                        : theme.row_spacing;
  float ascent = ui_paint_font_ascent(font, scale);

  float w = width;
  float h = ascent + padding_y * 2.0f;
  if (style && style->min_height > h)
    h = style->min_height;

  float x, y;
  switch (anchor) {
  case ANCHOR_TOP_LEFT:
    x = margin;
    y = margin;
    break;
  case ANCHOR_TOP_RIGHT:
    x = frame_view->canvas_width - margin - w;
    y = margin;
    break;
  case ANCHOR_BOTTOM_LEFT:
    x = margin;
    y = frame_view->canvas_height - margin - h;
    break;
  case ANCHOR_BOTTOM_RIGHT:
    x = frame_view->canvas_width - margin - w;
    y = frame_view->canvas_height - margin - h;
    break;
  case ANCHOR_CENTER:
    x = (frame_view->canvas_width - w) * 0.5f;
    y = (frame_view->canvas_height - h) * 0.5f;
    break;
  default:
    x = margin;
    y = margin;
  }
  x += offset_x;
  y += offset_y;

  bool enabled=!style || !style->disabled;
  bool hover = enabled && ui_pointer_hit(x, y, w, h);
  bool nav_focus=ui_nav_register(ui_overlay_context(), id,(UIRect){x,y,w,h},UI_NAV_TEXT,enabled,hover);
  if(!enabled || !ui_nav_allowed(ui_overlay_context())) state->focused=false;
  if(enabled && !state->focused && ((nav_focus && navigation_view->nav_edit) || ui_nav_has_events(ui_overlay_context(), id) ||
      ui_nav_key(ui_overlay_context(), id,PLATFORM_KEY_ENTER,false) ||
      ui_nav_key(ui_overlay_context(), id,PLATFORM_KEY_KP_ENTER,false))) {
    navigation_view->nav_editor_finished=false;
    state->focused=ui_text_snapshot(state,buffer); state->caret=state->selection_anchor=length;
  }
  if (hover)
    frame_view->g_any_hover = true;

  // Un solo edge-detect del click, condiviso da tutte le istanze: va bene
  // perché il mouse è uno solo — un click attiva il focus se cade dentro
  // questo campo, altrimenti (se il campo era già a fuoco) lo disattiva.
  // Il fronte di discesa del click è calcolato una volta sola per frame in
  // ui_begin_frame (g_mouse_clicked) — niente più static locale qui, che
  // altrimenti verrebbe "consumata" da questa chiamata e lascerebbe altri
  // widget dello stesso tipo a secco nello stesso frame.
  if (ui_capture_widget_press(ui_overlay_context())) {
    if (hover && !state->focused) {
      state->focused = ui_text_snapshot(state,buffer);
      state->caret = state->selection_anchor = (int)strlen(buffer);
    } else if (!hover) {
      state->focused = false;
    }
  }

  bool confirmed = false;
  if (state->focused && state->needs_snapshot) {
    state->focused=ui_text_snapshot(state,buffer);
    state->caret=state->selection_anchor=(int)strlen(buffer);
  }

  if (state->focused) {
    size_t count; InputKeyEvent *events=ui_frame_events(&count);
    for (size_t i=0;i<count;++i) {
      InputKeyEvent *e=&events[i];
      if (e->consumed || !ui_nav_event(ui_overlay_context(), id,i)) continue;
      e->consumed=true;
      if (!state->focused) continue;
      if(e->kind==INPUT_KEY && e->action!=PLATFORM_PRESS && e->action!=PLATFORM_REPEAT) continue;
      if (e->kind==INPUT_KEY && e->key==PLATFORM_KEY_ESCAPE && ui_capture_editor_escape_allowed(ui_overlay_context())) {
        ui_text_edit_cancel(state,buffer,buffer_size);
        ui_nav_editor_finish(ui_overlay_context(), id);
        ui_capture_mark_escape(ui_overlay_context());
      } else if (e->kind==INPUT_KEY && (e->key==PLATFORM_KEY_ENTER || e->key==PLATFORM_KEY_KP_ENTER)) {
        confirmed=true; state->focused=false;
        ui_nav_editor_finish(ui_overlay_context(), id);
      } else if(password && e->kind==INPUT_KEY && (e->mods&PLATFORM_MOD_CONTROL) &&
                (e->key==PLATFORM_KEY_C || e->key==PLATFORM_KEY_X)) {
        /* Password fields never export their contents to the clipboard. */
      } else ui_text_edit_event(state,buffer,buffer_size,e,&clipboard_view->clipboard,clipboard_view->clipboard_user);
    }
  }

  if(navigation_view->nav_enabled && !ui_nav_focused(ui_overlay_context(), id)) state->focused=false;
  bool custom = style && style->has_custom_color;
  UIColor fill = custom ? style->color_normal
                        : (state->focused || hover ? theme.field_hover
                                                   : theme.field);
  UIColor borderColor =
      state->focused ? theme.accent
                     : (custom ? style->color_border : theme.border);

  float border = 1.0f;
  if (style && style->border < 0)
    border = 0.0f;
  else if (style && style->border > 0)
    border = style->border;

  if (theme.paper_panels) {
    const float color[4] = {fill.r,fill.g,fill.b,1}, accent[3] = {borderColor.r,borderColor.g,borderColor.b};
    ui_paint_draw_hud_paper_panel(x,y,w,h,color,accent,state->focused,ui_hash_id(id));
  } else {
    if (border > 0.0f)
      ui_paint_draw_quad(x - border, y - border, w + border * 2, h + border * 2,
                borderColor.r, borderColor.g, borderColor.b);
    ui_paint_draw_quad(x, y, w, h, fill.r, fill.g, fill.b);
  }

  ui_text_edit_observe(state,buffer);
  char *mask=NULL,*display=buffer;
  int draw_caret=state->caret,draw_anchor=state->selection_anchor;
  if(password && !reveal) {
    size_t bytes=strlen(buffer),characters=0;
    mask=malloc(bytes+1);
    if(!mask) { ui_nav_ring(id,(UIRect){x,y,w,h}); return confirmed; }
    for(size_t at=0;at<bytes;at=utf8_next(buffer,bytes,at)) {
      if(at==(size_t)state->caret) draw_caret=(int)characters;
      if(at==(size_t)state->selection_anchor) draw_anchor=(int)characters;
      mask[characters++]='*';
    }
    if(state->caret==(int)bytes) draw_caret=(int)characters;
    if(state->selection_anchor==(int)bytes) draw_anchor=(int)characters;
    mask[characters]=0; display=mask;
  }
  float availableTextW=fmaxf(0,w-padding_x*2.0f);
  int visible_end=state->focused?draw_caret:(int)strlen(display);
  char saved=display[visible_end]; display[visible_end]=0;
  const char *shown=display;
  while(*shown && ui_paint_measure_text_width(font,shown,scale)>fmaxf(0,availableTextW-theme.border_thickness))
    shown+=utf8_next(shown,strlen(shown),0);
  display[visible_end]=saved;
  bool clipped=ui_paint_text_clip_push(x+padding_x,y,availableTextW,h);
  if(clipped) {
    int hidden=(int)(shown-display);
    if(state->focused && draw_caret!=draw_anchor) {
      int lo=(draw_caret<draw_anchor?draw_caret:draw_anchor),hi=(draw_caret>draw_anchor?draw_caret:draw_anchor);
      if(lo<hidden) lo=hidden;
      if(hi>lo) {
        saved=display[lo]; display[lo]=0;
        float left=ui_paint_measure_text_width(font,shown,scale); display[lo]=saved;
        saved=display[hi]; display[hi]=0;
        float right=ui_paint_measure_text_width(font,shown,scale); display[hi]=saved;
        ui_paint_draw_quad_alpha(x+padding_x+left,y+padding_y,right-left,h-2*padding_y,
                                theme.accent.r,theme.accent.g,theme.accent.b,.35f);
      }
    }
    ui_paint_draw_text_colored(font,shown,x+padding_x,y+(h+ascent)*.5f,
                              scale,theme.text.r,theme.text.g,theme.text.b);
    if(state->focused) {
      saved=display[draw_caret]; display[draw_caret]=0;
      float caretX=x+padding_x+ui_paint_measure_text_width(font,shown,scale);
      display[draw_caret]=saved;
      ui_paint_draw_quad(caretX,y+padding_y,theme.border_thickness,h-2*padding_y,
                        theme.accent.r,theme.accent.g,theme.accent.b);
    }
    ui_paint_text_clip_pop();
  }

  free(mask);
  if (!state->focused) { free(state->original_long); state->original_long=NULL; }
  ui_nav_ring(id,(UIRect){x,y,w,h});
  return confirmed;
}

bool ui_text_input(const char *id,Font *font,TextAnchor anchor,float margin,float scale,
                   float x,float y,float width,char *buffer,int capacity,const UIButtonStyle *style) {
  return ui_text_input_mode(id,font,anchor,margin,scale,x,y,width,buffer,capacity,style,false,false);
}

/* UIModalOpenState / g_modal_states: dichiarati in cima (servono a ui_begin_frame). */
