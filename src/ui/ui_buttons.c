#include "ui_state_tooltip_internal.h"
#include "ui_state_navigation_internal.h"
#include "ui_state_frame_internal.h"
#include "ui_state_buttons_internal.h"
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
UIState *ui_button_state(const char *id) {
  UIButtonsState *buttons_view=ui_state_buttons(ui_overlay_context());
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

  if (!ui_valid_id(id)) {
    ui_report_error(ui_overlay_context(),UI_ERROR_ARGUMENT,UI_OPERATION_BUTTON,id); return NULL;
  }
  unsigned hash = ui_hash_id(id);
  for (int i = 0; i < buttons_view->ui_state_count; i++) {
    if (buttons_view->ui_states[i].id_hash == hash && strcmp(buttons_view->ui_states[i].id, id) == 0) {
      if(frame_view->frame_active && buttons_view->ui_states[i].last_seen_frame+1<frame_view->g_ui_frame_serial)
        buttons_view->ui_states[i].is_pressed=false;
      buttons_view->ui_states[i].last_seen_frame = frame_view->g_ui_frame_serial;
      return &buttons_view->ui_states[i];
    }
  }
  if (buttons_view->ui_state_count < MAX_UI_ELEMENTS) {
    UIState *s = &buttons_view->ui_states[buttons_view->ui_state_count++];
    strncpy(s->id, id, sizeof(s->id) - 1);
    s->id[sizeof(s->id) - 1] = '\0';
    s->id_hash = hash;
    s->is_hover = false;
    s->is_pressed = false;
    s->hover_t = 0.0f;
    s->last_seen_frame = frame_view->g_ui_frame_serial;
    return s;
  }
  /* The registry is a bounded cache, not an append-only lifetime list.
   * Screens and Inspector tabs legitimately submit many different semantic
   * IDs over a session. Reuse the least-recent inactive entry, while never
   * evicting a widget pressed in-flight or one already emitted this frame. */
  UIState *reuse = NULL;
  for (int i = 0; i < buttons_view->ui_state_count; ++i) {
    UIState *candidate = &buttons_view->ui_states[i];
    if (candidate->is_pressed ||
        candidate->last_seen_frame == frame_view->g_ui_frame_serial)
      continue;
    if (!reuse || candidate->last_seen_frame < reuse->last_seen_frame)
      reuse = candidate;
  }
  if (reuse) {
    memset(reuse, 0, sizeof(*reuse));
    snprintf(reuse->id, sizeof(reuse->id), "%s", id);
    reuse->id_hash = hash;
    reuse->last_seen_frame = frame_view->g_ui_frame_serial;
    return reuse;
  }

  // More than MAX_UI_ELEMENTS live in one frame: fail visibly and safely.

  ui_report_error(ui_overlay_context(),UI_ERROR_LIMIT,UI_OPERATION_BUTTON,id);
  if (!buttons_view->warned_state_capacity) {
    LOG_WARN("UI: troppi widget simultanei (max %d) — id '%s' ignorato",
             MAX_UI_ELEMENTS, id);
    buttons_view->warned_state_capacity = true;
  }
  return NULL;
}

static float lerp(float a, float b, float t) { return a + (b - a) * t; }
float ui_button_height(Font *font, float scale, const UIButtonStyle *style) {
  UITheme theme = ui_get_theme();
  float padding_y = (style && style->padding_y > 0) ? style->padding_y : theme.row_spacing - 1.0f;
  float min_height = style ? style->min_height : 0.0f;
  float ascent = ui_paint_font_ascent(font, scale);

  float h = ascent + padding_y * 2.0f;
  if (h < min_height)
    h = min_height;
  return h;
}

