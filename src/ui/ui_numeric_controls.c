#include "ui_state_sliders_internal.h"
#include "ui_state_numeric_internal.h"
#include "ui_state_navigation_internal.h"
#include "ui_state_frame_internal.h"
#include "ui/ui.h"
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
#include <float.h>
#include <errno.h>
UIFloatFieldState *ui_numeric_state(const char *id) {
  UINumericState *numeric_view=ui_state_numeric(ui_overlay_context());
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

  if (!ui_valid_id(id)) return NULL;
  for (int i = 0; i < numeric_view->float_field_state_count; ++i)
    if (strcmp(numeric_view->float_field_states[i].id, id) == 0) {
      numeric_view->float_field_states[i].last_seen_frame = frame_view->g_ui_frame_serial;
      return &numeric_view->float_field_states[i];
    }
  UIFloatFieldState *state = NULL;
  if (numeric_view->float_field_state_count < MAX_UI_FLOAT_FIELDS) {
    state = &numeric_view->float_field_states[numeric_view->float_field_state_count++];
  } else {
    for (int i = 0; i < numeric_view->float_field_state_count; ++i) {
      UIFloatFieldState *candidate = &numeric_view->float_field_states[i];
      if (candidate->focused ||
          candidate->last_seen_frame == frame_view->g_ui_frame_serial)
        continue;
      if (!state || candidate->last_seen_frame < state->last_seen_frame)
        state = candidate;
    }
  }
  if (!state)
    return NULL;
  memset(state, 0, sizeof(*state));
  snprintf(state->id, sizeof(state->id), "%s", id);
  state->last_seen_frame = frame_view->g_ui_frame_serial;
  return state;
}

bool ui_parse_float_clamped(const char *text, float min, float max,
                            float *out_value) {
  if (!text || !out_value || !isfinite(min) || !isfinite(max) || min > max)
    return false;
  errno = 0;
  char *end = NULL;
  float value = strtof(text, &end);
  if (end == text || *end != '\0' || errno == ERANGE || !isfinite(value))
    return false;
  if (value < min) value = min;
  if (value > max) value = max;
  *out_value = value;
  return true;
}

bool ui_parse_int_clamped(const char *text, int min, int max, int *out_value) {
  if (!text || !out_value || min > max)
    return false;
  errno = 0;
  char *end = NULL;
  long value = strtol(text, &end, 10);
  if (end == text || *end != '\0' || errno == ERANGE)
    return false;
  if (value < min) value = min;
  if (value > max) value = max;
  *out_value = (int)value;
  return true;
}

/* A compact significant-digit spelling which parses to the same float.
 * This avoids displaying binary tails (2.20000005) without rounding the model
 * or losing precision when an unchanged editor is confirmed. */
static void ui_format_float(char *out, size_t capacity, float value) {
  for (int digits=6; digits<=FLT_DECIMAL_DIG; ++digits) {
    snprintf(out,capacity,"%.*g",digits,(double)value);
    if (strtof(out,NULL)==value) break;
  }
}

static void ui_float_insert(UIFloatFieldState *state, char c) {
  size_t len = strlen(state->buffer);
  if (len + 1 < sizeof(state->buffer)) {
    state->buffer[len] = c;
    state->buffer[len + 1] = '\0';
  }
}

