#include "ui_state_navigation_internal.h"
#include "ui_state_frame_internal.h"
#include "ui_state_dropdowns_internal.h"
#include "ui/ui.h"
#include "ui_error_internal.h"
#include "ui_overlay_internal.h"
#include "ui_capture_internal.h"
#include "ui_widget_internal.h"
#include "ui_paint_internal.h"
#include "ui_navigation_internal.h"
#include "log/log.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void ui_close_all_dropdowns(void) {
  UIDropdownsState *dropdowns_view=ui_state_dropdowns(ui_overlay_context());

  for (int i = 0; i < dropdowns_view->dropdown_state_count; ++i)
    dropdowns_view->dropdown_states[i].open = false;
  ui_capture_forget_dropdown(ui_overlay_context(),NULL);
}

void ui_reset_dropdown_popup_pass(void) {
  UIDropdownsState *dropdowns_view=ui_state_dropdowns(ui_overlay_context());

  dropdowns_view->g_dropdown_submission_serial = 0;
  dropdowns_view->g_dropdown_popup_pass_drawn = false;
  for (int i = 0; i < dropdowns_view->dropdown_state_count; ++i) {
    /* popupSubmitted/OpenRequested still describe the completed previous
     * frame here. An open widget that was not emitted there is stale state,
     * not current ownership. */
    if (dropdowns_view->dropdown_states[i].open && !dropdowns_view->dropdown_states[i].popupSubmitted &&
        !dropdowns_view->dropdown_states[i].popupOpenRequested)
      dropdowns_view->dropdown_states[i].open = false;
    dropdowns_view->dropdown_states[i].popupSubmitted =
        dropdowns_view->dropdown_states[i].popupOpenRequested = false;
  }
}

static UIDropdownState *find_or_create_dropdown_state(const char *id) {
  UIDropdownsState *dropdowns_view=ui_state_dropdowns(ui_overlay_context());
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

  if (!ui_valid_id(id)) {
    ui_report_error(ui_overlay_context(),UI_ERROR_ARGUMENT,UI_OPERATION_DROPDOWN,id); return NULL;
  }
  for (int i = 0; i < dropdowns_view->dropdown_state_count; i++) {
    if (strcmp(dropdowns_view->dropdown_states[i].id, id) == 0) {
      dropdowns_view->dropdown_states[i].last_seen_frame = frame_view->g_ui_frame_serial;
      return &dropdowns_view->dropdown_states[i];
    }
  }
  if (dropdowns_view->dropdown_state_count < MAX_UI_DROPDOWNS) {
    UIDropdownState *s = &dropdowns_view->dropdown_states[dropdowns_view->dropdown_state_count++];
    memset(s, 0, sizeof(*s));
    snprintf(s->id, sizeof(s->id), "%s", id);
    s->last_seen_frame = frame_view->g_ui_frame_serial;
    return s;
  }
  UIDropdownState *reuse = NULL;
  for (int i = 0; i < dropdowns_view->dropdown_state_count; ++i) {
    UIDropdownState *candidate = &dropdowns_view->dropdown_states[i];
    if (candidate->open || candidate->popupSubmitted ||
        candidate->popupOpenRequested ||
        candidate->last_seen_frame == frame_view->g_ui_frame_serial)
      continue;
    if (!reuse || candidate->last_seen_frame < reuse->last_seen_frame)
      reuse = candidate;
  }
  if (reuse) {
    ui_capture_forget_dropdown(ui_overlay_context(),reuse);
    char (*options)[128]=reuse->popupOptions;
    int capacity=reuse->popupOptionCapacity;
    memset(reuse, 0, sizeof(*reuse));
    reuse->popupOptions=options;
    reuse->popupOptionCapacity=capacity;
    snprintf(reuse->id, sizeof(reuse->id), "%s", id);
    reuse->last_seen_frame = frame_view->g_ui_frame_serial;
    return reuse;
  }

  ui_report_error(ui_overlay_context(),UI_ERROR_LIMIT,UI_OPERATION_DROPDOWN,id);
  if (!dropdowns_view->warned_dropdown_capacity) {
    LOG_WARN("UI: troppi dropdown simultanei (max %d) — id '%s' ignorato",
             MAX_UI_DROPDOWNS, id);
    dropdowns_view->warned_dropdown_capacity = true;
  }
  return NULL;
}