bool ui_button_labeled(const char *id, const char *label,
                       Font *font, TextAnchor anchor, float margin, float scale,
                       float offset_x, float offset_y,
                       const UIButtonStyle *style) {
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

  if (!ui_valid_id(id)) {
    ui_report_error(ui_overlay_context(),UI_ERROR_ARGUMENT,UI_OPERATION_BUTTON,id); return false;
  }
  UIState *state = ui_button_state(id);
  if (!state)
    return false;

  // Trasformata SUBITO, prima di misurare la larghezza — così bottone
  // e testo restano coerenti tra loro (misurati sulla stessa stringa
  // che verrà davvero disegnata più sotto).
  char displayLabel[256];
  ui_humanize_label(label, displayLabel, sizeof(displayLabel));

  UITheme theme = ui_get_theme();
  float padding_x = (style && style->padding_x > 0) ? style->padding_x : theme.label_value_gap;
  float padding_y = (style && style->padding_y > 0) ? style->padding_y : theme.row_spacing - 1.0f;
  float min_width = style ? style->min_width : 0.0f;
  float max_width = style ? style->max_width : 0.0f;
  float min_height = style ? style->min_height : 0.0f;
  float border = theme.border_thickness;
  if (style && style->border < 0)
    border = 0.0f;
  else if (style && style->border > 0)
    border = style->border;

  float text_w = ui_paint_measure_text_width(font, displayLabel, scale);
  float ascent = ui_paint_font_ascent(font, scale);

  float w = text_w + padding_x * 2.0f;
  if (w < min_width)
    w = min_width;
  if (max_width > 0.0f && w > max_width) {
    w = max_width;
    ui_ellipsize_right(font, displayLabel, scale,
                       fmaxf(0.0f, w - padding_x * 2.0f), displayLabel,
                       sizeof(displayLabel));
    text_w = ui_paint_measure_text_width(font, displayLabel, scale);
  }

  float h = ascent + padding_y * 2.0f;
  if (h < min_height)
    h = min_height;

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

  // Overflow orizzontale: se il bottone (nella posizione calcolata sopra)
  // finirebbe oltre il bordo destro dello schermo, lo si rientra così
  // resta interamente visibile e cliccabile — senza questo, un bottone
  // piazzato con ANCHOR_TOP_LEFT + offset_x grande (es. nella toolbar,
  // dove gli elementi si accodano uno dopo l'altro) può finire
  // parzialmente o del tutto fuori schermo su risoluzioni più strette di
  // quella su cui gli offset erano stati tarati a occhio.
  if (x + w > frame_view->canvas_width - 10.0f)
    x = frame_view->canvas_width - 10.0f - w;
  if (x < 10.0f)
    x = 10.0f;

  state->is_hover = (!style || !style->disabled) &&
                    ui_pointer_hit(x, y, w, h);
  if (state->is_hover)
    frame_view->g_any_hover = true;

  ui_nav_register(ui_overlay_context(), id,(UIRect){x,y,w,h},UI_NAV_BUTTON,!style || !style->disabled,state->is_hover);
  bool clicked = (!style || !style->disabled) && ui_nav_activate(ui_overlay_context(), id);
  clicked |= ui_pointer_activate(&state->is_pressed,state->is_hover,!style || !style->disabled);

  float target = state->is_hover ? 1.0f : 0.0f;
  float speed = 8.0f;
  if (state->hover_t < target) {
    state->hover_t += speed * frame_view->g_delta_time;
    if (state->hover_t > target)
      state->hover_t = target;
  } else if (state->hover_t > target) {
    state->hover_t -= speed * frame_view->g_delta_time;
    if (state->hover_t < target)
      state->hover_t = target;
  }

  UIColor normal = theme.button_normal;
  UIColor hover = theme.button_hover;
  UIColor pressed = theme.button_pressed;
  UIColor bcolor = theme.button_border;
  UIColor text_color = theme.text;

  if (style && style->has_custom_color) {
    normal = style->color_normal;
    hover = style->color_hover;
    pressed = style->color_pressed;
    bcolor = style->color_border;
  }
  if (style && style->has_custom_text)
    text_color = style->color_text;
  if (style && style->danger) {
    normal = theme.danger; hover = theme.error; pressed = theme.danger;
    bcolor = theme.error;
  }
  if (style && style->selected) {
    normal = theme.selected; hover = theme.accent; pressed = theme.field;
    bcolor = theme.accent;
  }
  if (style && style->disabled) {
    normal = hover = pressed = theme.disabled;
    bcolor = theme.border;
    text_color = theme.text_muted;
  }

  float r = lerp(normal.r, hover.r, state->hover_t);
  float g = lerp(normal.g, hover.g, state->hover_t);
  float b = lerp(normal.b, hover.b, state->hover_t);

  if (state->is_pressed) {
    r = pressed.r;
    g = pressed.g;
    b = pressed.b;
  }

  if (theme.paper_panels) {
    const float fill[4] = {r,g,b,1};
    const float accent[3] = {theme.accent.r,theme.accent.g,theme.accent.b};
    /* lo sbaffo trash polka dietro (lo stile dell'HUD, 2026-10-24) */
    ui_paint_draw_hud_trash_swipe(x, y, w, h, state->is_hover || (style && style->selected), ui_hash_id(id),
                         -1);
    ui_paint_draw_hud_paper_panel(x,y,w,h,fill,accent,
                         state->is_hover || (style && style->selected),ui_hash_id(id));
  } else if (border > 0.0f) {
    /* Neutral chrome at rest keeps the accent meaningful.  Hover, selected,
     * danger and focus are the states allowed to spend semantic colour. */
    UIColor resting_border = (style && style->has_custom_color)
                                 ? bcolor
                                 : theme.border;
    float border_r = lerp(resting_border.r, bcolor.r, state->hover_t);
    float border_g = lerp(resting_border.g, bcolor.g, state->hover_t);
    float border_b = lerp(resting_border.b, bcolor.b, state->hover_t);

    ui_paint_draw_quad(x - border, y - border, w + border * 2, h + border * 2,
              border_r, border_g, border_b);
  }
  if (!theme.paper_panels) ui_paint_draw_quad(x, y, w, h, r, g, b);

  float text_x = (style && style->align_left)
                     ? x + padding_x
                     : x + (w - text_w) * 0.5f;
  float text_y = y + (h + ascent) * 0.5f;
  ui_paint_draw_text_colored(font, displayLabel, text_x, text_y, scale,
                    text_color.r,
                    text_color.g, text_color.b);

  ui_nav_ring(id,(UIRect){x,y,w,h});
  return clicked;
}

