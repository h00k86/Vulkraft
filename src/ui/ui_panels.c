#include "ui_state_panels_internal.h"
#include "ui_state_navigation_internal.h"
#include "ui_state_ids_internal.h"
#include "ui_state_frame_internal.h"
#include "ui/ui.h"
#include "ui_state_types_internal.h"
#include "ui_navigation_internal.h"
#include "ui_paint_internal.h"
#include "ui_widget_internal.h"
#include "ui_capture_internal.h"
#include "log/log.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Public scopes operate on the selected context; state is owned by that
 * context. Drawing and input use shared facades, never a concrete backend. */
static UIContext *ui_panels_context(void) {
  UIContext *ctx=ui_context_get_current();
  assert(ctx && "Select a UIContext before using panel/scroll scopes");
  return ctx;
}

static UISectionState *find_or_create_section_state(UIContext *ctx,const char *key) {
  UIPanelsState *panels_view=ui_state_panels(ctx);
  UIFrameState *frame_view=ui_state_frame(ctx);

  if (!ui_valid_id(key)) return NULL;
  UISectionState *state = NULL;
  for (int i = 0; i < panels_view->g_section_state_count; ++i) {
    if (strcmp(panels_view->g_section_states[i].id, key) == 0) {
      state = &panels_view->g_section_states[i];
      state->last_seen_frame = frame_view->g_ui_frame_serial;
      return state;
    }
  }
  if (panels_view->g_section_state_count < MAX_UI_SECTIONS) {
    state = &panels_view->g_section_states[panels_view->g_section_state_count++];
  } else {
    for (int i = 0; i < panels_view->g_section_state_count; ++i) {
      UISectionState *candidate = &panels_view->g_section_states[i];
      if (candidate->last_seen_frame == frame_view->g_ui_frame_serial)
        continue;
      if (!state || candidate->last_seen_frame < state->last_seen_frame)
        state = candidate;
    }
  }
  if (!state) {

    if (!panels_view->warned_section_capacity) {
      LOG_WARN("UI: troppe sezioni simultanee (max %d) — id '%s' ignorato",
               MAX_UI_SECTIONS, key);
      panels_view->warned_section_capacity = true;
    }
    return NULL;
  }
  memset(state, 0, sizeof(*state));
  snprintf(state->id, sizeof(state->id), "%s", key);
  state->open = panels_view->g_section_default_open;
  state->last_seen_frame = frame_view->g_ui_frame_serial;
  return state;
}

/* Sezione che parte CHIUSA la prima volta (funzioni rare/avanzate: non
 * affollano il pannello); dopo il primo click lo stato e' quello scelto
 * dall'utente, come per ui_section_begin. */
UISection ui_section_begin_collapsed(UILayout *layout, const char *id,
                                     const char *title, Font *font) {
  UIContext *ctx=ui_panels_context();

  UIPanelsState *panels_view=ui_state_panels(ctx);

  panels_view->g_section_default_open = false;
  UISection s = ui_section_begin(layout, id, title, font);
  panels_view->g_section_default_open = true;
  return s;
}