bool ui_number_field(const char *id, Font *font, float x, float y,
                    float width, float height, float scale, float *value,
                    float min, float max, int *integer, int int_min, int int_max) {
  UINavigationState *navigation_view=ui_state_navigation(ui_overlay_context());

  UIFloatFieldState *state = ui_numeric_state(id);
  if (!state || (!integer && (!value || !isfinite(*value) || !isfinite(min) || !isfinite(max) || min > max)) ||
      (integer && int_min > int_max)) return false;
  UITheme theme = ui_get_theme();
  bool hover = ui_pointer_hit(x, y, width, height);
  if (hover) ui_state_frame(ui_overlay_context())->g_any_hover = true;
  if (ui_capture_widget_press(ui_overlay_context())) {
    if (hover && !state->focused) {
      state->focused = true; state->invalid = false; state->original = integer ? 0 : *value;
      state->original_int = integer ? *integer : 0;
      if (integer) snprintf(state->buffer, sizeof(state->buffer), "%d", *integer);
      else ui_format_float(state->buffer, sizeof(state->buffer), *value);
    } else if (!hover) {
      state->focused = false;
    }
  }
  bool nav_focus=ui_nav_register(ui_overlay_context(), id,(UIRect){x,y,width,height},UI_NAV_NUMBER,true,hover);
  if(!state->focused && ((nav_focus && navigation_view->nav_edit) || ui_nav_has_events(ui_overlay_context(), id) ||
      ui_nav_key(ui_overlay_context(), id,PLATFORM_KEY_ENTER,false) ||
      ui_nav_key(ui_overlay_context(), id,PLATFORM_KEY_KP_ENTER,false))) {
    navigation_view->nav_editor_finished=false;
    state->focused=true; state->invalid=false;
    state->original=integer?0:*value; state->original_int=integer?*integer:0;
    if(integer) snprintf(state->buffer,sizeof(state->buffer),"%d",*integer);
    else ui_format_float(state->buffer,sizeof(state->buffer),*value);
  }
  bool changed = false;
  if (state->focused) {
    size_t count; InputKeyEvent *events=ui_frame_events(&count);
    for (size_t i=0;i<count;++i) {
      InputKeyEvent *e=&events[i];
      if (e->consumed || !ui_nav_event(ui_overlay_context(), id,i)) continue;
      e->consumed=true;
      // Events after Enter/Escape belong to no editor, not the next widget.
      if (!state->focused) continue;
      if (e->kind==INPUT_CHARACTER) {
        unsigned c=e->codepoint;
        if ((c>='0' && c<='9') || c=='-' || (!integer && (c=='.' || c=='e' || c=='E' || c=='+')))
          ui_float_insert(state,(char)c);
        continue;
      }
      if (e->action!=PLATFORM_PRESS && e->action!=PLATFORM_REPEAT) continue;
      if (e->key==PLATFORM_KEY_ESCAPE && ui_capture_editor_escape_allowed(ui_overlay_context())) {
        if (integer) *integer=state->original_int; else *value=state->original;
        state->focused=false; state->invalid=false; ui_nav_editor_finish(ui_overlay_context(), id); ui_capture_mark_escape(ui_overlay_context());
      } else if (e->key==PLATFORM_KEY_ENTER || e->key==PLATFORM_KEY_KP_ENTER) {
        float parsed=0; int parsed_int=0;
        bool valid=integer ? ui_parse_int_clamped(state->buffer,int_min,int_max,&parsed_int)
                           : ui_parse_float_clamped(state->buffer,min,max,&parsed);
        if (valid) {
          if (integer) { changed=parsed_int!=*integer; *integer=parsed_int; }
          else { changed=parsed!=*value; *value=parsed; }
          state->focused=false; state->invalid=false;
          ui_nav_editor_finish(ui_overlay_context(), id);
        } else {
          state->invalid=true; ui_notify(UI_TOAST_ERROR,"Valore numerico non valido");
        }
      } else if ((e->mods & PLATFORM_MOD_CONTROL) && e->key==PLATFORM_KEY_A) {
        state->buffer[0]=0;
      } else if (e->key==PLATFORM_KEY_BACKSPACE) {
        size_t len=strlen(state->buffer); if (len) state->buffer[len-1]=0;
      }
    }
  }
  if(navigation_view->nav_enabled && !ui_nav_focused(ui_overlay_context(), id)) state->focused=false;
  if (!state->focused) {
    if (integer) snprintf(state->buffer, sizeof(state->buffer), "%d", *integer);
    else ui_format_float(state->buffer, sizeof(state->buffer), *value);
  }
  UIColor border = state->invalid ? theme.error :
                    state->focused ? theme.accent : theme.border;
  UIColor fill = state->focused ? theme.field_hover : theme.field;
  float thickness = theme.border_thickness;
  if (theme.paper_panels) {
    const float color[4] = {fill.r,fill.g,fill.b,1}, accent[3] = {border.r,border.g,border.b};
    ui_paint_draw_hud_paper_panel(x,y,width,height,color,accent,state->focused || state->invalid,ui_hash_id(id));
  } else {
    ui_paint_draw_quad(x - thickness, y - thickness, width + thickness * 2, height + thickness * 2,
              border.r, border.g, border.b);
    ui_paint_draw_quad(x, y, width, height, fill.r, fill.g, fill.b);
  }
  float text_x=x+theme.label_value_gap;
  float available=fmaxf(0,width-2*theme.label_value_gap);
  if (available>0 && ui_paint_text_clip_push(text_x,y,available,height)) {
    /* Numeric editing appends at the end; keep that end visible while typing.
     * At rest preserve the leading sign/digits and clip inside the field. */
    float offset=state->focused ? fmaxf(0,ui_paint_measure_text_width(font,state->buffer,scale)-available) : 0;
    ui_paint_draw_text_colored(font, state->buffer, text_x-offset,
                      y + (height + ui_paint_font_ascent(font, scale)) * 0.5f, scale,
                      state->invalid ? theme.error.r : theme.text.r,
                      state->invalid ? theme.error.g : theme.text.g,
                      state->invalid ? theme.error.b : theme.text.b);
    ui_paint_text_clip_pop();
  }
  ui_nav_ring(id,(UIRect){x,y,width,height});
  return changed;
}