// Checkbox: quadratino + etichetta a destra, a coordinate ASSOLUTE (x,y —
// niente sistema di ancoraggio come ui_slider/ui_button, per come viene
// già usata nei chiamanti esistenti, es. option.c/draw_volume_row).
// Aggiorna *value DIRETTAMENTE al click (non serve leggere il valore di
// ritorno per sapere il nuovo stato) e ritorna true SOLO nel frame in cui
// il click avviene — utile a chi chiama per pushare undo o altra logica
// aggiuntiva in quel frame, stesso principio del valore di ritorno di
// ui_button_labeled.
//
// NOTA RICOSTRUZIONE: questa funzione è andata persa da una sovrascrittura
// accidentale e non c'era backup — riscritta da zero seguendo la firma già
// dichiarata extern nei chiamanti esistenti (option.c, editor_hud_toolbar.c)
// e lo stesso stile/pattern di click-detection di ui_button_labeled qui
// sopra. Se il comportamento visivo non torna esattamente a quello di
// prima, è qui che va aggiustato.
bool ui_checkbox(const char *id, const char *label, Font *font,
                 float x, float y, float scale, bool *value) {
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

  if (!ui_valid_id(id)) {
    ui_report_error(ui_overlay_context(),UI_ERROR_ARGUMENT,UI_OPERATION_BUTTON,id); return false;
  }
  UIState *state = ui_button_state(id);
  if (!state || !value)
    return false;

  char displayLabel[128];
  ui_humanize_label(label, displayLabel, sizeof(displayLabel));

  float ascent = ui_paint_font_ascent(font, scale);
  float boxSize = ascent; // stessa altezza del testo, per allinearsi bene
                          // sulla riga invece di un quadratino fisso che
                          // può risultare troppo piccolo/grande rispetto
                          // all'etichetta a scale diverse
  float gap = 8.0f;
  float text_w = ui_paint_measure_text_width(font, displayLabel, scale);
  float totalW = boxSize + gap + text_w;
  float totalH = boxSize;

  // Area cliccabile: quadratino + etichetta insieme, non solo il
  // quadratino — più facile da colpire col mouse, stesso spirito
  // "generoso" degli altri widget di questo file.
  state->is_hover = ui_pointer_hit(x, y, totalW, totalH);
  if (state->is_hover)
    frame_view->g_any_hover = true;

  ui_nav_register(ui_overlay_context(), id,(UIRect){x,y,totalW,totalH},UI_NAV_BUTTON,true,state->is_hover);
  bool clicked = ui_nav_activate(ui_overlay_context(), id);
  clicked |= ui_pointer_activate(&state->is_pressed,state->is_hover,true);

  if (clicked)
    *value = !*value;

  float target = state->is_hover ? 1.0f : 0.0f;
  float speed = 8.0f;
  if (state->hover_t < target) {
    state->hover_t += speed * frame_view->g_delta_time;
    if (state->hover_t > target)
      state->hover_t = target;
  } else if (state->hover_t > target) {
    state->hover_t -= speed * frame_view->g_delta_time;
    if (state->hover_t < target)
      state->hover_t = target;
  }

  UITheme theme = ui_get_theme();
  UIColor normal = theme.button_normal;
  UIColor hover = theme.button_hover;
  UIColor bcolor = theme.button_border;
  UIColor text_color = theme.text;

  float r = lerp(normal.r, hover.r, state->hover_t);
  float g = lerp(normal.g, hover.g, state->hover_t);
  float b = lerp(normal.b, hover.b, state->hover_t);
  if (state->is_pressed) {
    r = theme.button_pressed.r;
    g = theme.button_pressed.g;
    b = theme.button_pressed.b;
  }

  // Bordo, stesso trucco "quadrato più grande sotto" già usato da
  // ui_button_labeled invece di un vero contorno non riempito.
  float border = 1.0f;
  if (theme.paper_panels) {
    const float fill[4] = {r,g,b,1}, accent[3] = {theme.accent.r,theme.accent.g,theme.accent.b};
    ui_paint_draw_hud_paper_panel(x,y,boxSize,boxSize,fill,accent,state->is_hover,ui_hash_id(id));
  } else {
    ui_paint_draw_quad(x - border, y - border, boxSize + border * 2.0f,
             boxSize + border * 2.0f, bcolor.r, bcolor.g, bcolor.b);
    ui_paint_draw_quad(x, y, boxSize, boxSize, r, g, b);
  }

  // Spunta interna, solo se true — un quadratino più piccolo centrato,
  // nel colore del bordo/accent così resta leggibile su qualunque stato
  // hover/pressed dello sfondo.
  if (*value) {
    float pad = boxSize * 0.22f;
    ui_paint_draw_quad(x + pad, y + pad, boxSize - pad * 2.0f,
             boxSize - pad * 2.0f, bcolor.r, bcolor.g, bcolor.b);
  }

  float text_x = x + boxSize + gap;
  float text_y = y + (totalH + ascent) * 0.5f;
  ui_paint_draw_text_colored(font, displayLabel, text_x, text_y, scale,
                    text_color.r, text_color.g, text_color.b);

  ui_nav_ring(id,(UIRect){x,y,totalW,totalH});
  return clicked;
}