UISection ui_section_begin(UILayout *layout, const char *id,
                           const char *title, Font *font) {
  if (!layout || !ui_valid_id(id)) return (UISection){0};
  UIContext *ctx=ui_panels_context();
  if(ui_state_ids(ctx)->g_id_stack_depth >= MAX_UI_ID_STACK) return (UISection){0};
  const char *key = ui_scoped_id(id);
  if (!ui_valid_id(key)) return (UISection){0};
  UISectionState *state = find_or_create_section_state(ctx,key);
  bool open = state ? state->open : true;
  UITheme theme = ui_get_theme();
  if (layout->cursor_y > layout->bounds.y) {
    float transition = fmaxf(0.0f, theme.section_spacing - theme.row_spacing);
    ui_layout_space(layout, transition);
  }
  float y = ui_layout_next_row(layout);
  UIButtonStyle style = {.min_width = layout->bounds.w - layout->indent,
                         .max_width = layout->bounds.w - layout->indent,
                         .min_height = layout->row_height,
                         .has_custom_color = true,
                         .color_normal = theme.panel_header,
                         .color_hover = theme.field_hover,
                         .color_pressed = theme.field,
                         .color_border = open ? theme.accent : theme.border};
  char label[160]; snprintf(label, sizeof(label), "%s %s", open ? "v" : ">", title);
  if (ui_button_labeled(key, label, font, ANCHOR_TOP_LEFT, 0,
                        14.0f * layout->scale,
                        layout->bounds.x + layout->indent, y, &style) &&
      state) {
    state->open = open = !open;
#ifdef DEBUG
    LOG_INFO("[UI SECTION] %s -> %s", key, open ? "expanded" : "collapsed");
#endif
  }
  if (open)
    ui_paint_draw_quad(layout->bounds.x + layout->indent, y,
              fmaxf(theme.separator_thickness * 2.0f, 2.0f * layout->scale),
              layout->row_height, theme.accent.r, theme.accent.g,
              theme.accent.b);
  UILayout child = ui_layout_begin_nested(layout, ui_get_theme().indent);
  if (!open) child.cursor_y = layout->cursor_y;
  ui_push_id(id);
  return (UISection){.layout = child, .parent = layout, .open = open};
}

void ui_section_end(UISection *section) {
  if (!section || !section->parent) return;
  if (section->parent && section->open)
    section->parent->cursor_y = section->layout.cursor_y;
  ui_pop_id();
}

/* La rotellina scorre il pannello sotto il cursore. Non e' bloccata da un
   campo di testo con il focus altrove ne' da un bottone tenuto premuto:
   lo era prima (qualunque cattura), e dentro il file picker la lista non
   scorreva mai. Resta bloccata da: drag di uno slider/scrollbar, lista di un
   dropdown aperta (sta sopra il pannello), menu aperto, e da un modale
   quando il pannello NON sta dentro il modale stesso. */