bool ui_float_field(const char *id, Font *font, float x, float y,
                    float width, float height, float scale, float *value,
                    float min, float max) {
  return ui_number_field(id, font, x, y, width, height, scale,
                         value, min, max, NULL, 0, 0);
}

void ui_release_stale_slider_drags(void) {
  UISlidersState *sliders_view=ui_state_sliders(ui_overlay_context());

  for (int i = 0; i < sliders_view->slider_state_count; ++i)
    if (sliders_view->slider_states[i].dragging && sliders_view->slider_states[i].last_seen_frame + 1 < ui_state_frame(ui_overlay_context())->g_ui_frame_serial)
      sliders_view->slider_states[i].dragging = false;
}

UISliderState *ui_slider_state(const char *id) {
  UISlidersState *sliders_view=ui_state_sliders(ui_overlay_context());
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

  if (!ui_valid_id(id)) return NULL;
  for (int i = 0; i < sliders_view->slider_state_count; i++) {
    if (strcmp(sliders_view->slider_states[i].id, id) == 0) {
      if(frame_view->frame_active && sliders_view->slider_states[i].last_seen_frame+1<frame_view->g_ui_frame_serial)
        sliders_view->slider_states[i].dragging=false;
      sliders_view->slider_states[i].last_seen_frame = frame_view->g_ui_frame_serial;
      return &sliders_view->slider_states[i];
    }
  }
  if (sliders_view->slider_state_count < MAX_UI_SLIDERS) {
    UISliderState *s = &sliders_view->slider_states[sliders_view->slider_state_count++];
    memset(s, 0, sizeof(*s));
    snprintf(s->id, sizeof(s->id), "%s", id);
    s->last_seen_frame = frame_view->g_ui_frame_serial;
    return s;
  }
  UISliderState *reuse = NULL;
  for (int i = 0; i < sliders_view->slider_state_count; ++i) {
    UISliderState *candidate = &sliders_view->slider_states[i];
    if (candidate->dragging ||
        candidate->last_seen_frame == frame_view->g_ui_frame_serial)
      continue;
    if (!reuse || candidate->last_seen_frame < reuse->last_seen_frame)
      reuse = candidate;
  }
  if (reuse) {
    memset(reuse, 0, sizeof(*reuse));
    snprintf(reuse->id, sizeof(reuse->id), "%s", id);
    reuse->last_seen_frame = frame_view->g_ui_frame_serial;
    return reuse;
  }

  if (!sliders_view->warned_slider_capacity) {
    LOG_WARN("UI: troppi slider simultanei (max %d) — id '%s' ignorato",
             MAX_UI_SLIDERS, id);
    sliders_view->warned_slider_capacity = true;
  }
  return NULL;
}