bool ui_toggle(const char *id, const char *label, Font *font,
               float x, float y, float scale, float width, bool *value) {
  if (!ui_valid_id(id)) {
    ui_report_error(ui_overlay_context(),UI_ERROR_ARGUMENT,UI_OPERATION_BUTTON,id); return false;
  }
  if (!value)
    return false;
  char shown[128];
  /* Etichetta vuota (righe di proprieta': l'etichetta sta nella colonna
     sinistra) -> solo lo stato, mai ": ON". */
  if (label && label[0])
    snprintf(shown, sizeof(shown), "%s: %s", label, *value ? "ON" : "OFF");
  else
    snprintf(shown, sizeof(shown), "%s", *value ? "ON" : "OFF");
  UIButtonStyle style = {.min_width = width,
                         .min_height = ui_get_theme().button_height * scale,
                         .selected = *value};
  bool changed = ui_button_labeled(id, shown, font, ANCHOR_TOP_LEFT, 0,
                                   14.0f * scale, x, y, &style);
  if (changed)
    *value = !*value;
  return changed;
}

bool ui_button_styled(const char *id, Font *font, TextAnchor anchor,
                      float margin, float scale, float offset_x, float offset_y,
                      const UIButtonStyle *style) {
  if (!ui_valid_id(id)) {
    ui_report_error(ui_overlay_context(),UI_ERROR_ARGUMENT,UI_OPERATION_BUTTON,id); return false;
  }
  return ui_button_labeled(id, id, font, anchor, margin, scale, offset_x,
                           offset_y, style);
}