UIScrollScope ui_scroll_begin(const char *id, UIRect viewport, float scale) {
  UIContext *ctx=ui_panels_context();

  UIPanelsState *panels_view=ui_state_panels(ctx);
  UINavigationState *navigation_view=ui_state_navigation(ctx);
  UIFrameState *frame_view=ui_state_frame(ctx);

  const char *key = ui_scoped_id(id);
  int index = -1;
  for (int i = 0; i < panels_view->g_scroll_state_count; ++i)
    if (strcmp(panels_view->g_scroll_states[i].id, key) == 0) index = i;
  if (index < 0 && ui_valid_id(key) && panels_view->g_scroll_state_count < MAX_UI_SCROLL_SCOPES) {
    index = panels_view->g_scroll_state_count++;
    snprintf(panels_view->g_scroll_states[index].id, sizeof(panels_view->g_scroll_states[index].id), "%s", key);
  }
  UIScrollState *state = index >= 0 ? &panels_view->g_scroll_states[index] : NULL;
  float max_scroll = state ? fmaxf(0.0f, state->content_height - viewport.h) : 0.0f;
  if (state) {
    state->wheel_consumed = false;
    state->viewport_height = viewport.h;
    if (!isfinite(state->offset)) state->offset = 0.0f;
    if (state->offset > max_scroll) state->offset = max_scroll;
    if(navigation_view->nav_keyboard) {
      int at=ui_nav_find(ctx, navigation_view->nav_previous,navigation_view->nav_previous_count,navigation_view->nav_focus);
      if(at>=0 && navigation_view->nav_previous[at].scroll_index==index) {
        UINavigationEntry *entry=&navigation_view->nav_previous[at];
        if(entry->content_y<state->offset) state->offset=entry->content_y;
        else if(entry->content_y+entry->height>state->offset+viewport.h)
          state->offset=entry->content_y+entry->height-viewport.h;
        state->offset=fmaxf(0,fminf(max_scroll,state->offset));
      }
    }
    UITheme theme = ui_get_theme();
    float track_w = theme.scrollbar_width * scale;
    float thumb_h = max_scroll > 0.0f ? fmaxf(theme.row_height * scale,
        viewport.h * viewport.h / state->content_height) : viewport.h;
    float thumb_y = max_scroll > 0.0f ? viewport.y + (viewport.h - thumb_h) * (state->offset / max_scroll) : viewport.y;
    bool on_thumb=ui_pointer_hit(viewport.x+viewport.w-track_w,thumb_y,track_w,thumb_h);
    state->thumb_hovered=on_thumb;
    if(ui_pointer_drag(&state->dragging,on_thumb,max_scroll>0))
      state->drag_grab=frame_view->g_mouse_y-thumb_y;
    if (state->dragging && max_scroll > 0.0f) {
      float travel = viewport.h - thumb_h;
      float pos = fmaxf(0.0f, fminf(travel, frame_view->g_mouse_y - viewport.y - state->drag_grab));
      state->offset = travel > 0.0f ? (pos / travel) * max_scroll : 0.0f;
    }
    if (!state->dragging && ui_pointer_hit(viewport.x,viewport.y,viewport.w,viewport.h) &&
        ui_scroll_delta() != 0.0f && ui_capture_scroll_allowed(viewport)) {
      state->offset = fmaxf(0.0f, fminf(max_scroll, state->offset - ui_scroll_delta() * ui_get_theme().row_height * scale));
      state->wheel_consumed = ui_consume_scroll_delta();
    }
  }
  bool clip_pushed = ui_clip_push(viewport);
  /* Il contenuto lascia SEMPRE libera la colonna della scrollbar: prima il
     layout aveva tutta la larghezza e l'ultimo controllo a destra (freccia
     dei dropdown, bottone "...") finiva sotto la barra. */
  UIRect content = viewport;
  {
    float track_w = ui_get_theme().scrollbar_width * scale;
    if (content.w > track_w * 3.0f) content.w -= track_w + 2.0f * scale;
  }
  UILayout layout = ui_layout_begin(content, scale);
  if (state) layout.cursor_y -= state->offset;
  bool nav_pushed=state && navigation_view->nav_scroll_depth<MAX_UI_SCROLL_SCOPES;
  if(nav_pushed) {
    int depth=navigation_view->nav_scroll_depth++;
    navigation_view->nav_scroll_stack[depth]=index;
    navigation_view->nav_scroll_viewports[depth]=viewport;
  }
  return (UIScrollScope){.nav_pushed=nav_pushed,.layout = layout, .viewport = viewport,
                         .offset = state ? state->offset : 0.0f,
                         .max_scroll = max_scroll, .state_index = index,
                         .open = true, .clip_pushed = clip_pushed};
}

void ui_scroll_end(UIScrollScope *scope) {
  if (!scope || !scope->open) return;
  UIContext *ctx=ui_panels_context();

  UINavigationState *navigation_view=ui_state_navigation(ctx);
  UIPanelsState *panels_view=ui_state_panels(ctx);

  scope->open = false;
  if(scope->nav_pushed && navigation_view->nav_scroll_depth>0) --navigation_view->nav_scroll_depth;
  scope->nav_pushed=false;
  bool had_clip = scope->clip_pushed;
  scope->clip_pushed = false;
  if (had_clip) ui_clip_pop();
  if (scope->state_index < 0 || scope->state_index >= panels_view->g_scroll_state_count) return;
  UIScrollState *state = &panels_view->g_scroll_states[scope->state_index];
  state->content_height = fmaxf(0.0f, scope->layout.cursor_y + state->offset - scope->viewport.y);
  float max_scroll = fmaxf(0.0f, state->content_height - scope->viewport.h);
  if (state->offset > max_scroll) state->offset = max_scroll;
  if (!had_clip || max_scroll <= 0.0f) return;
  UITheme t = ui_get_theme();
  float track_w = t.scrollbar_width * scope->layout.scale;
  float thumb_h = fmaxf(t.row_height * scope->layout.scale,
                         scope->viewport.h * scope->viewport.h / state->content_height);
  float thumb_y = scope->viewport.y + (scope->viewport.h - thumb_h) * (state->offset / max_scroll);
  ui_paint_draw_quad(scope->viewport.x + scope->viewport.w - track_w, scope->viewport.y,
            track_w, scope->viewport.h, t.separator.r, t.separator.g, t.separator.b);
  UIColor thumb = state->thumb_hovered || state->dragging ? t.accent
                                                           : t.field_hover;
  float inset = fminf(2.0f * scope->layout.scale, track_w * 0.25f);
  ui_paint_draw_quad(scope->viewport.x + scope->viewport.w - track_w + inset,
            thumb_y + inset, track_w - inset * 2.0f,
            fmaxf(2.0f, thumb_h - inset * 2.0f), thumb.r, thumb.g, thumb.b);
}