// Forza programmaticamente lo stato aperto/chiuso di UN dropdown per
// ID — serve ai sottomenu a cascata (es. FILE -> "CAMBIA SLOT"): quando
// scegli la voce "genitore" nel menu sopra, il sottomenu deve aprirsi
// SUBITO, non aspettare un secondo click sulla propria intestazione
// (che il find_or_create_dropdown_state normale creerebbe sempre
// chiusa la prima volta). Il resto del comportamento (scroll, click
// fuori chiude, overflow schermo) resta identico a un dropdown normale
// — questo tocca SOLO il flag open.
void ui_dropdown_set_open(const char *id, bool open) {
  UIDropdownState *state = find_or_create_dropdown_state(id);
  if (state) {
    state->open = open;
    if (open)
      state->popupOpenRequested = true;
    if (!open) ui_capture_forget_dropdown(ui_overlay_context(),state);
  }
}

// Y di apertura della lista di un dropdown. Prima la scelta era binaria
// ("se sotto non basta, apri sopra"), senza controllare se sopra ci fosse
// davvero spazio: per un dropdown vicino alla CIMA dello schermo (es. il
// selettore texture nella toolbar, y=15) con molte opzioni, "sopra" ha
// ancora meno spazio di "sotto" — il vecchio codice apriva comunque verso
// l'alto, spingendo la lista a una Y negativa e facendola sparire oltre
// il bordo superiore. Ora si sceglie la direzione con PIÙ spazio libero,
// e in ogni caso il risultato viene sempre clampato dentro lo schermo:
// anche una lista più alta dello spazio disponibile in ENTRAMBE le
// direzioni resta ancorata e leggibile, non sparisce fuori vista.
static float ui_dropdown_list_top(float y, float h, float listH,
                                  bool *out_open_upward) {
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

  float spaceBelow = frame_view->canvas_height - 10.0f - (y + h);
  float spaceAbove = y - 10.0f;
  bool openUpward = (listH > spaceBelow) && (spaceAbove > spaceBelow);

  float top = openUpward ? (y - listH) : (y + h);
  if (top < 10.0f)
    top = 10.0f;
  if (top + listH > frame_view->canvas_height - 10.0f)
    top = frame_view->canvas_height - 10.0f - listH;
  if (top < 10.0f) // lista più alta dello schermo intero: resta ancorata
    top = 10.0f;  // in cima invece di sforare sopra

  if (out_open_upward)
    *out_open_upward = openUpward;
  return top;
}

// Quante opzioni mostrare insieme in una lista aperta, e se serve lo
// scroll. Prima la lista dichiarava sempre un'altezza pari a TUTTE le
// opzioni (rowH * option_count): con poche opzioni ui_dropdown_list_top
// riusciva a farla stare comunque, ma con tante (es. molte texture) quel
// totale poteva superare l'intero schermo — nessuna posizione, sopra o
// sotto, bastava. Il clamp spostava il CONTENITORE dentro lo schermo, ma
// il suo contenuto (ancora alto quanto tutte le opzioni) continuava a
// sforare comunque. Qui si limita il numero di righe MOSTRATE a quelle
// che entrano davvero, con due frecce ^/v per scorrere il resto.
static int ui_dropdown_visible_rows(int option_count, float rowH,
                                    bool *out_needs_scroll) {
  float availH = ui_state_frame(ui_overlay_context())->canvas_height - 20.0f;
  int fitAll = (rowH > 0.0f) ? (int)(availH / rowH) : option_count;
  if (fitAll < 1)
    fitAll = 1;

  if (option_count <= fitAll) {
    if (out_needs_scroll)
      *out_needs_scroll = false;
    return option_count;
  }

  if (out_needs_scroll)
    *out_needs_scroll = true;
  float availForRows =
      availH - 2.0f * (UI_DROPDOWN_SCROLL_ARROW_H + UI_DROPDOWN_SCROLL_GAP);
  int fit = (rowH > 0.0f) ? (int)(availForRows / rowH) : 1;
  if (fit < 1)
    fit = 1;
  if (fit > option_count)
    fit = option_count;
  return fit;
}