static void ui_basic_circle(float x,float y,float radius,UIColor color) {
  int rows=(int)floorf(2*radius);
  if(rows<1) return;
  for(int i=0;i<rows;++i) {
    float dy=((i+.5f)/rows*2-1)*radius;
    float half=sqrtf(fmaxf(0,radius*radius-dy*dy));
    ui_paint_draw_quad(x-half,y-radius+i*(2*radius/rows),2*half,2*radius/rows,color.r,color.g,color.b);
  }
}
bool ui_radio_button(const char *id,Font *font,UIRect r,const char *label,int option,int *selected,bool disabled) {
  if(!font||!label||!selected||!ui_layout_rect_valid(r)||!ui_valid_id(id)) return false;
  UIState *state=ui_button_state(id); if(!state) return false;
  bool hover=!disabled&&ui_layout_pointer_hit(r)&&ui_nav_allowed(ui_overlay_context());
  ui_nav_register(ui_overlay_context(), id,r,UI_NAV_BUTTON,!disabled,hover);
  if(hover) ui_state_frame(ui_overlay_context())->g_any_hover=true;
  bool activate=!disabled&&ui_nav_activate(ui_overlay_context(), id);
  activate|=ui_pointer_activate(&state->is_pressed,hover,!disabled);
  int old=*selected; if(activate) *selected=option;
  UITheme t=ui_get_theme(); float radius=fminf(8,r.h*.3f);
  if(ui_clip_push(r)) {
    ui_basic_circle(r.x+radius+2,r.y+r.h*.5f,radius,disabled?t.disabled:t.text_muted);
    ui_basic_circle(r.x+radius+2,r.y+r.h*.5f,fmaxf(0,radius-2),t.field);
    if(*selected==option) ui_basic_circle(r.x+radius+2,r.y+r.h*.5f,radius*.5f,disabled?t.text_muted:t.accent);
    UIColor fg=disabled?t.text_muted:t.text;
    ui_paint_draw_text_colored(font,label,r.x+radius*2+8,r.y+(r.h+ui_paint_font_ascent(font,14))*.5f,14,fg.r,fg.g,fg.b);
    ui_clip_pop();
  }
  ui_nav_ring(id,r); return old!=*selected;
}
void ui_tooltip(const char *id,Font *font,UIRect r,const char *text,float delay) {
  UIContext *c=ui_overlay_context();

  UINavigationState *navigation_view=ui_state_navigation(c);
  UIFrameState *frame_view=ui_state_frame(c);
  UITooltipState *tooltip_view=ui_state_tooltip(c);

  if(!ui_valid_id(id)||!font||!text||!text[0]||!ui_layout_rect_valid(r)||!isfinite(delay)||delay<0||
     navigation_view->nav_cancelled||!ui_nav_allowed(ui_overlay_context())||ui_pointer_blocked_by_modal()||frame_view->g_mouse_down||ui_menu_hierarchy_is_open()) return;
  if(!ui_layout_pointer_hit(r)&&!(navigation_view->nav_keyboard&&ui_nav_focused(ui_overlay_context(), id))) return;
  if(tooltip_view->tooltip_frame==frame_view->g_ui_frame_serial) return;
  if(strcmp(tooltip_view->tooltip_id,id)||tooltip_view->tooltip_frame+1!=frame_view->g_ui_frame_serial) tooltip_view->tooltip_elapsed=0;
  else tooltip_view->tooltip_elapsed+=frame_view->g_delta_time;
  snprintf(tooltip_view->tooltip_id,sizeof(tooltip_view->tooltip_id),"%s",id);
  utf8_copy(tooltip_view->tooltip_text,sizeof(tooltip_view->tooltip_text),text);
  snprintf(tooltip_view->tooltip_modal,sizeof(tooltip_view->tooltip_modal),"%s",ui_nav_scope(ui_overlay_context()));
  tooltip_view->tooltip_font=font; tooltip_view->tooltip_anchor=r; tooltip_view->tooltip_delay=delay; tooltip_view->tooltip_frame=frame_view->g_ui_frame_serial;
}
void ui_tooltip_draw_pending(void) {
  UIContext *c=ui_overlay_context();

  UITooltipState *tooltip_view=ui_state_tooltip(c);
  UIFrameState *frame_view=ui_state_frame(c);

  if(!tooltip_view->tooltip_font||!tooltip_view->tooltip_text[0]||tooltip_view->tooltip_frame!=frame_view->g_ui_frame_serial||tooltip_view->tooltip_elapsed<tooltip_view->tooltip_delay||
     strcmp(tooltip_view->tooltip_modal,ui_state_navigation(c)->nav_modal)||ui_menu_hierarchy_is_open()) return;
  UITheme t=ui_get_theme(); float size=14,pad=8,h=30;
  char shown[512];
  ui_ellipsize_right(tooltip_view->tooltip_font,tooltip_view->tooltip_text,size,fmaxf(0,frame_view->canvas_width-2*pad-8),shown,sizeof(shown));
  float w=fminf(frame_view->canvas_width,ui_paint_measure_text_width(tooltip_view->tooltip_font,shown,size)+2*pad);
  float x=fmaxf(0,fminf(tooltip_view->tooltip_anchor.x,frame_view->canvas_width-w));
  float y=tooltip_view->tooltip_anchor.y+tooltip_view->tooltip_anchor.h+4;
  if(y+h>frame_view->canvas_height) y=fmaxf(0,tooltip_view->tooltip_anchor.y-h-4);
  ui_paint_draw_quad(x,y,w,h,t.panel_header.r,t.panel_header.g,t.panel_header.b);
  ui_paint_draw_text_colored(tooltip_view->tooltip_font,shown,x+pad,y+(h+ui_paint_font_ascent(tooltip_view->tooltip_font,size))*.5f,size,t.text.r,t.text.g,t.text.b);
}