bool ui_scroll_get_diagnostics(const char *id, UIScrollDiagnostics *out) {
  if (!id || !id[0] || !out)
    return false;
  UIContext *ctx=ui_panels_context();

  UIPanelsState *panels_view=ui_state_panels(ctx);

  const char *key = ui_scoped_id(id);
  UIScrollState *state = NULL;
  const char *drag_owner = NULL;
  for (int i = 0; i < panels_view->g_scroll_state_count; ++i) {
    if (panels_view->g_scroll_states[i].dragging)
      drag_owner = panels_view->g_scroll_states[i].id;
    if (strcmp(panels_view->g_scroll_states[i].id, key) == 0)
      state = &panels_view->g_scroll_states[i];
  }
  if (!state)
    return false;
  out->id = state->id;
  out->scrollbar_drag_owner = drag_owner;
  out->offset = state->offset;
  out->content_height = state->content_height;
  out->viewport_height = state->viewport_height;
  out->max_scroll = fmaxf(0.0f, state->content_height - out->viewport_height);
  out->wheel_consumed = state->wheel_consumed;
  out->scrollbar_dragging = state->dragging;
  return true;
}

UIPanel ui_panel_begin(const char *id, const char *title,
                       Font *font, UIRect bounds, float scale) {
  UITheme theme = ui_get_theme();
  if (scale <= 0.0f) scale = 1.0f;
  float pad = theme.panel_padding * scale;
  float border = theme.border_thickness * scale;
  ui_paint_draw_quad(bounds.x - border, bounds.y - border,
            bounds.w + border * 2.0f, bounds.h + border * 2.0f,
            theme.border.r, theme.border.g, theme.border.b);
  ui_paint_draw_quad(bounds.x, bounds.y, bounds.w, bounds.h, theme.panel.r,
            theme.panel.g, theme.panel.b);
  float header_h = 0.0f;
  if (title && title[0]) {
    header_h = theme.panel_header_height * scale;
    ui_paint_draw_quad(bounds.x, bounds.y, bounds.w, header_h,
              theme.panel_header.r, theme.panel_header.g, theme.panel_header.b);
    ui_paint_draw_quad(bounds.x, bounds.y + header_h - theme.separator_thickness,
              bounds.w, theme.separator_thickness, theme.separator.r,
              theme.separator.g, theme.separator.b);
    float title_scale = 14.0f * scale * theme.section_text_scale;
    ui_paint_draw_text_colored(font, title, bounds.x + pad,
                      bounds.y + (header_h + ui_paint_font_ascent(font, title_scale)) * 0.5f,
                      title_scale, theme.text.r, theme.text.g, theme.text.b);
  }
  UIRect content = {bounds.x + pad, bounds.y + header_h + pad,
                    fmaxf(0.0f, bounds.w - pad * 2.0f),
                    fmaxf(0.0f, bounds.h - header_h - pad * 2.0f)};
  ui_input_region_register(ui_scoped_id(id), bounds);
  bool clip_pushed = ui_clip_push(content);
  return (UIPanel){.outer = bounds, .content = content,
                   .layout = ui_layout_begin(content, scale), .open = true,
                   .clip_pushed = clip_pushed};
}