bool ui_dropdown_is_open(const char *id) {
  UIDropdownsState *dropdowns_view=ui_state_dropdowns(ui_overlay_context());

  if (!ui_valid_id(id)) return false;
  for (int i = 0; i < dropdowns_view->dropdown_state_count; i++) {
    if (strcmp(dropdowns_view->dropdown_states[i].id, id) == 0)
      return dropdowns_view->dropdown_states[i].open;
  }
  return false;
}

bool ui_dropdown(const char *id, Font *font, TextAnchor anchor,
                 float margin, float scale, float offset_x, float offset_y,
                 float width, const char *const *options, int option_count,
                 int *selected_index, const char *header_label,
                 const UIButtonStyle *style) {
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());
  UINavigationState *navigation_view=ui_state_navigation(ui_overlay_context());

  UIDropdownState *state = find_or_create_dropdown_state(id);
  if (!state) return false;
  if (!options || option_count <= 0 || !selected_index) {
    ui_report_error(ui_overlay_context(),UI_ERROR_ARGUMENT,UI_OPERATION_DROPDOWN,id); return false;
  }

  // Menu-azione (FILE, EDIT...) passano sempre un -1 "usa e getta",
  // mai un valore che persiste da un frame all'altro — non hanno una
  // "voce già scelta" da ricordare, sono solo una lista di comandi.
  // Tracciato QUI, prima del clamp sotto, perché una volta forzato a 0
  // diventa indistinguibile da "la voce 0 è genuinamente selezionata"
  // (un vero dropdown a valore persistente, tipo MODALITA' o TAB).
  bool hasRealSelection = (*selected_index >= 0);

  if (*selected_index < 0)
    *selected_index = 0;
  if (*selected_index >= option_count)
    *selected_index = option_count - 1;

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

  // Overflow orizzontale: se il dropdown finirebbe oltre il bordo destro
  // dello schermo, lo si rientra così header E lista aperta restano
  // interamente visibili. Prima esisteva solo l'equivalente verticale
  // (openUpward, sotto) — mancava questo, ed è la causa diretta dei
  // dropdown che escono dallo schermo quando la toolbar li piazza a X
  // alte (es. TEXTURE/PREFAB, disegnati dopo FILE/EDIT/AIUTO).
  if (x + w > frame_view->canvas_width - 10.0f)
    x = frame_view->canvas_width - 10.0f - w;
  if (x < 10.0f)
    x = 10.0f;

  bool enabled=!style || !style->disabled;
  bool headerHover = enabled && ui_pointer_hit(x, y, w, h);
  ui_nav_register(ui_overlay_context(), id,(UIRect){x,y,w,h},UI_NAV_DROPDOWN,enabled,headerHover);
  if(!enabled || (navigation_view->nav_enabled && navigation_view->nav_keyboard && !ui_nav_focused(ui_overlay_context(), id))) state->open=false;
  if (headerHover)
    frame_view->g_any_hover = true;

  float rowH = h;

  // Righe mostrate insieme e se serve lo scroll — calcolato una volta,
  // non dipende da state->open (solo l'altezza EFFETTIVA della lista,
  // sotto, dipende da se è aperta o no).
  bool needsScroll = false;
  int visibleRows = ui_dropdown_visible_rows(option_count, rowH, &needsScroll);
  int maxScroll = option_count - visibleRows;
  if (maxScroll < 0)
    maxScroll = 0;
  if (state->scrollOffset > maxScroll)
    state->scrollOffset = maxScroll;
  if (state->scrollOffset < 0)
    state->scrollOffset = 0;

  float rowsH = rowH * (float)visibleRows;
  float scrollBarsH =
      needsScroll ? 2.0f * (UI_DROPDOWN_SCROLL_ARROW_H + UI_DROPDOWN_SCROLL_GAP)
                  : 0.0f;
  float fullListH = rowsH + scrollBarsH;

  // La geometria è sempre quella della lista completa: nello stesso frame in
  // cui un header apre il menu deve essere pronta per il pass overlay e per
  // il precheck di input del frame successivo.
  bool openUpward = false;
  float listTop = ui_dropdown_list_top(y, h, fullListH, &openUpward);
  float rowsTop = listTop + (needsScroll ? (UI_DROPDOWN_SCROLL_ARROW_H +
                                            UI_DROPDOWN_SCROLL_GAP)
                                         : 0.0f);

  // Geometria vera salvata per query esterne (vedi ui_dropdown_row_position
  // sotto) — SEMPRE, aperto o chiuso: un sottomenu a cascata deve poter
  // sapere "dove sarebbe la riga N" ancora prima che la lista sia
  // visibile (nel frame in cui hai appena cliccato la voce che apre il
  // sottomenu, questo dropdown genitore potrebbe già essere richiuso).
  state->lastX = x;
  state->lastRowsTop = rowsTop;
  state->lastRowH = rowH;
  state->lastWidth = w;
  state->lastListTop = listTop;
  state->lastListH = state->open ? fullListH : 0.0f;

  bool listHover = false;
  bool upArrowHover = false, downArrowHover = false;
  if (state->open) {
    listHover = ui_pointer_hit(x, listTop, w, fullListH);
    if (listHover)
      frame_view->g_any_hover = true;
    if (needsScroll) {
      upArrowHover = ui_pointer_hit(x, listTop, w,
                                   UI_DROPDOWN_SCROLL_ARROW_H);
      downArrowHover = ui_pointer_hit(x,
                                     listTop + fullListH - UI_DROPDOWN_SCROLL_ARROW_H,
                                     w, UI_DROPDOWN_SCROLL_ARROW_H);
    }
  }

  bool clickedNow = ui_capture_dropdown_press(ui_overlay_context(),state,enabled);

  int clickedOption = -1;
  if (clickedNow) {
    if (headerHover) {
      state->open = !state->open;
      state->keyboard_index=*selected_index;
      ui_capture_claim_dropdown(ui_overlay_context(),state);
    } else if (state->open && needsScroll && upArrowHover) {
      if (state->scrollOffset > 0)
        state->scrollOffset--;
    } else if (state->open && needsScroll && downArrowHover) {
      if (state->scrollOffset < maxScroll)
        state->scrollOffset++;
    } else if (state->open && listHover) {
      int rel = (int)((frame_view->g_mouse_y - rowsTop) / rowH);
      if (rel >= 0 && rel < visibleRows) {
        int optIdx = state->scrollOffset + rel;
        if (optIdx >= 0 && optIdx < option_count)
          clickedOption = optIdx;
      }
    } else if (state->open) {
      state->open = false; // click fuori: chiude senza selezionare
    }
  }

  if(enabled && navigation_view->nav_enabled && ui_nav_allowed(ui_overlay_context())) {
    for(size_t n=0;n<frame_view->key_event_count;++n) {
      InputKeyEvent *e=&frame_view->key_events[n];
      if(e->consumed || !ui_nav_event(ui_overlay_context(), id,n) || e->kind!=INPUT_KEY ||
         (e->action!=PLATFORM_PRESS && e->action!=PLATFORM_REPEAT)) continue;
      bool confirm=e->key==PLATFORM_KEY_ENTER || e->key==PLATFORM_KEY_KP_ENTER || e->key==PLATFORM_KEY_SPACE;
      if(confirm && e->action==PLATFORM_PRESS) {
        if(state->open) { clickedOption=state->keyboard_index; state->open=false; }
        else { state->open=true; state->keyboard_index=*selected_index; }
      } else if(e->key==PLATFORM_KEY_DOWN || e->key==PLATFORM_KEY_UP) {
        if(!state->open) { state->open=true; state->keyboard_index=*selected_index; }
        else state->keyboard_index+=e->key==PLATFORM_KEY_DOWN?1:-1;
      } else if(state->open && e->key==PLATFORM_KEY_HOME) state->keyboard_index=0;
      else if(state->open && e->key==PLATFORM_KEY_END) state->keyboard_index=option_count-1;
      else if(state->open && e->key==PLATFORM_KEY_ESCAPE) {
        state->open=false; ui_capture_mark_escape(ui_overlay_context());
      } else continue;
      e->consumed=true;
      if(state->keyboard_index<0) state->keyboard_index=0;
      if(state->keyboard_index>=option_count) state->keyboard_index=option_count-1;
      if(state->keyboard_index<state->scrollOffset) state->scrollOffset=state->keyboard_index;
      if(state->keyboard_index>=state->scrollOffset+visibleRows) state->scrollOffset=state->keyboard_index-visibleRows+1;
    }
  }

  // ESC chiude la lista aperta senza cambiare la selezione: il dropdown
  // tiene la cattura della tastiera (ui_has_keyboard_capture) quindi deve
  // anche rilasciarla con lo stesso tasto usato da menu e modali.
  if (state->open && ui_capture_keyboard_allowed(ui_overlay_context()) && ui_capture_consume_escape(ui_overlay_context()))
    state->open = false;

  bool changed = false;
  if (clickedOption >= 0) {
    *selected_index = clickedOption;
    changed = true;
    state->open = false;
  }

  state->lastListH = state->open ? fullListH : 0.0f;

  /* Reserve only visible rows. Keep the old allocation intact on failure,
   * close the unavailable popup and forget prior-frame pointer ownership.
   * The opening gesture remains consumed, preventing click-through. */
  int popupRows=visibleRows>128?128:visibleRows;
  if (state->open && state->popupOptionCapacity<popupRows) {
    int capacity=state->popupOptionCapacity?state->popupOptionCapacity:8;
    while (capacity<popupRows) capacity*=2;
    char (*options_copy)[128]=realloc(state->popupOptions,(size_t)capacity*sizeof(*options_copy));
    if (options_copy) {
      state->popupOptions=options_copy;
      state->popupOptionCapacity=capacity;
    } else {
      ui_report_error(ui_overlay_context(),UI_ERROR_MEMORY,UI_OPERATION_DROPDOWN,id);
      state->open=false;
      state->popupSubmitted=false;
      state->popupOpenRequested=false;
      state->lastListH=0;
      ui_capture_forget_dropdown(ui_overlay_context(),state);
    }
  }

  bool custom = style && style->has_custom_color;
  UIColor normal = custom ? style->color_normal : theme.field;
  UIColor hover = custom ? style->color_hover : theme.field_hover;
  UIColor pressed = custom ? style->color_pressed : theme.selected;
  UIColor borderColor = custom ? style->color_border : theme.accent;

  float border = theme.border_thickness;
  if (style && style->border < 0)
    border = 0.0f;
  else if (style && style->border > 0)
    border = style->border;

  UIColor headerFill = headerHover ? hover : normal;
  if (theme.paper_panels) {
    const float fill[4] = {headerFill.r,headerFill.g,headerFill.b,1};
    const float accent[3] = {theme.accent.r,theme.accent.g,theme.accent.b};
    ui_paint_draw_hud_paper_panel(x,y,w,h,fill,accent,headerHover || state->open,ui_hash_id(id));
  } else if (border > 0.0f) {
    // Sottile/smorzato a riposo, pieno solo in hover — un dropdown
    // deve leggersi come un MENU, non come un bottone d'azione: prima
    // usava sempre il colore accent pieno, identico a un bottone
    // premuto, anche quando non ci stavi nemmeno passando sopra col
    // mouse.
    UIColor resting = state->open ? theme.accent : theme.border;
    float br = headerHover ? borderColor.r : resting.r;
    float bg = headerHover ? borderColor.g : resting.g;
    float bb = headerHover ? borderColor.b : resting.b;
    ui_paint_draw_quad(x - border, y - border, w + border * 2, h + border * 2,
              br, bg, bb);
  }
  if (!theme.paper_panels)
    ui_paint_draw_quad(x, y, w, h, headerFill.r, headerFill.g, headerFill.b);

  char displayHeaderLabel[192];
  ui_humanize_label(header_label ? header_label : options[*selected_index],
                    displayHeaderLabel, sizeof(displayHeaderLabel));
  char headerText[192];
  ui_ellipsize_left(font, displayHeaderLabel, scale,
                    w - padding_x * 2.0f - 18.0f, headerText,
                    sizeof(headerText));
  ui_paint_draw_text_colored(font, headerText, x + padding_x,
                    y + (h + ascent) * 0.5f, scale, theme.text.r, theme.text.g,
                    theme.text.b);
  const char *arrow = state->open ? "^" : "v";
  float arrow_w = ui_paint_measure_text_width(font, arrow, scale);
  ui_paint_draw_text_colored(font, arrow, x + w - padding_x - arrow_w,
                    y + (h + ascent) * 0.5f, scale, theme.text_muted.r,
                    theme.text_muted.g, theme.text_muted.b);

  // La lista non viene più emessa qui: i controlli chiamati dopo questo
  // dropdown la coprirebbero. L'istantanea qui sotto è autonoma e viene
  // disegnata nel pass overlay al termine dell'HUD.
  state->popupSubmitted = state->open;
  if (state->popupSubmitted) {
    state->popupOrder = ui_state_dropdowns(ui_overlay_context())->g_dropdown_submission_serial++;
    state->popupFont = font;
    state->popupScale = scale;
    state->popupPaddingX = padding_x;
    state->popupAscent = ascent;
    state->popupX = x;
    state->popupW = w;
    state->popupRowH = rowH;
    state->popupListTop = listTop;
    state->popupListH = fullListH;
    state->popupRowsTop = rowsTop;
    state->popupBorder = border;
    state->popupVisibleRows = popupRows;
    state->popupSelectedIndex = ui_nav_focused(ui_overlay_context(), id)&&navigation_view->nav_keyboard?state->keyboard_index:*selected_index;
    state->popupNeedsScroll = needsScroll;
    state->popupOpenUpward = openUpward;
    state->popupHasRealSelection = hasRealSelection;
    state->popupCanScrollUp = state->scrollOffset > 0;
    state->popupCanScrollDown = state->scrollOffset < maxScroll;
    state->popupNormal = normal;
    state->popupHover = hover;
    state->popupPressed = pressed;
    state->popupBorderColor = borderColor;
    for (int i = 0; i < state->popupVisibleRows; ++i) {
      int optIdx = state->scrollOffset + i;
      char displayOption[128];
      ui_humanize_label(options[optIdx], displayOption, sizeof(displayOption));
      ui_ellipsize_left(font, displayOption, scale, w - padding_x * 2.0f,
                        state->popupOptions[i],
                        sizeof(state->popupOptions[i]));
    }
  }

  ui_nav_ring(id,(UIRect){x,y,w,h});
  return changed;
}