bool ui_slider(const char *id, Font *font, TextAnchor anchor,
               float margin, float scale, float offset_x, float offset_y,
               float width, float *value, float min, float max,
               const char *label, bool *out_drag_started) {
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

  if (!ui_valid_id(id)) return false;
  if (out_drag_started)
    *out_drag_started = false;

  UISliderState *state = ui_slider_state(id);
  if (!state)
    return false;

  if (scale <= 0.0f)
    scale = 1.0f; // fallback prudente — uno scale a 0/negativo
                  // renderebbe lo slider invisibile/inutilizzabile
                  // invece di segnalare l'errore a schermo

  UITheme sliderTheme = ui_get_theme();
  float trackHeight = sliderTheme.slider_track_height * scale;
  float labelScale = 14.0f * scale * sliderTheme.text_scale;
  float labelHeight = sliderTheme.field_height * scale;
  float totalHeight = labelHeight + trackHeight;

  float x, y;
  switch (anchor) {
  case ANCHOR_TOP_LEFT:
    x = margin;
    y = margin;
    break;
  case ANCHOR_TOP_RIGHT:
    x = frame_view->canvas_width - margin - width;
    y = margin;
    break;
  case ANCHOR_BOTTOM_LEFT:
    x = margin;
    y = frame_view->canvas_height - margin - totalHeight;
    break;
  case ANCHOR_BOTTOM_RIGHT:
    x = frame_view->canvas_width - margin - width;
    y = frame_view->canvas_height - margin - totalHeight;
    break;
  case ANCHOR_CENTER:
    x = (frame_view->canvas_width - width) * 0.5f;
    y = (frame_view->canvas_height - totalHeight) * 0.5f;
    break;
  default:
    x = margin;
    y = margin;
  }
  x += offset_x;
  y += offset_y;

  float valueW = sliderTheme.numeric_field_width * scale;
  if (valueW > width * 0.42f) valueW = width * 0.42f;
  float fieldH = labelHeight;
  float trackW = width - valueW - sliderTheme.label_value_gap * scale;
  if (trackW < 32.0f) trackW = 32.0f;
  float trackY = y + labelHeight;

  char labelBuf[64];
  snprintf(labelBuf, sizeof(labelBuf), "%s", label ? label : "VALUE");
  char displaySliderLabel[64];
  ui_humanize_label(labelBuf, displaySliderLabel, sizeof(displaySliderLabel));
  float labelAvailable = trackW - sliderTheme.label_value_gap * scale;
  while (displaySliderLabel[0] &&
         ui_paint_measure_text_width(font, displaySliderLabel, labelScale) > labelAvailable) {
    size_t len = strlen(displaySliderLabel);
    if (len < 4) break;
    displaySliderLabel[len - 1] = '\0';
    displaySliderLabel[len - 2] = '.';
    displaySliderLabel[len - 3] = '.';
    displaySliderLabel[len - 4] = '.';
  }
  ui_paint_draw_text_colored(font, displaySliderLabel, x,
                    y + (labelHeight + ui_paint_font_ascent(font, labelScale)) * 0.5f,
                    labelScale, sliderTheme.text_muted.r,
                    sliderTheme.text_muted.g, sliderTheme.text_muted.b);
  // Binario — grigio dedicato, non il "bottone premuto" condiviso
  // (0.012, troppo scuro: con un valore basso o vicino a zero, il
  // riempimento arancione è minimo/assente e il binario scuro si
  // confondeva quasi del tutto con lo sfondo dell'editor, facendo
  // sembrare lo slider "rotto" invece che semplicemente vuoto).
  // Abbastanza chiaro da restare visibile SEMPRE, indipendentemente
  // dal valore — non tocca il tema condiviso, quindi non cambia
  // l'aspetto dei bottoni altrove che riusano lo stesso colore.
  ui_paint_draw_quad(x, trackY, trackW, trackHeight, sliderTheme.separator.r,
            sliderTheme.separator.g, sliderTheme.separator.b);

  float t = (max > min) ? (*value - min) / (max - min) : 0.0f;
  if (t < 0)
    t = 0;
  if (t > 1)
    t = 1;
  float handleW = fmaxf(8.0f * scale, trackHeight * 1.6f);
  float handleX = x + t * (trackW - handleW);

  // Riempimento colorato dall'inizio del binario fino alla maniglia —
  // capisci il valore a colpo d'occhio senza dover leggere il numero,
  // stesso linguaggio visivo degli slider di Blender. Solo se c'è
  // davvero qualcosa da riempire (handleX oltre il bordo sinistro).
  float fillW = (handleX + handleW * 0.5f) - x;
  if (fillW > 0.0f) {
    if (fillW > trackW)
      fillW = trackW;
    ui_paint_draw_quad(x, trackY, fillW, trackHeight, sliderTheme.button_border.r,
             sliderTheme.button_border.g, sliderTheme.button_border.b);
  }

  bool hoverTrack=ui_pointer_hit(x-6,trackY-8,trackW+12,trackHeight+16)&&ui_nav_allowed(ui_overlay_context());
  if(hoverTrack) frame_view->g_any_hover=true;
  ui_nav_register(ui_overlay_context(), id,(UIRect){x,trackY,trackW,trackHeight},UI_NAV_SLIDER,true,hoverTrack);
  float old_value=*value;
  float step=(max-min)/100.0f;
  if(ui_state_navigation(ui_overlay_context())->nav_enabled) for(size_t n=0;n<frame_view->key_event_count;++n) {
    InputKeyEvent *e=&frame_view->key_events[n];
    if(e->consumed || !ui_nav_event(ui_overlay_context(), id,n) || e->kind!=INPUT_KEY ||
       (e->action!=PLATFORM_PRESS && e->action!=PLATFORM_REPEAT)) continue;
    if(e->key==PLATFORM_KEY_LEFT || e->key==PLATFORM_KEY_DOWN) *value=fmaxf(min,*value-step);
    else if(e->key==PLATFORM_KEY_RIGHT || e->key==PLATFORM_KEY_UP) *value=fminf(max,*value+step);
    else if(e->key==PLATFORM_KEY_HOME) *value=min;
    else if(e->key==PLATFORM_KEY_END) *value=max;
    else continue;
    e->consumed=true;
  }
  t=(max>min)?(*value-min)/(max-min):0;
  handleX=x+t*(trackW-handleW);
  bool started=ui_pointer_drag(&state->dragging,hoverTrack,true);
  if(started && out_drag_started) *out_drag_started=true;

  bool changed = *value!=old_value;
  if (state->dragging) {
    float rel = (frame_view->g_mouse_x - x - handleW * 0.5f) / (trackW - handleW);
    if (rel < 0)
      rel = 0;
    if (rel > 1)
      rel = 1;
    float newValue = min + rel * (max - min);
    if (newValue != *value) {
      *value = newValue;
      changed = true;
    }
    t = rel;
    handleX = x + t * (trackW - handleW);
  }

  // Maniglia nel colore accent del tema (invece di un lavanda fisso,
  // scollegato da qualunque altra scelta di colore) — un po' più
  // chiara quando trascini, per un riscontro immediato di "la sto
  // tenendo in mano".
  float hc = state->dragging ? 1.25f : 1.0f;
  ui_paint_draw_quad(handleX, trackY - 3, handleW, trackHeight + 6,
           sliderTheme.button_border.r * hc, sliderTheme.button_border.g * hc,
           sliderTheme.button_border.b * hc);

  char valueID[UI_ID_CAPACITY + 32];
  snprintf(valueID, sizeof(valueID), "%s_exact", id);
  changed |= ui_float_field(valueID, font,
                            x + trackW + sliderTheme.label_value_gap * scale, y,
                            valueW, fieldH, labelScale, value, min, max);

  ui_nav_ring(id,(UIRect){x,trackY-3,trackW,trackHeight+6});
  return changed;
}