void ui_panel_end(UIPanel *panel) {
  if (!panel || !panel->open) return;
  panel->open = false;
  ui_layout_end(&panel->layout);
  if (panel->clip_pushed) {
    panel->clip_pushed = false;
    ui_clip_pop();
  }
}

/* Collapsible headers share the panel registry and compose public buttons. */
static UIPanelState *find_or_create_panel_state(const char *id) {
  UIPanelsState *panels_view=ui_state_panels(ui_panels_context());

  if (!ui_valid_id(id)) return NULL;
  for (int i = 0; i < panels_view->panel_state_count; i++) {
    if (strcmp(panels_view->panel_states[i].id, id) == 0)
      return &panels_view->panel_states[i];
  }
  if (panels_view->panel_state_count < MAX_UI_PANELS) {
    UIPanelState *s = &panels_view->panel_states[panels_view->panel_state_count++];
    strncpy(s->id, id, sizeof(s->id) - 1);
    s->id[sizeof(s->id) - 1] = '\0';
    s->collapsed = false; // default: sempre espanso, comportamento di
                          // sempre finché non lo comprimi tu esplicitamente
    return s;
  }

  if (!panels_view->warned_panel_capacity) {
    LOG_ERROR("ui_collapsible_header: pool esaurito (max %d) — pannelli "
              "oltre questo numero non si comprimeranno più, id ignorato: "
              "'%s'",
              MAX_UI_PANELS, id);
    panels_view->warned_panel_capacity = true;
  }
  return NULL;
}

// Intestazione di pannello comprimibile: un bottone a tutta larghezza
// con un prefisso v/> davanti all'etichetta — ASCII, non frecce
// unicode (▼/▶): non ho modo di verificare se il set di glifi del
// font le contiene, un carattere mancante sparirebbe in silenzio
// invece di dare un errore. Ritorna true se il pannello è ESPANSO — il
// chiamante disegna il contenuto sotto SOLO in quel caso, e deve
// calcolare l'altezza del proprio sfondo di conseguenza (un pannello
// compresso occupa solo l'altezza di questa intestazione). Lo stato
// persiste per id tra un frame e l'altro, stesso principio di
// ui_dropdown per aperto/chiuso.
bool ui_collapsible_header(const char *id, const char *label,
                           Font *font, float x, float y, float width,
                           float scale) {
  if (!ui_valid_id(id)) return false;
  UIPanelState *state = find_or_create_panel_state(id);
  // Pool esaurito: sempre espanso (fallback sicuro, comportamento di
  // sempre) invece di un pannello che non riesci più ad aprire.
  bool expanded = state ? !state->collapsed : true;

  char displaySectionLabel[80];
  ui_humanize_label(label, displaySectionLabel, sizeof(displaySectionLabel));
  char fullLabel[96];
  snprintf(fullLabel, sizeof(fullLabel), "%s  %s", expanded ? "v" : ">",
           displaySectionLabel);

  // Trattamento da SEZIONE, non da bottone d'azione: niente bordo,
  // sfondo solo leggermente più chiaro del pannello sotto — prima
  // usava lo stile bottone di default (bordo pieno, stesso contrasto
  // di un vero bottone cliccabile tipo ELIMINA/SALVA), risultato:
  // intestazioni di sezione indistinguibili da azioni vere. Qui la
  // gerarchia è Panel -> Section -> Property, non tre rettangoli
  // grigi tutti uguali.
  UITheme theme = ui_get_theme();
  UIButtonStyle style = {.min_width = width, .max_width = width,
                         .min_height = 0.0f};
  style.border = -1.0f; // niente bordo
  style.has_custom_color = true;
  style.color_normal = theme.panel_header;
  style.color_hover = theme.field_hover;
  style.color_pressed = theme.field;
  style.color_border = theme.border;

  if (ui_button_labeled(id, fullLabel, font, ANCHOR_TOP_LEFT, 0, scale,
                        x, y, &style)) {
    if (state)
      state->collapsed = !state->collapsed;
  }

  return expanded;
}