static void ui_draw_one_dropdown_popup(const UIDropdownState *state) {
  const float x = state->popupX;
  const float w = state->popupW;
  const float rowH = state->popupRowH;
  const float listTop = state->popupListTop;
  const float listH = state->popupListH;
  const float rowsTop = state->popupRowsTop;
  const UITheme theme = ui_get_theme();

  if (state->popupNeedsScroll) {
    bool upHover = ui_pointer_hit(x, listTop, w,
                                 UI_DROPDOWN_SCROLL_ARROW_H);
    float downY = listTop + listH - UI_DROPDOWN_SCROLL_ARROW_H;
    bool downHover = ui_pointer_hit(x, downY, w,
                                   UI_DROPDOWN_SCROLL_ARROW_H);
    UIColor upFill = !state->popupCanScrollUp
                          ? state->popupPressed
                          : (upHover ? state->popupHover : state->popupNormal);
    UIColor downFill = !state->popupCanScrollDown
                            ? state->popupPressed
                            : (downHover ? state->popupHover
                                         : state->popupNormal);
    ui_paint_draw_quad(x, listTop, w, UI_DROPDOWN_SCROLL_ARROW_H, upFill.r,
              upFill.g, upFill.b);
    ui_paint_draw_quad(x, downY, w, UI_DROPDOWN_SCROLL_ARROW_H, downFill.r,
              downFill.g, downFill.b);
    const float arrowScale = 16.0f;
    const float arrowAscent = ui_paint_font_ascent(state->popupFont, arrowScale);
    const float upW = ui_paint_measure_text_width(state->popupFont, "^", arrowScale);
    const float downW = ui_paint_measure_text_width(state->popupFont, "v", arrowScale);
    ui_paint_draw_text_colored(state->popupFont, "^", x + (w - upW) * 0.5f,
                      listTop + (UI_DROPDOWN_SCROLL_ARROW_H + arrowAscent) * 0.5f,
                      arrowScale, theme.text.r, theme.text.g, theme.text.b);
    ui_paint_draw_text_colored(state->popupFont, "v", x + (w - downW) * 0.5f,
                      downY + (UI_DROPDOWN_SCROLL_ARROW_H + arrowAscent) * 0.5f,
                      arrowScale, theme.text.r, theme.text.g, theme.text.b);
  }

  for (int i = 0; i < state->popupVisibleRows; ++i) {
    float rowY = rowsTop + (float)i * rowH;
    bool rowHover = ui_pointer_hit(x, rowY, w, rowH);
    UIColor rowFill =
        (state->popupHasRealSelection &&
         state->scrollOffset + i == state->popupSelectedIndex)
            ? state->popupPressed
            : (rowHover ? state->popupHover : state->popupNormal);
    ui_paint_draw_quad(x, rowY, w, rowH, rowFill.r, rowFill.g, rowFill.b);
    ui_paint_draw_text_colored(state->popupFont, state->popupOptions[i],
                      x + state->popupPaddingX,
                      rowY + (rowH + state->popupAscent) * 0.5f,
                      state->popupScale, theme.text.r, theme.text.g,
                      theme.text.b);
  }

  if (state->popupBorder > 0.0f) {
    UIColor border = state->popupBorderColor;
    ui_paint_draw_quad(x - state->popupBorder, listTop, state->popupBorder,
              listH, border.r, border.g, border.b);
    ui_paint_draw_quad(x + w, listTop, state->popupBorder, listH, border.r,
              border.g, border.b);
    float farEdgeY = state->popupOpenUpward ? listTop : listTop + listH;
    ui_paint_draw_quad(x, farEdgeY, w, state->popupBorder, border.r, border.g,
              border.b);
  }
}

void ui_draw_dropdown_popups(void) {
  UIDropdownsState *dropdowns_view=ui_state_dropdowns(ui_overlay_context());

  if (dropdowns_view->g_dropdown_popup_pass_drawn)
    return;
  dropdowns_view->g_dropdown_popup_pass_drawn = true;

  // Chiudi stato rimasto da una schermata che non ha più emesso quel widget.
  // Un'apertura programmatica nello stesso frame (sottomenu FILE) è invece
  // intenzionale e viene conservata fino alla sua prima draw nel frame dopo.
  for (int i = 0; i < dropdowns_view->dropdown_state_count; ++i) {
    UIDropdownState *state = &dropdowns_view->dropdown_states[i];
    if (state->open && !state->popupSubmitted && !state->popupOpenRequested)
      state->open = false;
  }

  for (int order = 0; order < dropdowns_view->g_dropdown_submission_serial; ++order) {
    for (int i = 0; i < dropdowns_view->dropdown_state_count; ++i) {
      UIDropdownState *state = &dropdowns_view->dropdown_states[i];
      if (state->open && state->popupSubmitted && state->popupOrder == order)
        ui_draw_one_dropdown_popup(state);
    }
  }
}