bool ui_slider_is_dragging(const char *id) {
  UISlidersState *sliders_view=ui_state_sliders(ui_overlay_context());

  if (!id)
    return false;
  for (int i = 0; i < sliders_view->slider_state_count; ++i)
    if (strcmp(sliders_view->slider_states[i].id, id) == 0)
      return sliders_view->slider_states[i].dragging;
  return false;
}

/* Core-only layout controls. Included after slider state/capture helpers. */
bool ui_tabs(const char *id,Font *font,UIRect bounds,const UITab *tabs,int count,int *selected) {
  if(!ui_valid_id(id)||!font||!tabs||!selected||count<=0||count>64||
     !ui_layout_rect_valid(bounds)) return false;
  for(int i=0;i<count;++i) if(!tabs[i].label) return false;
  int old=*selected,first=-1,last=-1;
  for(int i=0;i<count;++i) if(!tabs[i].disabled) { if(first<0) first=i; last=i; }
  if(*selected<0||*selected>=count||tabs[*selected].disabled) *selected=first;
  bool hover=ui_layout_pointer_hit(bounds);
  if(hover && first>=0) ui_state_frame(ui_overlay_context())->g_any_hover=true;
  ui_nav_register(ui_overlay_context(), id,bounds,UI_NAV_BUTTON,first>=0,hover);
  if(first>=0) {
    int direction=0;
    if(ui_nav_key(ui_overlay_context(), id,PLATFORM_KEY_LEFT,true)) direction=-1;
    if(ui_nav_key(ui_overlay_context(), id,PLATFORM_KEY_RIGHT,true)) direction=1;
    if(direction) for(int n=1;n<=count;++n) {
      int at=(*selected+direction*n+count*2)%count;
      if(!tabs[at].disabled) { *selected=at; break; }
    }
    if(ui_nav_key(ui_overlay_context(), id,PLATFORM_KEY_HOME,false)) *selected=first;
    if(ui_nav_key(ui_overlay_context(), id,PLATFORM_KEY_END,false)) *selected=last;
  }
  UITheme theme=ui_get_theme();
  float w=bounds.w/count;
  for(int i=0;i<count;++i) {
    UIRect r={bounds.x+i*w,bounds.y,w,bounds.h};
    bool hit=ui_layout_pointer_hit(r)&&ui_nav_allowed(ui_overlay_context());
    if(ui_pointer_press(hit,!tabs[i].disabled)) *selected=i;
  }
  for(int i=0;i<count;++i) {
    UIRect r={bounds.x+i*w,bounds.y,w,bounds.h};
    UIColor bg=i==*selected?theme.accent:theme.button_normal;
    ui_paint_draw_quad(r.x,r.y,r.w,r.h,bg.r,bg.g,bg.b);
    if(ui_clip_push(r)) {
      UIColor fg=theme.text; float dim=tabs[i].disabled?.45f:1;
      float size=fminf(14,bounds.h*.6f);
      ui_paint_draw_text_colored(font,tabs[i].label,r.x+6,
        r.y+(r.h+ui_paint_font_ascent(font,size))*.5f,size,fg.r*dim,fg.g*dim,fg.b*dim);
      ui_clip_pop();
    }
  }
  ui_nav_ring(id,bounds);
  return old!=*selected;
}
bool ui_splitter(const char *id,UIRect bounds,UISplitAxis axis,float thickness,
                 float min_first,float min_second,float *ratio,UIRect *first,UIRect *second) {
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

  if(first) *first=(UIRect){0};
  if(second) *second=(UIRect){0};
  if(!ui_valid_id(id)||!ratio||!first||!second||first==second||!ui_layout_rect_valid(bounds)||
     (axis!=UI_SPLIT_COLUMNS&&axis!=UI_SPLIT_ROWS)||!isfinite(thickness)||thickness<=0||
     !isfinite(min_first)||!isfinite(min_second)||min_first<0||min_second<0) return false;
  float length=axis==UI_SPLIT_COLUMNS?bounds.w:bounds.h;
  float bar=fminf(thickness,length),space=length-bar;
  float lo=0,hi=1;
  if(space>0) {
    double total=(double)min_first+min_second;
    if(total>space) lo=hi=(float)(min_first/total);
    else { lo=min_first/space; hi=1-min_second/space; }
  }
  float old=*ratio;
  *ratio=isfinite(*ratio)?fmaxf(lo,fminf(hi,*ratio)):(lo+hi)*.5f;
  float origin=axis==UI_SPLIT_COLUMNS?bounds.x:bounds.y;
  UIRect handle=bounds;
  if(axis==UI_SPLIT_COLUMNS) { handle.x+=space * (*ratio); handle.w=bar; }
  else { handle.y+=space * (*ratio); handle.h=bar; }
  UISliderState *state=ui_slider_state(id);
  if(!state) return false;
  bool hover=ui_layout_pointer_hit(handle)&&ui_nav_allowed(ui_overlay_context());
  if(hover) frame_view->g_any_hover=true;
  ui_nav_register(ui_overlay_context(), id,handle,UI_NAV_SLIDER,space>0,hover);
  if(ui_pointer_drag(&state->dragging,hover,space>0)) {
    state->grab_offset=(axis==UI_SPLIT_COLUMNS?frame_view->g_mouse_x:frame_view->g_mouse_y)-origin-space * (*ratio);
  }
  if(state->dragging) *ratio=((axis==UI_SPLIT_COLUMNS?frame_view->g_mouse_x:frame_view->g_mouse_y)-origin-state->grab_offset)/space;
  if(ui_nav_key(ui_overlay_context(), id,axis==UI_SPLIT_COLUMNS?PLATFORM_KEY_LEFT:PLATFORM_KEY_UP,true)) *ratio-=.01f;
  if(ui_nav_key(ui_overlay_context(), id,axis==UI_SPLIT_COLUMNS?PLATFORM_KEY_RIGHT:PLATFORM_KEY_DOWN,true)) *ratio+=.01f;
  if(ui_nav_key(ui_overlay_context(), id,PLATFORM_KEY_HOME,false)) *ratio=lo;
  if(ui_nav_key(ui_overlay_context(), id,PLATFORM_KEY_END,false)) *ratio=hi;
  *ratio=fmaxf(lo,fminf(hi,*ratio));
  *first=*second=bounds;
  if(axis==UI_SPLIT_COLUMNS) {
    first->w=space * (*ratio); handle.x=bounds.x+first->w;
    second->x=handle.x+bar; second->w=space-first->w;
  } else {
    first->h=space * (*ratio); handle.y=bounds.y+first->h;
    second->y=handle.y+bar; second->h=space-first->h;
  }
  UIColor color=(hover||state->dragging)?ui_get_theme().accent:ui_get_theme().button_border;
  ui_paint_draw_quad(handle.x,handle.y,handle.w,handle.h,color.r,color.g,color.b);
  ui_nav_ring(id,handle);
  return old!=*ratio;
}
