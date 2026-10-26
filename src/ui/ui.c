#include <GLFW/glfw3.h>
#include "ui/ui.h"
#include "render/texture_manager.h"

#include "app/application.h"
#include "platform/paths.h"
#include "log/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "platform/platform_fs.h"
/* Riorganizzazione UI 2026-09-17: 128 bottoni erano meno delle righe di una
 * gerarchia media (ogni riga e' un bottone): oltre il limite i bottoni
 * dell'inspector SPARIVANO in silenzio (find_or_create_state -> NULL).
 * Il pool e' un cache limitato ma capiente; la ricerca confronta prima un
 * hash dell'id, cosi' il costo per lookup resta piatto anche pieno. */
#define MAX_UI_ELEMENTS 512
#define MAX_UI_INPUT_REGIONS 32
#define MAX_UI_ID_STACK 16
#define UI_SCOPED_ID_BUFFER_COUNT 16
#define MAX_UI_SECTIONS 64
#define MAX_UI_SCROLL_SCOPES 32

static char g_id_stack[MAX_UI_ID_STACK][64];
static int g_id_stack_depth = 0;
static char g_scoped_id_buffers[UI_SCOPED_ID_BUFFER_COUNT][256];
static int g_scoped_id_buffer_index = 0;
static unsigned long long g_ui_frame_serial = 1;

typedef struct {
  char id[128];
  bool open;
  unsigned long long last_seen_frame;
} UISectionState;
static UISectionState g_section_states[MAX_UI_SECTIONS];
static int g_section_state_count = 0;
typedef struct {
  char id[128];
  float offset, content_height, viewport_height, drag_grab;
  bool dragging, wheel_consumed, thumb_hovered;
} UIScrollState;
static UIScrollState g_scroll_states[MAX_UI_SCROLL_SCOPES];
static int g_scroll_state_count = 0;

bool ui_push_id(const char *scope) {
  if (!scope || !scope[0] || g_id_stack_depth >= MAX_UI_ID_STACK) {
#ifdef DEBUG
    LOG_WARN("UI ID stack overflow or empty scope");
#endif
    return false;
  }
  snprintf(g_id_stack[g_id_stack_depth], sizeof(g_id_stack[0]), "%s", scope);
  g_id_stack_depth++;
  return true;
}

void ui_pop_id(void) {
  if (g_id_stack_depth <= 0) {
#ifdef DEBUG
    LOG_WARN("UI ID stack underflow");
#endif
    return;
  }
  g_id_stack_depth--;
}

const char *ui_scoped_id(const char *local_id) {
  char *out = g_scoped_id_buffers[g_scoped_id_buffer_index++ % UI_SCOPED_ID_BUFFER_COUNT];
  size_t used = 0;
  out[0] = '\0';
  for (int i = 0; i < g_id_stack_depth && used + 2 < 256; ++i) {
    int wrote = snprintf(out + used, 256 - used, "%s/", g_id_stack[i]);
    if (wrote < 0) break;
    used += (size_t)wrote;
  }
  snprintf(out + used, 256 - used, "%s", local_id ? local_id : "widget");
  return out;
}

typedef struct {
  char id[128];
  unsigned id_hash;
  bool is_hover;
  bool is_pressed;
  float hover_t;
  unsigned long long last_seen_frame;
} UIState;

static unsigned ui_hash_id(const char *id) {
  unsigned h = 2166136261u; /* FNV-1a */
  for (const unsigned char *p = (const unsigned char *)id; *p; ++p)
    h = (h ^ *p) * 16777619u;
  return h;
}

static UIState ui_states[MAX_UI_ELEMENTS];
static int ui_state_count = 0;

typedef struct {
  char id[64];
  UIRect rect;
} UIInputRegion;

static UIInputRegion g_input_regions[MAX_UI_INPUT_REGIONS];
static int g_input_region_count = 0;

static float g_mouse_x, g_mouse_y;
static bool g_mouse_down;
static bool g_mouse_down_prev = false;
static bool g_mouse_clicked = false; // true SOLO nel frame in cui il tasto
static bool g_mouse_released = false;
static bool g_escape_pressed = false;
static bool g_escape_was_down = false;
static bool g_escape_consumed = false;

#if 1 /* Generic menu support. */
// Vive in editor_panel_layout.c.

#endif


// Vero se il click di QUESTO frame è caduto dentro la lista APERTA di
// un dropdown — quando è vero, nessun altro widget (bottoni in
// particolare) deve reagire allo stesso click, anche se la sua area
// si sovrappone visivamente a quella lista. Calcolato una volta sola
// in ui_begin_frame (ui_precheck_dropdown_click_priority, definita più
// avanti nel file — dropdown_states non è ancora dichiarato qui,
// serve questa dichiarazione anticipata per poterla chiamare comunque
// da ui_begin_frame).
static bool g_click_claimed_by_dropdown_list = false;
static bool g_click_claimed_by_menu = false;
static void ui_precheck_dropdown_click_priority(void);
static void ui_reset_dropdown_popup_pass(void);
#if 1 /* Generic menu support. */
static void ui_menu_begin_frame(void);
static bool ui_menu_precheck_click(void);
#endif
static bool ui_any_modal_open(void);
static void ui_release_stale_slider_drags(void);
/* rettangolo del modale aperto (frame precedente), per il routing della rotellina */
typedef struct {
  char id[64];
  bool wasOpen;
  bool submittedThisFrame;
  bool scopeOpen; /* ui_modal_begin riuscito in questo frame, ui_modal_end non ancora chiamato */
} UIModalOpenState;

#define MAX_UI_MODALS 16
static UIModalOpenState g_modal_states[MAX_UI_MODALS];
static int g_modal_state_count = 0;
static bool g_modal_rect_valid = false;
static bool g_modal_rect_seen_this_frame = false;
static UIRect g_modal_rect;
/* MACROTASK UI residui (2026-09-17): un modale rende INERTI i widget sotto
 * di lui, in ui.c e non nei chiamanti. g_modal_scope_depth conta i
 * ui_modal_begin (riusciti) in corso nel frame; g_modal_open_count_prev e'
 * il numero di modali aperti nel frame precedente. Un widget riceve il
 * puntatore solo se sta nel modale piu' interno (depth >= aperti): quelli
 * disegnati fuori (prima o dopo il modale nel codice) non vedono ne' hover
 * ne' click, qualunque sia la loro posizione. */
static int g_modal_scope_depth = 0;
static int g_modal_open_count_prev = 0;
static bool point_in_rect(float px, float py, float x, float y, float w, float h);
static bool ui_pointer_hit(float x, float y, float w, float h) {
  if (g_modal_scope_depth < g_modal_open_count_prev)
    return false;
  return point_in_rect(g_mouse_x, g_mouse_y, x, y, w, h);
}
bool ui_pointer_blocked_by_modal(void) {
  return g_modal_scope_depth < g_modal_open_count_prev;
}
static void ui_modal_input_begin_frame(void);
static void ui_modal_input_end_frame(void);
static bool ui_file_picker_capture_active(void);
void ui_release_input_capture(void);
static void ui_close_all_dropdowns(void);
static bool point_in_rect(float px, float py, float x, float y, float w,
                          float h);
static void ui_ellipsize_right(Font *font, const char *text, float scale,
                               float max_width, char *out, size_t out_size);

                                     // passa da su a giù — calcolato UNA
                                     // volta qui, non ridondantemente in ogni
                                     // widget con una propria static locale
                                     // (quel pattern faceva sì che il primo
                                     // widget chiamato "consumasse" il fronte
                                     // di discesa condiviso, lasciando gli
                                     // altri sempre a secco nello stesso frame)
static float g_delta_time;
static bool g_any_hover = false;
static float g_scroll_delta = 0.0f;
static bool g_scroll_consumed = false;

static UITheme g_theme;
static bool g_theme_initialized = false;

#define MAX_UI_TOASTS 4
typedef struct {
  char message[96];
  UIToastKind kind;
  float remaining;
} UIToast;
static UIToast g_toasts[MAX_UI_TOASTS];
static int g_toast_count = 0;

// La swapchain usa VK_FORMAT_B8G8R8A8_SRGB: i colori del tema sono in
// spazio LINEARE e la GPU li ri-codifica in sRGB alla scrittura. La
// palette è definita nei valori sRGB del mockup ("Engine Editor") e
// convertita qui con la curva inversa (x^2.2), così l'esadecimale nel
// codice corrisponde a ciò che appare a schermo.
static UIColor ui_srgb_hex(uint32_t rgb) {
  float r = ((rgb >> 16) & 0xFF) / 255.0f;
  float g = ((rgb >> 8) & 0xFF) / 255.0f;
  float b = (rgb & 0xFF) / 255.0f;
  return (UIColor){powf(r, 2.2f), powf(g, 2.2f), powf(b, 2.2f)};
}

UIColor ui_color_srgb(uint32_t rgb) { return ui_srgb_hex(rgb); }

UITheme ui_theme_dark(void) {
  // Palette dal mockup "Engine Editor" (claude.ai/design): grigi caldi
  // scuri + VIOLA #ab34eb come colore di selezione/attivo (migrazione
  // tema: l'identita' visiva era arancio #f0842c). Ogni stato accent
  // (hover/focus/drag/selezione) deriva da QUESTO campo — mai valori
  // viola sparsi nei widget.
  UITheme t = {0};
  t.button_normal = ui_srgb_hex(0x20252b);
  t.button_hover = ui_srgb_hex(0x262d34);
  t.button_pressed = ui_srgb_hex(0x14181c);
  t.button_border = ui_srgb_hex(0x2f353d);
  t.text = ui_srgb_hex(0xc8cfd7);
  t.editor_background = ui_srgb_hex(0x101215);
  t.panel = ui_srgb_hex(0x15181c);
  t.panel_header = ui_srgb_hex(0x1b1f24);
  t.field = ui_srgb_hex(0x0f1215);
  t.field_hover = ui_srgb_hex(0x1d2126);
  t.selected = ui_srgb_hex(0x23122f);   /* purple-selection well */
  t.disabled = ui_srgb_hex(0x171b20);
  t.text_muted = ui_srgb_hex(0x8d97a1);
  t.accent = ui_srgb_hex(0xab34eb);
  t.warning = ui_srgb_hex(0xf2c94c);
  t.error = ui_srgb_hex(0xe0524f);
  t.success = ui_srgb_hex(0x52c07a);
  t.border = ui_srgb_hex(0x262b32);
  t.separator = ui_srgb_hex(0x22262b);
  t.danger = ui_srgb_hex(0xa33b39);
  /* Metriche mockup: righe più compatte del vecchio tema (24-26 px). */
  t.row_height = t.button_height = t.field_height = 26.0f;
  t.compact_button_height = 22.0f;
  t.panel_padding = 10.0f; t.row_spacing = 5.0f; t.section_spacing = 9.0f;
  // Scrollbar allargata da 9 a 14px (segnalazione utente: "troppo
  // piccola" nella lista APRI LIVELLO) -- unica fonte di verita' per
  // ogni tema (i chiari/warm sotto derivano scrollbar_width da qui).
  t.indent = 13.0f; t.label_value_gap = 8.0f; t.scrollbar_width = 14.0f;
  t.border_thickness = 1.0f; t.separator_thickness = 1.0f;
  t.popup_padding = 8.0f; t.modal_padding = 14.0f;
  t.menu_height = 28.0f; t.panel_header_height = 24.0f;
  t.status_height = 24.0f; t.slider_track_height = 5.0f;
  t.numeric_field_width = 78.0f;
  t.text_scale = 1.0f; t.secondary_text_scale = 0.85f;
  t.section_text_scale = 1.0f;
  return t;
}

// Tema CHIARO: sfondi chiari, testo scuro — utile con schermi molto
// luminosi o preferenza personale, stessa struttura del tema scuro
// (nessun altro codice deve sapere quale sia attivo, legge sempre
// ui_get_theme()).
UITheme ui_theme_light(void) {
  UITheme t = {0};
  t.button_normal = (UIColor){0.85f, 0.85f, 0.88f};
  t.button_hover = (UIColor){0.75f, 0.78f, 0.85f};
  t.button_pressed = (UIColor){0.65f, 0.68f, 0.75f};
  t.button_border = (UIColor){0.25f, 0.35f, 0.55f};
  t.text = (UIColor){0.08f, 0.08f, 0.1f};
  t.editor_background = (UIColor){0.72f, 0.74f, 0.78f};
  t.panel = (UIColor){0.82f, 0.84f, 0.88f};
  t.panel_header = (UIColor){0.70f, 0.74f, 0.82f};
  t.field = (UIColor){0.92f, 0.93f, 0.96f};
  t.field_hover = (UIColor){0.82f, 0.86f, 0.93f};
  t.selected = (UIColor){0.35f, 0.52f, 0.82f};
  t.disabled = (UIColor){0.68f, 0.69f, 0.72f};
  t.text_muted = (UIColor){0.28f, 0.30f, 0.36f};
  t.accent = t.button_border; t.warning = (UIColor){0.78f, 0.38f, 0.04f};
  t.error = (UIColor){0.70f, 0.10f, 0.12f}; t.success = (UIColor){0.10f, 0.52f, 0.24f};
  t.border = (UIColor){0.48f, 0.52f, 0.60f}; t.separator = (UIColor){0.62f, 0.65f, 0.72f};
  t.danger = (UIColor){0.62f, 0.12f, 0.14f};
  UITheme metrics = ui_theme_dark();
  t.row_height = metrics.row_height; t.button_height = metrics.button_height;
  t.compact_button_height = metrics.compact_button_height; t.field_height = metrics.field_height;
  t.panel_padding = metrics.panel_padding; t.row_spacing = metrics.row_spacing;
  t.section_spacing = metrics.section_spacing; t.indent = metrics.indent;
  t.label_value_gap = metrics.label_value_gap; t.scrollbar_width = metrics.scrollbar_width;
  t.border_thickness = metrics.border_thickness; t.separator_thickness = metrics.separator_thickness;
  t.popup_padding = metrics.popup_padding; t.modal_padding = metrics.modal_padding;
  t.menu_height = metrics.menu_height; t.panel_header_height = metrics.panel_header_height;
  t.status_height = metrics.status_height; t.slider_track_height = metrics.slider_track_height;
  t.numeric_field_width = metrics.numeric_field_width;
  t.text_scale = metrics.text_scale; t.secondary_text_scale = metrics.secondary_text_scale;
  t.section_text_scale = metrics.section_text_scale;
  return t;
}

// Tema CALDO: variante scura con accenti arancioni invece che blu —
// stessa leggibilità del tema scuro, solo diversa tinta.
UITheme ui_theme_warm(void) {
  UITheme t = {0};
  t.button_normal = (UIColor){0.28f, 0.20f, 0.16f};
  t.button_hover = (UIColor){0.42f, 0.30f, 0.20f};
  t.button_pressed = (UIColor){0.18f, 0.12f, 0.10f};
  t.button_border = (UIColor){0.85f, 0.55f, 0.25f};
  t.text = (UIColor){1.0f, 0.95f, 0.9f};
  t.editor_background = (UIColor){0.055f, 0.034f, 0.025f};
  t.panel = (UIColor){0.10f, 0.060f, 0.040f};
  t.panel_header = (UIColor){0.19f, 0.11f, 0.07f};
  t.field = (UIColor){0.14f, 0.08f, 0.05f};
  t.field_hover = (UIColor){0.24f, 0.14f, 0.08f};
  t.selected = (UIColor){0.62f, 0.24f, 0.05f};
  t.disabled = (UIColor){0.09f, 0.055f, 0.040f};
  t.text_muted = (UIColor){0.55f, 0.42f, 0.34f};
  t.accent = t.button_border; t.warning = (UIColor){0.88f, 0.50f, 0.08f};
  t.error = (UIColor){0.72f, 0.08f, 0.06f}; t.success = (UIColor){0.18f, 0.56f, 0.20f};
  t.border = (UIColor){0.30f, 0.17f, 0.10f}; t.separator = (UIColor){0.20f, 0.11f, 0.07f};
  t.danger = (UIColor){0.48f, 0.05f, 0.03f};
  UITheme metrics = ui_theme_dark();
  t.row_height = metrics.row_height; t.button_height = metrics.button_height;
  t.compact_button_height = metrics.compact_button_height; t.field_height = metrics.field_height;
  t.panel_padding = metrics.panel_padding; t.row_spacing = metrics.row_spacing;
  t.section_spacing = metrics.section_spacing; t.indent = metrics.indent;
  t.label_value_gap = metrics.label_value_gap; t.scrollbar_width = metrics.scrollbar_width;
  t.border_thickness = metrics.border_thickness; t.separator_thickness = metrics.separator_thickness;
  t.popup_padding = metrics.popup_padding; t.modal_padding = metrics.modal_padding;
  t.menu_height = metrics.menu_height; t.panel_header_height = metrics.panel_header_height;
  t.status_height = metrics.status_height; t.slider_track_height = metrics.slider_track_height;
  t.numeric_field_width = metrics.numeric_field_width;
  t.text_scale = metrics.text_scale; t.secondary_text_scale = metrics.secondary_text_scale;
  t.section_text_scale = metrics.section_text_scale;
  return t;
}

void ui_set_theme(UITheme theme) {
  g_theme = theme;
  g_theme_initialized = true;
}

UITheme ui_get_theme(void) {
  if (!g_theme_initialized) {
    g_theme = ui_theme_dark();
    g_theme_initialized = true;
  }
  return g_theme;
}

static UIFeedbackFn g_feedback;
static void *g_feedbackUser;
void ui_set_feedback(UIFeedbackFn fn, void *user) {
  g_feedback = fn;
  g_feedbackUser = user;
}
/* dal frame appena finito: su quale widget stava il puntatore (hash del suo
 * id, 0 = nessuno con stato) e se era su qualcosa di cliccabile */
static void ui_feedback_frame(bool mouse_down) {
  static unsigned lastHover;
  static bool lastAny, lastDown;
  unsigned hover = 0;
  for (int i = 0; i < ui_state_count; ++i)
    if (ui_states[i].is_hover) {
      hover = ui_states[i].id_hash;
      break;
    }
  const bool enter = (hover && hover != lastHover) || (g_any_hover && !lastAny);
  if (g_feedback && enter)
    g_feedback("focus", g_feedbackUser);
  /* la pressione si giudica col frame IN CUI e' avvenuta (lo hover di quel
   * frame e' gia' sulla posizione premuta): qui, al frame dopo */
  static bool pressedLast;
  if (g_feedback && pressedLast && g_any_hover)
    g_feedback("confirm", g_feedbackUser);
  pressedLast = mouse_down && !lastDown;
  lastHover = hover;
  lastAny = g_any_hover;
  lastDown = mouse_down;
}

void ui_begin_frame(APP *app, float mouse_x, float mouse_y, bool mouse_down,
                    float delta_time) {
  ui_feedback_frame(mouse_down);
  ++g_ui_frame_serial;
  if (g_ui_frame_serial == 0) {
    g_ui_frame_serial = 1;
    for (int i = 0; i < ui_state_count; ++i)
      ui_states[i].last_seen_frame = 0;
    for (int i = 0; i < g_section_state_count; ++i)
      g_section_states[i].last_seen_frame = 0;
  }
  g_scoped_id_buffer_index = 0;
  g_scroll_consumed = false;
  g_modal_rect_valid = g_modal_rect_seen_this_frame; /* modale del frame precedente */
  g_modal_rect_seen_this_frame = false;
  g_modal_open_count_prev = 0;
  for (int i = 0; i < g_modal_state_count; ++i)
    if (g_modal_states[i].wasOpen)
      g_modal_open_count_prev++;
  g_modal_scope_depth = 0;
  g_mouse_x = mouse_x;
  g_mouse_y = mouse_y;
  g_mouse_clicked = mouse_down && !g_mouse_down_prev;
  g_mouse_released = !mouse_down && g_mouse_down_prev;
  g_mouse_down = mouse_down;
  g_mouse_down_prev = mouse_down;
  bool escape_down = app && glfwGetKey(app->window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
  g_escape_pressed = escape_down && !g_escape_was_down;
  g_escape_was_down = escape_down;
  g_escape_consumed = false;
  if (!g_mouse_down) {
    for (int i = 0; i < g_scroll_state_count; ++i)
      g_scroll_states[i].dragging = false;
    /* Un widget SPARITO mentre era premuto/in drag (cambio pannello o
       modalita' durante la pressione) non viene piu' ridisegnato e quindi
       non azzererebbe mai il proprio stato: ui_has_pointer_capture()
       restava vero per sempre. Si azzerano SOLO i widget non visti nel
       frame precedente: quelli ancora vivi consumano il rilascio da soli
       (e' cosi' che nasce il click). */
    for (int i = 0; i < ui_state_count; ++i)
      if (ui_states[i].is_pressed && ui_states[i].last_seen_frame + 1 < g_ui_frame_serial)
        ui_states[i].is_pressed = false;
    ui_release_stale_slider_drags();
  }
  g_delta_time = delta_time;
  for (int i = 0; i < g_toast_count;) {
    g_toasts[i].remaining -= delta_time;
    if (g_toasts[i].remaining <= 0.0f) {
      memmove(&g_toasts[i], &g_toasts[i + 1],
              sizeof(g_toasts[0]) * (size_t)(g_toast_count - i - 1));
      --g_toast_count;
    } else {
      ++i;
    }
  }
  g_any_hover = false;
  for (int i = 0; i < ui_state_count; ++i)
    ui_states[i].is_hover = false;
  for (int i = 0; i < g_scroll_state_count; ++i) {
    g_scroll_states[i].wheel_consumed = false;
    g_scroll_states[i].thumb_hovered = false;
  }
  ui_reset_dropdown_popup_pass();
  ui_input_regions_begin();

  // Deve girare DOPO aver impostato g_mouse_x/y/clicked sopra (li
  // legge), PRIMA che qualunque bottone/dropdown di questo frame
  // inizi a processare il proprio click — così un bottone disegnato
  // PRIMA di un dropdown che lo sovrasta (nel codice, non solo a
  // schermo) sa già, quando tocca a lui, se questo click "appartiene"
  // alla lista aperta di quel dropdown invece che a lui.
  ui_precheck_dropdown_click_priority();
#if 1 /* Generic menu support. */
  ui_menu_begin_frame();
  g_click_claimed_by_menu = ui_menu_precheck_click();
#else
  g_click_claimed_by_menu = false;
#endif
  ui_modal_input_begin_frame();

#if 1 /* Generic menu support. */
  // Azzera il cursore di layout condiviso dei pannelli editor (Layer,
  // Path, Inspector box/luce/nemico) — vive in editor_panel_layout.c,
  // compilato SOLO in questa build (come tutto ciò che è
  // genuinamente editor). Deve girare una volta per frame, prima che
  // qualunque pannello chieda "dove comincio?" tramite
  // editor_panel_layout_next.

#endif
}

static bool ui_consume_escape(void) {
  if (!g_escape_pressed || g_escape_consumed)
    return false;
  g_escape_consumed = true;
  return true;
}

void ui_set_scroll_delta(float delta) {
  g_scroll_delta = isfinite(delta) ? delta : 0.0f;
  g_scroll_consumed = false;
}

float ui_scroll_delta(void) { return g_scroll_consumed ? 0.0f : g_scroll_delta; }
bool ui_consume_scroll_delta(void) {
  if (g_scroll_consumed || g_scroll_delta == 0.0f) return false;
  g_scroll_consumed = true;
  return true;
}

void ui_notify(UIToastKind kind, const char *message) {
  if (!message || !message[0])
    return;
  for (int i = 0; i < g_toast_count; ++i) {
    if (!strcmp(g_toasts[i].message, message)) {
      g_toasts[i].kind = kind;
      g_toasts[i].remaining = 2.8f;
      return;
    }
  }
  if (g_toast_count == MAX_UI_TOASTS) {
    memmove(&g_toasts[0], &g_toasts[1],
            sizeof(g_toasts[0]) * (MAX_UI_TOASTS - 1));
    --g_toast_count;
  }
  UIToast *toast = &g_toasts[g_toast_count++];
  snprintf(toast->message, sizeof(toast->message), "%s", message);
  toast->kind = kind;
  toast->remaining = 2.8f;
}

static float g_toast_top = -1.0f;
void ui_set_notification_top(float y) { g_toast_top = y; }

void ui_draw_notifications(APP *app, Font *font) {
  if (!app || !font || g_toast_count == 0)
    return;
  UITheme theme = ui_get_theme();
  float width = 300.0f;
  float height = theme.menu_height;
  float x = VIRTUAL_W - width - theme.panel_padding;
  /* Sotto il chrome che il chiamante dichiara (editor: topbar + subbar),
     mai sopra i tab del workspace. */
  float y = g_toast_top >= 0.0f ? g_toast_top : theme.menu_height + theme.panel_padding;
  for (int i = g_toast_count - 1; i >= 0; --i) {
    UIToast *toast = &g_toasts[i];
    UIColor semantic = theme.accent;
    if (toast->kind == UI_TOAST_SUCCESS) semantic = theme.success;
    else if (toast->kind == UI_TOAST_WARNING) semantic = theme.warning;
    else if (toast->kind == UI_TOAST_ERROR) semantic = theme.error;
    draw_quad(app, x - theme.border_thickness, y - theme.border_thickness,
              width + theme.border_thickness * 2.0f,
              height + theme.border_thickness * 2.0f, theme.border.r,
              theme.border.g, theme.border.b);
    draw_quad(app, x, y, width, height, theme.panel_header.r,
              theme.panel_header.g, theme.panel_header.b);
    draw_quad(app, x, y, 4.0f, height, semantic.r, semantic.g, semantic.b);
    char display[96];
    ui_ellipsize_right(font, toast->message, 13.0f,
                       width - theme.panel_padding * 2.0f, display,
                       sizeof(display));
    draw_text_colored(app, font, display, x + theme.panel_padding,
                      y + (height + font->ascender * 13.0f) * 0.5f, 13.0f,
                      theme.text.r, theme.text.g, theme.text.b);
    y += height + theme.row_spacing;
  }
}

Vec2 ui_mouse_position(void) { return (Vec2){g_mouse_x, g_mouse_y}; }
unsigned long long ui_frame_serial_hint(void) { return g_ui_frame_serial; }
bool ui_mouse_down(void) { return g_mouse_down; }

void ui_end_frame(void) {
  ui_modal_input_end_frame();
#ifdef DEBUG
  if (g_id_stack_depth != 0)
    LOG_WARN("UI ID stack imbalance: %d scope(s) left open", g_id_stack_depth);
#endif
  g_id_stack_depth = 0;
}

void ui_input_regions_begin(void) { g_input_region_count = 0; }

bool ui_input_region_register(const char *id, UIRect rect) {
  if (!id || !id[0] || !isfinite(rect.x) || !isfinite(rect.y) ||
      !isfinite(rect.w) || !isfinite(rect.h) || rect.w <= 0.0f ||
      rect.h <= 0.0f ||
      g_input_region_count >= MAX_UI_INPUT_REGIONS)
    return false;

  UIInputRegion *region = &g_input_regions[g_input_region_count++];
  strncpy(region->id, id, sizeof(region->id) - 1);
  region->id[sizeof(region->id) - 1] = '\0';
  region->rect = rect;
  return true;
}

bool ui_input_region_contains(float x, float y) {
  /* Last registration wins. This keeps an eventual overlay region above its
   * parent without giving callers another priority mechanism. */
  for (int i = g_input_region_count - 1; i >= 0; --i) {
    UIRect r = g_input_regions[i].rect;
    if (point_in_rect(x, y, r.x, r.y, r.w, r.h))
      return true;
  }
  return false;
}

bool ui_clip_push(UIRect rect) {
  if (rect.w <= 0.0f || rect.h <= 0.0f)
    return false;
  return text_clip_push(rect.x, rect.y, rect.w, rect.h);
}

bool ui_clip_pop(void) {
  return text_clip_pop();
}

int ui_clip_depth(void) { return text_clip_depth(); }

int ui_input_region_count(void) { return g_input_region_count; }

const UIRect *ui_input_region_at(int index, const char **out_id) {
  if (index < 0 || index >= g_input_region_count)
    return NULL;
  if (out_id)
    *out_id = g_input_regions[index].id;
  return &g_input_regions[index].rect;
}

bool ui_any_hover(void) { return g_any_hover; }

/* Implemented after the overlay-state registries below. */
bool ui_has_input_capture(void);

static UIState *find_or_create_state(const char *id) {
  unsigned hash = ui_hash_id(id);
  for (int i = 0; i < ui_state_count; i++) {
    if (ui_states[i].id_hash == hash && strcmp(ui_states[i].id, id) == 0) {
      ui_states[i].last_seen_frame = g_ui_frame_serial;
      return &ui_states[i];
    }
  }
  if (ui_state_count < MAX_UI_ELEMENTS) {
    UIState *s = &ui_states[ui_state_count++];
    strncpy(s->id, id, sizeof(s->id) - 1);
    s->id[sizeof(s->id) - 1] = '\0';
    s->id_hash = hash;
    s->is_hover = false;
    s->is_pressed = false;
    s->hover_t = 0.0f;
    s->last_seen_frame = g_ui_frame_serial;
    return s;
  }
  /* The registry is a bounded cache, not an append-only lifetime list.
   * Screens and Inspector tabs legitimately submit many different semantic
   * IDs over a session. Reuse the least-recent inactive entry, while never
   * evicting a widget pressed in-flight or one already emitted this frame. */
  UIState *reuse = NULL;
  for (int i = 0; i < ui_state_count; ++i) {
    UIState *candidate = &ui_states[i];
    if (candidate->is_pressed ||
        candidate->last_seen_frame == g_ui_frame_serial)
      continue;
    if (!reuse || candidate->last_seen_frame < reuse->last_seen_frame)
      reuse = candidate;
  }
  if (reuse) {
    memset(reuse, 0, sizeof(*reuse));
    snprintf(reuse->id, sizeof(reuse->id), "%s", id);
    reuse->id_hash = hash;
    reuse->last_seen_frame = g_ui_frame_serial;
    return reuse;
  }

  // More than MAX_UI_ELEMENTS live in one frame: fail visibly and safely.
  static bool warned = false;
  if (!warned) {
    LOG_WARN("UI: troppi widget simultanei (max %d) — id '%s' ignorato",
             MAX_UI_ELEMENTS, id);
    warned = true;
  }
  return NULL;
}

static bool point_in_rect(float px, float py, float x, float y, float w,
                          float h) {
  return px >= x && px <= x + w && py >= y && py <= y + h;
}

static float lerp(float a, float b, float t) { return a + (b - a) * t; }

// Se 'in' è TUTTA maiuscola (nessuna lettera minuscola dentro), la
// trasforma in Title Case (prima lettera di ogni parola maiuscola,
// resto minuscolo) — es. "SALVA COME..." diventa "Salva Come...". Se
// contiene anche una sola lettera minuscola, la copia INTATTA: un nome
// file scelto dall'utente ("goccia_sheet.png") o qualunque testo già
// scritto a mano non deve mai essere toccato, solo le etichette
// generate dal codice che erano SEMPRE tutte maiuscole per scelta di
// stile, mai per contenuto vero dell'utente.
void ui_humanize_label(const char *in, char *out, size_t outSize) {
  if (!in || outSize == 0)
    return;
  // Riorganizzazione UI 2026-09-17: le unita' fra parentesi ("(px)",
  // "(gradi)", "(0 = sfera)") non contano come "testo scritto a mano":
  // prima bastava una minuscola in parentesi per lasciare TUTTA l'etichetta
  // maiuscola, e i pannelli mostravano "MARGINE SX (px)" accanto a
  // "Ripetizioni X". La decisione guarda solo le lettere FUORI parentesi;
  // il contenuto fra parentesi viene copiato intatto.
  bool hasLower = false;
  int depth = 0;
  for (const char *p = in; *p; p++) {
    if (*p == '(') depth++;
    else if (*p == ')' && depth > 0) depth--;
    else if (depth == 0 && *p >= 'a' && *p <= 'z') {
      hasLower = true;
      break;
    }
  }
  if (hasLower) {
    strncpy(out, in, outSize - 1);
    out[outSize - 1] = '\0';
    return;
  }
  size_t i = 0;
  bool startOfWord = true;
  depth = 0;
  for (const char *p = in; *p && i < outSize - 1; p++, i++) {
    char c = *p;
    if (c == '(') depth++;
    if (depth > 0) {
      out[i] = c; // parentesi: intatto
      if (c == ')') depth--;
      startOfWord = true;
      continue;
    }
    if (c >= 'A' && c <= 'Z') {
      out[i] = startOfWord ? c : (char)(c - 'A' + 'a');
      startOfWord = false;
    } else {
      out[i] = c;
      startOfWord = (c == ' ' || c == '(' || c == '/' || c == '-' ||
                    c == '_' || c == ':' || c == '\'');
    }
  }
  out[i] = '\0';
}

static void ui_ellipsize_left(Font *font, const char *text, float scale,
                              float max_width, char *out, size_t out_size) {
  if (!out || out_size == 0)
    return;
  if (text != out)
    snprintf(out, out_size, "%s", text ? text : "");
  if (!font || max_width <= 0.0f ||
      measure_text_width(font, out, scale) <= max_width)
    return;
  const char *tail = text ? text : "";
  while (*tail && measure_text_width(font, tail, scale) > max_width -
                     measure_text_width(font, "...", scale))
    ++tail;
  snprintf(out, out_size, "...%s", tail);
}

static void ui_ellipsize_right(Font *font, const char *text, float scale,
                               float max_width, char *out, size_t out_size) {
  if (!out || out_size == 0)
    return;
  if (text != out)
    snprintf(out, out_size, "%s", text ? text : "");
  if (!font || max_width <= 0.0f ||
      measure_text_width(font, out, scale) <= max_width)
    return;
  size_t len = strlen(out);
  while (len > 0 && measure_text_width(font, out, scale) > max_width) {
    out[--len] = '\0';
  }
  if (len >= 3) {
    out[len - 3] = '.';
    out[len - 2] = '.';
    out[len - 1] = '.';
  }
}

float ui_button_height(Font *font, float scale, const UIButtonStyle *style) {
  UITheme theme = ui_get_theme();
  float padding_y = (style && style->padding_y > 0) ? style->padding_y : theme.row_spacing - 1.0f;
  float min_height = style ? style->min_height : 0.0f;
  float ascent = font->ascender * scale;

  float h = ascent + padding_y * 2.0f;
  if (h < min_height)
    h = min_height;
  return h;
}

bool ui_button_labeled(APP *app, const char *id, const char *label,
                       Font *font, TextAnchor anchor, float margin, float scale,
                       float offset_x, float offset_y,
                       const UIButtonStyle *style) {
  UIState *state = find_or_create_state(id);
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

  float text_w = measure_text_width(font, displayLabel, scale);
  float ascent = font->ascender * scale;

  float w = text_w + padding_x * 2.0f;
  if (w < min_width)
    w = min_width;
  if (max_width > 0.0f && w > max_width) {
    w = max_width;
    ui_ellipsize_right(font, displayLabel, scale,
                       fmaxf(0.0f, w - padding_x * 2.0f), displayLabel,
                       sizeof(displayLabel));
    text_w = measure_text_width(font, displayLabel, scale);
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
    x = VIRTUAL_W - margin - w;
    y = margin;
    break;
  case ANCHOR_BOTTOM_LEFT:
    x = margin;
    y = VIRTUAL_H - margin - h;
    break;
  case ANCHOR_BOTTOM_RIGHT:
    x = VIRTUAL_W - margin - w;
    y = VIRTUAL_H - margin - h;
    break;
  case ANCHOR_CENTER:
    x = (VIRTUAL_W - w) * 0.5f;
    y = (VIRTUAL_H - h) * 0.5f;
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
  if (x + w > VIRTUAL_W - 10.0f)
    x = VIRTUAL_W - 10.0f - w;
  if (x < 10.0f)
    x = 10.0f;

  state->is_hover = (!style || !style->disabled) &&
                    ui_pointer_hit(x, y, w, h);
  if (state->is_hover)
    g_any_hover = true;

  bool clicked = false;
  if (state->is_hover && g_mouse_down && !g_click_claimed_by_dropdown_list &&
      !g_click_claimed_by_menu) {
    state->is_pressed = true;
  }
  if (state->is_pressed && !g_mouse_down) {
    clicked = state->is_hover;
    state->is_pressed = false;
  }
  if (!g_mouse_down) {
    state->is_pressed = false;
  }

  float target = state->is_hover ? 1.0f : 0.0f;
  float speed = 8.0f;
  if (state->hover_t < target) {
    state->hover_t += speed * g_delta_time;
    if (state->hover_t > target)
      state->hover_t = target;
  } else if (state->hover_t > target) {
    state->hover_t -= speed * g_delta_time;
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
    draw_hud_trash_swipe(app, x, y, w, h, state->is_hover || (style && style->selected), ui_hash_id(id),
                         -1);
    draw_hud_paper_panel(app,x,y,w,h,fill,accent,
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

    draw_quad(app, x - border, y - border, w + border * 2, h + border * 2,
              border_r, border_g, border_b);
  }
  if (!theme.paper_panels) draw_quad(app, x, y, w, h, r, g, b);

  float text_x = (style && style->align_left)
                     ? x + padding_x
                     : x + (w - text_w) * 0.5f;
  float text_y = y + (h + ascent) * 0.5f;
  draw_text_colored(app, font, displayLabel, text_x, text_y, scale,
                    text_color.r,
                    text_color.g, text_color.b);

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
bool ui_checkbox(APP *app, const char *id, const char *label, Font *font,
                 float x, float y, float scale, bool *value) {
  UIState *state = find_or_create_state(id);
  if (!state || !value)
    return false;

  char displayLabel[128];
  ui_humanize_label(label, displayLabel, sizeof(displayLabel));

  float ascent = font->ascender * scale;
  float boxSize = ascent; // stessa altezza del testo, per allinearsi bene
                          // sulla riga invece di un quadratino fisso che
                          // può risultare troppo piccolo/grande rispetto
                          // all'etichetta a scale diverse
  float gap = 8.0f;
  float text_w = measure_text_width(font, displayLabel, scale);
  float totalW = boxSize + gap + text_w;
  float totalH = boxSize;

  // Area cliccabile: quadratino + etichetta insieme, non solo il
  // quadratino — più facile da colpire col mouse, stesso spirito
  // "generoso" degli altri widget di questo file.
  state->is_hover = ui_pointer_hit(x, y, totalW, totalH);
  if (state->is_hover)
    g_any_hover = true;

  bool clicked = false;
  if (state->is_hover && g_mouse_down && !g_click_claimed_by_dropdown_list &&
      !g_click_claimed_by_menu) {
    state->is_pressed = true;
  }
  if (state->is_pressed && !g_mouse_down) {
    clicked = state->is_hover;
    state->is_pressed = false;
  }
  if (!g_mouse_down) {
    state->is_pressed = false;
  }

  if (clicked)
    *value = !*value;

  float target = state->is_hover ? 1.0f : 0.0f;
  float speed = 8.0f;
  if (state->hover_t < target) {
    state->hover_t += speed * g_delta_time;
    if (state->hover_t > target)
      state->hover_t = target;
  } else if (state->hover_t > target) {
    state->hover_t -= speed * g_delta_time;
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
    draw_hud_paper_panel(app,x,y,boxSize,boxSize,fill,accent,state->is_hover,ui_hash_id(id));
  } else {
    draw_quad(app, x - border, y - border, boxSize + border * 2.0f,
             boxSize + border * 2.0f, bcolor.r, bcolor.g, bcolor.b);
    draw_quad(app, x, y, boxSize, boxSize, r, g, b);
  }

  // Spunta interna, solo se true — un quadratino più piccolo centrato,
  // nel colore del bordo/accent così resta leggibile su qualunque stato
  // hover/pressed dello sfondo.
  if (*value) {
    float pad = boxSize * 0.22f;
    draw_quad(app, x + pad, y + pad, boxSize - pad * 2.0f,
             boxSize - pad * 2.0f, bcolor.r, bcolor.g, bcolor.b);
  }

  float text_x = x + boxSize + gap;
  float text_y = y + (totalH + ascent) * 0.5f;
  draw_text_colored(app, font, displayLabel, text_x, text_y, scale,
                    text_color.r, text_color.g, text_color.b);

  return clicked;
}

bool ui_toggle(APP *app, const char *id, const char *label, Font *font,
               float x, float y, float scale, float width, bool *value) {
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
  bool changed = ui_button_labeled(app, id, shown, font, ANCHOR_TOP_LEFT, 0,
                                   14.0f * scale, x, y, &style);
  if (changed)
    *value = !*value;
  return changed;
}

bool ui_button_styled(APP *app, const char *id, Font *font, TextAnchor anchor,
                      float margin, float scale, float offset_x, float offset_y,
                      const UIButtonStyle *style) {
  return ui_button_labeled(app, id, id, font, anchor, margin, scale, offset_x,
                           offset_y, style);
}

// Stato "espanso/compresso" di un pannello, per-ID — stesso identico
// meccanismo di UIState sopra (find_or_create per stringa), ma un
// registro SEPARATO: l'ID di un pannello e quello del bottone che lo
// comprime possono coincidere (è voluto, vedi ui_collapsible_header)
// senza che i due stati si confondano tra loro. Indipendente dalla
// scena/box selezionato — un pannello che comprimi resta compresso
// finché non lo riapri tu, qualunque cosa selezioni nel frattempo.
//
// Protetto da EDITOR_ENABLED — a differenza del resto di questo file
// (ui_button_labeled/ui_dropdown li usa anche home.c per i bottoni del
// menu, fuori dall'editor), "pannello comprimibile" è un concetto
// esclusivo dell'editor: in una build senza EDITOR_ENABLED non deve
// restare compilato dentro per niente.
#if 1 /* Generic menu support. */
#define MAX_UI_PANELS 64
typedef struct {
  char id[64];
  bool collapsed;
} UIPanelState;

static UIPanelState panel_states[MAX_UI_PANELS];
static int panel_state_count = 0;

static UIPanelState *find_or_create_panel_state(const char *id) {
  for (int i = 0; i < panel_state_count; i++) {
    if (strcmp(panel_states[i].id, id) == 0)
      return &panel_states[i];
  }
  if (panel_state_count < MAX_UI_PANELS) {
    UIPanelState *s = &panel_states[panel_state_count++];
    strncpy(s->id, id, sizeof(s->id) - 1);
    s->id[sizeof(s->id) - 1] = '\0';
    s->collapsed = false; // default: sempre espanso, comportamento di
                          // sempre finché non lo comprimi tu esplicitamente
    return s;
  }
  static bool warned = false;
  if (!warned) {
    LOG_ERROR("ui_collapsible_header: pool esaurito (max %d) — pannelli "
              "oltre questo numero non si comprimeranno più, id ignorato: "
              "'%s'",
              MAX_UI_PANELS, id);
    warned = true;
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
bool ui_collapsible_header(APP *app, const char *id, const char *label,
                           Font *font, float x, float y, float width,
                           float scale) {
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

  if (ui_button_labeled(app, id, fullLabel, font, ANCHOR_TOP_LEFT, 0, scale,
                        x, y, &style)) {
    if (state)
      state->collapsed = !state->collapsed;
  }

  return expanded;
}

#define MAX_UI_MENU_ITEMS 32 /* editor.md A5: menu contestuale unico + SELEZIONE nei menu */
#define MAX_UI_SUBMENU_SUBMISSIONS 16

typedef struct {
  char id[64], label[64], submenu_id[64], shortcut[32];
  UIMenuItemKind kind;
  bool disabled, checked;
} UIStoredMenuItem;

typedef struct {
  bool active, context_root, opens_left;
  char id[64], parent_id[64], parent_item_id[64];
  int depth, item_count;
  float width;
  UIRect anchor, popup;
  UIStoredMenuItem items[MAX_UI_MENU_ITEMS];
} UIMenuLevel;

typedef struct {
  char parent_id[64], parent_item_id[64], submenu_id[64];
  float width;
  int item_count;
  UIStoredMenuItem items[MAX_UI_MENU_ITEMS];
} UIMenuSubmission;

typedef struct {
  UIMenuLevel levels[UI_MENU_MAX_DEPTH];
  UIMenuSubmission submissions[MAX_UI_SUBMENU_SUBMISSIONS];
  int depth, submission_count, hovered_depth, owning_depth;
  bool overflowed, escape_consumed, root_opened_this_frame;
  bool root_submitted_this_frame;
  bool result_pending;
  char result_root_id[64], result_item_id[64];
} UIMenuHierarchy;

static UIMenuHierarchy g_menu = {.hovered_depth = -1, .owning_depth = -1};

static void ui_menu_copy_items(UIStoredMenuItem *out, int *out_count,
                               const UIMenuItem *items, int item_count) {
  int count = item_count;
  if (count < 0) count = 0;
  if (count > MAX_UI_MENU_ITEMS) {
    count = MAX_UI_MENU_ITEMS;
    g_menu.overflowed = true;
#ifdef DEBUG
    LOG_WARN("Menu item capacity exceeded");
#endif
  }
  for (int i = 0; i < count; ++i) {
    snprintf(out[i].id, sizeof(out[i].id), "%s",
             items[i].id ? items[i].id : "item");
    snprintf(out[i].label, sizeof(out[i].label), "%s",
             items[i].label ? items[i].label : "");
    snprintf(out[i].submenu_id, sizeof(out[i].submenu_id), "%s",
             items[i].submenu_id ? items[i].submenu_id : "");
    snprintf(out[i].shortcut, sizeof(out[i].shortcut), "%s",
             items[i].shortcut ? items[i].shortcut : "");
    out[i].kind = items[i].kind;
    out[i].disabled = items[i].disabled;
    out[i].checked = items[i].checked;
  }
  *out_count = count;
}

static void ui_menu_close_from(int depth) {
  if (depth < 0) depth = 0;
  if (depth > g_menu.depth) {
#ifdef DEBUG
    LOG_WARN("Menu hierarchy close underflow: %d > %d", depth, g_menu.depth);
#endif
    return;
  }
  for (int i = depth; i < UI_MENU_MAX_DEPTH; ++i)
    memset(&g_menu.levels[i], 0, sizeof(g_menu.levels[i]));
  g_menu.depth = depth;
}

void ui_menu_close_hierarchy(void) { ui_menu_close_from(0); }
bool ui_menu_hierarchy_is_open(void) { return g_menu.depth > 0; }

static void ui_menu_open_root(const char *id, UIRect anchor, float width,
                              bool context_root, const UIMenuItem *items,
                              int item_count) {
  ui_close_all_dropdowns();
  ui_menu_close_hierarchy();
  UIMenuLevel *root = &g_menu.levels[0];
  root->active = true;
  root->context_root = context_root;
  root->depth = 0;
  root->anchor = anchor;
  root->width = width > 0.0f ? width : 220.0f;
  snprintf(root->id, sizeof(root->id), "%s", id ? id : "menu");
  ui_menu_copy_items(root->items, &root->item_count, items, item_count);
  g_menu.depth = 1;
  g_menu.root_opened_this_frame = true;
  g_menu.root_submitted_this_frame = true;
}

static void ui_menu_begin_frame(void) {
  g_menu.hovered_depth = -1;
  g_menu.owning_depth = -1;
  g_menu.escape_consumed = false;
  g_menu.root_opened_this_frame = false;
  g_menu.root_submitted_this_frame = false;
}

static bool ui_menu_precheck_click(void) {
  return g_mouse_clicked && g_menu.depth > 0;
}

bool ui_menu_submit_submenu(const char *parent_popup_id,
                            const char *parent_item_id,
                            const char *submenu_id,
                            const UIMenuItem *items, int item_count) {
  if (!parent_popup_id || !parent_item_id || !submenu_id || !items)
    return false;
  UIMenuSubmission *submission = NULL;
  for (int i = 0; i < g_menu.submission_count; ++i) {
    if (!strcmp(g_menu.submissions[i].parent_id, parent_popup_id) &&
        !strcmp(g_menu.submissions[i].parent_item_id, parent_item_id) &&
        !strcmp(g_menu.submissions[i].submenu_id, submenu_id)) {
      submission = &g_menu.submissions[i];
      break;
    }
  }
  if (!submission && g_menu.submission_count >= MAX_UI_SUBMENU_SUBMISSIONS) {
    g_menu.overflowed = true;
#ifdef DEBUG
    LOG_WARN("Menu submenu submission capacity exceeded");
#endif
    return false;
  }
  if (!submission)
    submission = &g_menu.submissions[g_menu.submission_count++];
  memset(submission, 0, sizeof(*submission));
  snprintf(submission->parent_id, sizeof(submission->parent_id), "%s",
           parent_popup_id);
  snprintf(submission->parent_item_id, sizeof(submission->parent_item_id),
           "%s", parent_item_id);
  snprintf(submission->submenu_id, sizeof(submission->submenu_id), "%s",
           submenu_id);
  submission->width = 220.0f;
  ui_menu_copy_items(submission->items, &submission->item_count, items,
                     item_count);
  return true;
}

static UIMenuSubmission *ui_menu_find_submission(const char *parent_id,
                                                 const char *item_id,
                                                 const char *submenu_id) {
  for (int i = 0; i < g_menu.submission_count; ++i) {
    UIMenuSubmission *submission = &g_menu.submissions[i];
    if (!strcmp(submission->parent_id, parent_id) &&
        !strcmp(submission->parent_item_id, item_id) &&
        !strcmp(submission->submenu_id, submenu_id))
      return submission;
  }
  return NULL;
}

void ui_menu_open_context(const char *id, float x, float y, float width,
                          const UIMenuItem *items, int item_count) {
  ui_menu_open_root(id, (UIRect){x, y, 0.0f, 0.0f}, width, true, items,
                    item_count);
}

const char *ui_menu_take_action_id(const char *root_id) {
  if (!root_id || !g_menu.result_pending ||
      strcmp(g_menu.result_root_id, root_id) != 0)
    return NULL;
  g_menu.result_pending = false;
  return g_menu.result_item_id;
}

const char *ui_menu_bar(APP *app, const char *id, const char *label,
                        Font *font, float x, float y, float width, float scale,
                        const UIMenuItem *items, int item_count,
                        const UIButtonStyle *style) {
  if (!id || !label || !items || item_count <= 0)
    return NULL;
  const char *result = ui_menu_take_action_id(id);
  UIButtonStyle trigger = style ? *style : (UIButtonStyle){0};
  UITheme theme = ui_get_theme();
  float natural_width = measure_text_width(font, label, scale) +
                        theme.popup_padding * 2.0f;
  float trigger_width = natural_width;
  if (trigger.min_width > trigger_width)
    trigger_width = trigger.min_width;
  trigger.min_width = trigger_width;
  trigger.max_width = trigger_width;
  trigger.min_height = theme.menu_height;
  bool root_active = g_menu.depth > 0 && !strcmp(g_menu.levels[0].id, id);
  if (root_active)
    g_menu.root_submitted_this_frame = true;
  if (root_active) {
    trigger.has_custom_color = true;
    trigger.color_normal = theme.selected;
    trigger.color_hover = theme.selected;
    trigger.color_pressed = theme.field;
    trigger.color_border = theme.accent;
  }
  float trigger_height = ui_button_height(font, scale, &trigger);
  bool clicked_open_trigger =
      g_mouse_clicked && g_menu.depth > 0 &&
      ui_pointer_hit(x, y, trigger_width, trigger_height);
  if (ui_button_labeled(app, id, label, font, ANCHOR_TOP_LEFT, 0, scale,
                        x, y, &trigger) || clicked_open_trigger) {
    if (g_menu.depth > 0 && !strcmp(g_menu.levels[0].id, id))
      ui_menu_close_hierarchy();
    else
      ui_menu_open_root(id, (UIRect){x, y, trigger_width, trigger_height}, width,
                        false, items, item_count);
  } else if (g_menu.depth > 0 && !strcmp(g_menu.levels[0].id, id)) {
    UIMenuLevel *root = &g_menu.levels[0];
    root->anchor = (UIRect){x, y, trigger_width, trigger_height};
    root->width = width;
    ui_menu_copy_items(root->items, &root->item_count, items, item_count);
  }
  return result;
}

static float ui_menu_row_height(const UIStoredMenuItem *item, Font *font) {
  (void)font;
  float normal = ui_get_theme().menu_height;
  return item->kind == UI_MENU_ITEM_SEPARATOR ? fmaxf(7.0f, normal * 0.25f)
                                               : normal;
}

static void ui_menu_place_level(UIMenuLevel *level, Font *font) {
  float height = 0.0f;
  UITheme theme = ui_get_theme();
  float text_scale = 14.0f * theme.text_scale;
  for (int i = 0; i < level->item_count; ++i) {
    const UIStoredMenuItem *item = &level->items[i];
    height += ui_menu_row_height(item, font);
    /* Riorganizzazione UI 2026-09-17: la larghezza richiesta dal chiamante
     * e' un minimo; l'etichetta piu' lunga con la sua scorciatoia deve
     * starci per intero (prima "VISTA FRONTALE SUL LIVELLO" e "Home" si
     * sovrapponevano nel menu VISTA). */
    if (item->kind == UI_MENU_ITEM_SEPARATOR) continue;
    float natural = theme.popup_padding * 2.0f + 18.0f +
                    measure_text_width(font, item->label, text_scale) + 24.0f;
    if (item->shortcut[0])
      natural += measure_text_width(font, item->shortcut,
                                    text_scale * theme.secondary_text_scale);
    if (item->kind == UI_MENU_ITEM_SUBMENU) natural += 18.0f;
    if (natural > level->width) level->width = natural;
  }
  float x, y;
  if (level->depth == 0) {
    x = level->anchor.x;
    y = level->context_root ? level->anchor.y
                            : level->anchor.y + level->anchor.h;
    level->opens_left = false;
  } else {
    float right = level->anchor.x + level->anchor.w;
    bool flip_left = right + level->width > VIRTUAL_W - 10.0f;
    x = flip_left ? level->anchor.x - level->width : right;
    y = level->anchor.y;
    level->opens_left = flip_left;
  }
  if (x + level->width > VIRTUAL_W - 10.0f)
    x = VIRTUAL_W - 10.0f - level->width;
  if (x < 10.0f) x = 10.0f;
  if (y + height > VIRTUAL_H - 10.0f) y = VIRTUAL_H - 10.0f - height;
  if (y < 10.0f) y = 10.0f;
  level->popup = (UIRect){x, y, level->width, height};
}

static UIRect ui_menu_item_rect(const UIMenuLevel *level, Font *font,
                                int item_index) {
  float y = level->popup.y;
  for (int i = 0; i < item_index; ++i)
    y += ui_menu_row_height(&level->items[i], font);
  return (UIRect){level->popup.x, y, level->popup.w,
                  ui_menu_row_height(&level->items[item_index], font)};
}

static void ui_menu_open_child(int parent_depth, int item_index, Font *font) {
  UIMenuLevel *parent = &g_menu.levels[parent_depth];
  UIStoredMenuItem *item = &parent->items[item_index];
  UIMenuSubmission *submission = ui_menu_find_submission(
      parent->id, item->id, item->submenu_id);
  if (!submission || item->disabled)
    return;
  int child_depth = parent_depth + 1;
  if (child_depth >= UI_MENU_MAX_DEPTH) {
    g_menu.overflowed = true;
#ifdef DEBUG
    LOG_WARN("Menu hierarchy depth exceeded: %d", UI_MENU_MAX_DEPTH);
#endif
    return;
  }
  if (g_menu.depth > child_depth &&
      !strcmp(g_menu.levels[child_depth].id, submission->submenu_id) &&
      !strcmp(g_menu.levels[child_depth].parent_id, parent->id) &&
      !strcmp(g_menu.levels[child_depth].parent_item_id, item->id)) {
    UIMenuLevel *child = &g_menu.levels[child_depth];
    child->anchor = ui_menu_item_rect(parent, font, item_index);
    child->width = submission->width;
    memcpy(child->items, submission->items, sizeof(child->items));
    child->item_count = submission->item_count;
    return;
  }
  char parent_id[64], parent_item_id[64];
  snprintf(parent_id, sizeof(parent_id), "%s", parent->id);
  snprintf(parent_item_id, sizeof(parent_item_id), "%s", item->id);
  ui_menu_close_from(child_depth);
  UIMenuLevel *child = &g_menu.levels[child_depth];
  child->active = true;
  child->depth = child_depth;
  child->width = submission->width;
  child->anchor = ui_menu_item_rect(parent, font, item_index);
  snprintf(child->id, sizeof(child->id), "%s", submission->submenu_id);
  snprintf(child->parent_id, sizeof(child->parent_id), "%s", parent_id);
  snprintf(child->parent_item_id, sizeof(child->parent_item_id), "%s",
           parent_item_id);
  memcpy(child->items, submission->items, sizeof(child->items));
  child->item_count = submission->item_count;
  g_menu.depth = child_depth + 1;
}

void ui_menu_render_overlay(APP *app, Font *font) {
  if (g_menu.depth <= 0)
    return;
  if (!g_menu.levels[0].context_root &&
      !g_menu.root_submitted_this_frame) {
    ui_menu_close_hierarchy();
    return;
  }
  if (ui_any_modal_open()) {
    ui_menu_close_hierarchy();
    return;
  }
  if (ui_consume_escape()) {
    ui_menu_close_from(g_menu.depth - 1);
    g_menu.escape_consumed = true;
    return;
  }

  UITheme theme = ui_get_theme();
  g_menu.hovered_depth = -1;
  g_menu.owning_depth = -1;
  for (int depth = 0; depth < g_menu.depth; ++depth) {
    UIMenuLevel *level = &g_menu.levels[depth];
    ui_menu_place_level(level, font);
    bool inside = ui_pointer_hit(level->popup.x,
                                level->popup.y, level->popup.w,
                                level->popup.h);
    if (inside) {
      g_any_hover = true;
      g_menu.hovered_depth = depth;
    }
    float popup_border = theme.border_thickness;
    draw_quad(app, level->popup.x - popup_border,
              level->popup.y - popup_border,
              level->popup.w + popup_border * 2.0f,
              level->popup.h + popup_border * 2.0f,
              theme.border.r, theme.border.g, theme.border.b);
    draw_quad(app, level->popup.x, level->popup.y, level->popup.w,
              level->popup.h, theme.panel.r, theme.panel.g, theme.panel.b);
    for (int i = 0; i < level->item_count; ++i) {
      UIStoredMenuItem *item = &level->items[i];
      UIRect row = ui_menu_item_rect(level, font, i);
      if (item->kind == UI_MENU_ITEM_SEPARATOR) {
        draw_quad(app, row.x + theme.popup_padding,
                  row.y + row.h * 0.5f,
                  row.w - theme.popup_padding * 2.0f,
                  theme.separator_thickness, theme.separator.r,
                  theme.separator.g, theme.separator.b);
        continue;
      }
      bool hovered = ui_pointer_hit(row.x, row.y,
                                   row.w, row.h);
      if (hovered) {
        if (!item->disabled)
          draw_quad(app, row.x, row.y, row.w, row.h, theme.button_hover.r,
                    theme.button_hover.g, theme.button_hover.b);
        if (!item->disabled && item->kind == UI_MENU_ITEM_SUBMENU)
          ui_menu_open_child(depth, i, font);
        else if (g_menu.depth > depth + 1)
          ui_menu_close_from(depth + 1);
      }
      UIColor color = item->disabled ? theme.text_muted : theme.text;
      float text_scale = 14.0f * theme.text_scale;
      float baseline = row.y + (row.h + font->ascender * text_scale) * 0.5f;
      if (item->checked)
        draw_text_colored(app, font, "x", row.x + theme.popup_padding,
                          baseline, text_scale, theme.accent.r,
                          theme.accent.g, theme.accent.b);
      draw_text_colored(app, font, item->label,
                        row.x + theme.popup_padding + 18.0f, baseline,
                        text_scale, color.r, color.g, color.b);
      if (item->shortcut[0]) {
        float shortcut_w = measure_text_width(font, item->shortcut,
                                              text_scale * theme.secondary_text_scale);
        draw_text_colored(app, font, item->shortcut,
                          row.x + row.w - theme.popup_padding - shortcut_w -
                              (item->kind == UI_MENU_ITEM_SUBMENU ? 18.0f : 0.0f),
                          baseline, text_scale * theme.secondary_text_scale,
                          theme.text_muted.r, theme.text_muted.g,
                          theme.text_muted.b);
      }
      if (item->kind == UI_MENU_ITEM_SUBMENU)
        draw_text_colored(app, font, level->opens_left ? "<" : ">",
                          row.x + row.w - theme.popup_padding - 8.0f,
                          baseline, text_scale, color.r, color.g, color.b);
    }
  }

  int hit_depth = -1, hit_item = -1;
  for (int depth = g_menu.depth - 1; depth >= 0; --depth) {
    UIMenuLevel *level = &g_menu.levels[depth];
    if (!ui_pointer_hit(level->popup.x, level->popup.y,
                       level->popup.w, level->popup.h))
      continue;
    hit_depth = depth;
    for (int i = 0; i < level->item_count; ++i) {
      UIRect row = ui_menu_item_rect(level, font, i);
      if (ui_pointer_hit(row.x, row.y, row.w, row.h)) {
        hit_item = i;
        break;
      }
    }
    break;
  }
  g_menu.owning_depth = hit_depth;
  if (!g_mouse_clicked)
    return;
  if (hit_depth < 0) {
    if (g_menu.root_opened_this_frame) {
      g_mouse_clicked = false;
      g_click_claimed_by_menu = true;
      return;
    }
    ui_menu_close_hierarchy();
    g_mouse_clicked = false;
    g_click_claimed_by_menu = true;
    return;
  }
  if (hit_item < 0) {
    g_mouse_clicked = false;
    g_click_claimed_by_menu = true;
    return;
  }
  UIMenuLevel *level = &g_menu.levels[hit_depth];
  UIStoredMenuItem *item = &level->items[hit_item];
  if (item->disabled || item->kind != UI_MENU_ITEM_ACTION) {
    g_mouse_clicked = false;
    g_click_claimed_by_menu = true;
    return;
  }
  g_menu.result_pending = true;
  snprintf(g_menu.result_root_id, sizeof(g_menu.result_root_id), "%s",
           g_menu.levels[0].id);
  snprintf(g_menu.result_item_id, sizeof(g_menu.result_item_id), "%s",
           item->id);
  ui_menu_close_hierarchy();
  g_mouse_clicked = false;
  g_click_claimed_by_menu = true;
}

bool ui_menu_get_diagnostics(UIMenuDiagnostics *out) {
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  out->active_depth = g_menu.depth;
  out->hovered_depth = g_menu.hovered_depth;
  out->owning_depth = g_menu.owning_depth;
  out->escape_consumed = g_menu.escape_consumed;
  out->overflowed = g_menu.overflowed;
  for (int i = 0; i < g_menu.depth && i < UI_MENU_MAX_DEPTH; ++i) {
    out->popup_id[i] = g_menu.levels[i].id;
    out->parent_id[i] = g_menu.levels[i].parent_id;
    out->parent_item_id[i] = g_menu.levels[i].parent_item_id;
    out->anchor[i] = g_menu.levels[i].anchor;
    out->popup[i] = g_menu.levels[i].popup;
    out->opens_left[i] = g_menu.levels[i].opens_left;
  }
  return true;
}
#endif // EDITOR_ENABLED

typedef struct {
  char id[128];
  bool dragging;
  unsigned long long last_seen_frame;
} UISliderState;

typedef struct {
  char id[128];
  char buffer[32];
  float original;
  bool focused;
  bool invalid;
  unsigned long long last_seen_frame;
} UIFloatFieldState;

#define MAX_UI_FLOAT_FIELDS 96
static UIFloatFieldState float_field_states[MAX_UI_FLOAT_FIELDS];
static int float_field_state_count = 0;

static UIFloatFieldState *find_or_create_float_field_state(const char *id) {
  for (int i = 0; i < float_field_state_count; ++i)
    if (strcmp(float_field_states[i].id, id) == 0) {
      float_field_states[i].last_seen_frame = g_ui_frame_serial;
      return &float_field_states[i];
    }
  UIFloatFieldState *state = NULL;
  if (float_field_state_count < MAX_UI_FLOAT_FIELDS) {
    state = &float_field_states[float_field_state_count++];
  } else {
    for (int i = 0; i < float_field_state_count; ++i) {
      UIFloatFieldState *candidate = &float_field_states[i];
      if (candidate->focused ||
          candidate->last_seen_frame == g_ui_frame_serial)
        continue;
      if (!state || candidate->last_seen_frame < state->last_seen_frame)
        state = candidate;
    }
  }
  if (!state)
    return NULL;
  memset(state, 0, sizeof(*state));
  snprintf(state->id, sizeof(state->id), "%s", id);
  state->last_seen_frame = g_ui_frame_serial;
  return state;
}

static bool ui_float_key_pressed(APP *app, int key) {
  static bool down[GLFW_KEY_LAST + 1] = {false};
  bool now = glfwGetKey(app->window, key) == GLFW_PRESS;
  bool pressed = now && !down[key];
  down[key] = now;
  return pressed;
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

static void ui_float_insert(UIFloatFieldState *state, char c) {
  size_t len = strlen(state->buffer);
  if (len + 1 < sizeof(state->buffer)) {
    state->buffer[len] = c;
    state->buffer[len + 1] = '\0';
  }
}

bool ui_float_field(APP *app, const char *id, Font *font, float x, float y,
                    float width, float height, float scale, float *value,
                    float min, float max) {
  UIFloatFieldState *state = find_or_create_float_field_state(id);
  if (!state || !value || !isfinite(*value)) return false;
  UITheme theme = ui_get_theme();
  bool hover = ui_pointer_hit(x, y, width, height);
  if (hover) g_any_hover = true;
  if (g_mouse_clicked && !g_click_claimed_by_dropdown_list &&
      !g_click_claimed_by_menu) {
    if (hover && !state->focused) {
      state->focused = true; state->invalid = false; state->original = *value;
      snprintf(state->buffer, sizeof(state->buffer), "%.6g", *value);
    } else if (!hover) {
      state->focused = false;
    }
  }
  bool changed = false;
  if (state->focused) {
    bool menu_owns_escape = false;
#if 1 /* Generic menu support. */
    menu_owns_escape = ui_menu_hierarchy_is_open();
#endif
    bool float_escape_pressed = ui_float_key_pressed(app, GLFW_KEY_ESCAPE);
    if (!menu_owns_escape && float_escape_pressed) {
      *value = state->original; state->focused = false; state->invalid = false;
    } else if (ui_float_key_pressed(app, GLFW_KEY_ENTER)) {
      float parsed;
      if (ui_parse_float_clamped(state->buffer, min, max, &parsed)) {
        changed = parsed != *value; *value = parsed; state->focused = false;
        state->invalid = false;
      } else {
        state->invalid = true;
        ui_notify(UI_TOAST_ERROR, "Valore numerico non valido");
      }
    } else {
      bool ctrl = glfwGetKey(app->window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                  glfwGetKey(app->window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
      if (ctrl && ui_float_key_pressed(app, GLFW_KEY_A))
        state->buffer[0] = '\0';
      if (ui_float_key_pressed(app, GLFW_KEY_BACKSPACE)) {
        size_t len = strlen(state->buffer); if (len) state->buffer[len - 1] = '\0';
      }
      for (int key = GLFW_KEY_0; key <= GLFW_KEY_9; ++key)
        if (ui_float_key_pressed(app, key)) ui_float_insert(state, (char)('0' + key - GLFW_KEY_0));
      if (ui_float_key_pressed(app, GLFW_KEY_PERIOD)) ui_float_insert(state, '.');
      if (ui_float_key_pressed(app, GLFW_KEY_MINUS)) ui_float_insert(state, '-');
    }
  }
  if (!state->focused) snprintf(state->buffer, sizeof(state->buffer), "%.6g", *value);
  UIColor border = state->invalid ? theme.error :
                    state->focused ? theme.accent : theme.border;
  UIColor fill = state->focused ? theme.field_hover : theme.field;
  float thickness = theme.border_thickness;
  if (theme.paper_panels) {
    const float color[4] = {fill.r,fill.g,fill.b,1}, accent[3] = {border.r,border.g,border.b};
    draw_hud_paper_panel(app,x,y,width,height,color,accent,state->focused || state->invalid,ui_hash_id(id));
  } else {
    draw_quad(app, x - thickness, y - thickness, width + thickness * 2, height + thickness * 2,
              border.r, border.g, border.b);
    draw_quad(app, x, y, width, height, fill.r, fill.g, fill.b);
  }
  draw_text_colored(app, font, state->buffer, x + theme.label_value_gap,
                    y + (height + font->ascender * scale) * 0.5f, scale,
                    state->invalid ? theme.error.r : theme.text.r,
                    state->invalid ? theme.error.g : theme.text.g,
                    state->invalid ? theme.error.b : theme.text.b);
  return changed;
}

#define MAX_UI_SLIDERS 64
static UISliderState slider_states[MAX_UI_SLIDERS];
static int slider_state_count = 0;

static bool ui_any_slider_dragging(void) {
  for (int i = 0; i < slider_state_count; ++i)
    if (slider_states[i].dragging) return true;
  return false;
}
static void ui_release_stale_slider_drags(void) {
  for (int i = 0; i < slider_state_count; ++i)
    if (slider_states[i].dragging && slider_states[i].last_seen_frame + 1 < g_ui_frame_serial)
      slider_states[i].dragging = false;
}

static UISliderState *find_or_create_slider_state(const char *id) {
  for (int i = 0; i < slider_state_count; i++) {
    if (strcmp(slider_states[i].id, id) == 0) {
      slider_states[i].last_seen_frame = g_ui_frame_serial;
      return &slider_states[i];
    }
  }
  if (slider_state_count < MAX_UI_SLIDERS) {
    UISliderState *s = &slider_states[slider_state_count++];
    memset(s, 0, sizeof(*s));
    snprintf(s->id, sizeof(s->id), "%s", id);
    s->last_seen_frame = g_ui_frame_serial;
    return s;
  }
  UISliderState *reuse = NULL;
  for (int i = 0; i < slider_state_count; ++i) {
    UISliderState *candidate = &slider_states[i];
    if (candidate->dragging ||
        candidate->last_seen_frame == g_ui_frame_serial)
      continue;
    if (!reuse || candidate->last_seen_frame < reuse->last_seen_frame)
      reuse = candidate;
  }
  if (reuse) {
    memset(reuse, 0, sizeof(*reuse));
    snprintf(reuse->id, sizeof(reuse->id), "%s", id);
    reuse->last_seen_frame = g_ui_frame_serial;
    return reuse;
  }
  static bool warned = false;
  if (!warned) {
    LOG_WARN("UI: troppi slider simultanei (max %d) — id '%s' ignorato",
             MAX_UI_SLIDERS, id);
    warned = true;
  }
  return NULL;
}

// 'scale' funziona come nelle altre funzioni ui_* (ui_button_labeled,
// ui_dropdown): 1.0 = dimensioni base, il chiamante moltiplica per il
// proprio fattore (es. g_uiScale) se il suo pannello scala con la
// DIMENSIONE UI, o passa 1.0 se è un elemento a dimensione fissa (es.
// i modali). Prima queste dimensioni erano hardcoded (trackHeight 10,
// labelHeight 22, handleW 12) — un'unica altezza VERA fissa, uguale a
// qualunque g_uiScale, mentre QUASI OVUNQUE i chiamanti calcolavano lo
// spazio verticale ATTORNO allo slider moltiplicando per g_uiScale.
// Con g_uiScale < 1 (0.75 di default) quello spazio si restringeva ma
// lo slider restava alla sua taglia intera, finendo per sovrapporsi
// alla riga successiva — il bug "le scritte si accavallano" presente
// in ogni pannello con slider (NEMICO, BOX, PARTICELLE, PATH,
// COMBATTIMENTO...). Ora lo slider scala esattamente come tutto il
// resto del pannello che lo contiene, sempre coerente con qualunque
// g_uiScale, niente più calcoli compensativi sparsi nei chiamanti.
bool ui_slider(APP *app, const char *id, Font *font, TextAnchor anchor,
               float margin, float scale, float offset_x, float offset_y,
               float width, float *value, float min, float max,
               const char *label, bool *out_drag_started) {
  if (out_drag_started)
    *out_drag_started = false;

  UISliderState *state = find_or_create_slider_state(id);
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
    x = VIRTUAL_W - margin - width;
    y = margin;
    break;
  case ANCHOR_BOTTOM_LEFT:
    x = margin;
    y = VIRTUAL_H - margin - totalHeight;
    break;
  case ANCHOR_BOTTOM_RIGHT:
    x = VIRTUAL_W - margin - width;
    y = VIRTUAL_H - margin - totalHeight;
    break;
  case ANCHOR_CENTER:
    x = (VIRTUAL_W - width) * 0.5f;
    y = (VIRTUAL_H - totalHeight) * 0.5f;
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
         measure_text_width(font, displaySliderLabel, labelScale) > labelAvailable) {
    size_t len = strlen(displaySliderLabel);
    if (len < 4) break;
    displaySliderLabel[len - 1] = '\0';
    displaySliderLabel[len - 2] = '.';
    displaySliderLabel[len - 3] = '.';
    displaySliderLabel[len - 4] = '.';
  }
  draw_text_colored(app, font, displaySliderLabel, x,
                    y + (labelHeight + font->ascender * labelScale) * 0.5f,
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
  draw_quad(app, x, trackY, trackW, trackHeight, sliderTheme.separator.r,
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
    draw_quad(app, x, trackY, fillW, trackHeight, sliderTheme.button_border.r,
             sliderTheme.button_border.g, sliderTheme.button_border.b);
  }

  bool hoverTrack = g_mouse_x >= x - 6 && g_mouse_x <= x + trackW + 6 &&
                    g_mouse_y >= trackY - 8 &&
                    g_mouse_y <= trackY + trackHeight + 8;
  if (hoverTrack)
    g_any_hover = true;

  bool wasDragging = state->dragging;
  if (g_mouse_down && !g_click_claimed_by_dropdown_list &&
      !g_click_claimed_by_menu &&
      (hoverTrack || state->dragging)) {
    state->dragging = true;
  }
  if (!g_mouse_down) {
    state->dragging = false;
  }

  if (state->dragging && !wasDragging && out_drag_started) {
    *out_drag_started = true;
  }

  bool changed = false;
  if (state->dragging) {
    float rel = (g_mouse_x - x - handleW * 0.5f) / (trackW - handleW);
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
  draw_quad(app, handleX, trackY - 3, handleW, trackHeight + 6,
           sliderTheme.button_border.r * hc, sliderTheme.button_border.g * hc,
           sliderTheme.button_border.b * hc);

  char valueID[96];
  snprintf(valueID, sizeof(valueID), "%s_exact", id);
  changed |= ui_float_field(app, valueID, font,
                            x + trackW + sliderTheme.label_value_gap * scale, y,
                            valueW, fieldH, labelScale, value, min, max);

  return changed;
}

bool ui_slider_is_dragging(const char *id) {
  if (!id)
    return false;
  for (int i = 0; i < slider_state_count; ++i)
    if (strcmp(slider_states[i].id, id) == 0)
      return slider_states[i].dragging;
  return false;
}

UIPropertyLayout ui_property_layout_begin(float x, float y, float width,
                                          float scale) {
  UITheme theme = ui_get_theme();
  if (scale <= 0.0f) scale = 1.0f;
  UIPropertyLayout layout = {.x = x,
                             .y = y,
                             .width = width,
                             .cursor_y = y,
                             .label_width = width * 0.34f,
                             .row_height = theme.field_height * scale,
                             .scale = scale};
  if (layout.label_width > 180.0f * scale) layout.label_width = 180.0f * scale;
  return layout;
}

void ui_property_layout_commit(UILayout *parent, const UIPropertyLayout *layout) {
  if (parent && layout && layout->cursor_y > parent->cursor_y)
    parent->cursor_y = layout->cursor_y;
}

UILayout ui_layout_begin(UIRect bounds, float scale) {
  UITheme theme = ui_get_theme();
  if (scale <= 0.0f) scale = 1.0f;
  return (UILayout){.bounds = bounds,
                    .cursor_y = bounds.y,
                    .indent = 0.0f,
                    .row_height = theme.row_height * scale,
                    .spacing = theme.row_spacing * scale,
                    .scale = scale};
}

void ui_layout_end(UILayout *layout) { (void)layout; }

float ui_layout_next_row(UILayout *layout) {
  if (!layout) return 0.0f;
  float y = layout->cursor_y;
  layout->cursor_y += layout->row_height + layout->spacing;
  return y;
}

void ui_layout_space(UILayout *layout, float amount) {
  if (layout && amount > 0.0f) layout->cursor_y += amount * layout->scale;
}

void ui_layout_indent(UILayout *layout, float amount) {
  if (layout) layout->indent += amount * layout->scale;
}

UILayout ui_layout_begin_nested(UILayout *parent, float indent) {
  if (!parent) return (UILayout){0};
  UILayout child = *parent;
  child.bounds.x += indent * child.scale;
  child.bounds.w -= indent * child.scale;
  child.cursor_y = parent->cursor_y;
  child.indent += indent * child.scale;
  return child;
}

void ui_layout_end_nested(UILayout *layout) { (void)layout; }

UIRow ui_row_begin(UILayout *layout, int item_count) {
  if (!layout || item_count < 1) return (UIRow){0};
  float y = ui_layout_next_row(layout);
  return (UIRow){.bounds = {layout->bounds.x + layout->indent, y,
                             layout->bounds.w - layout->indent, layout->row_height},
                 .count = item_count, .spacing = layout->spacing};
}

UIRect ui_row_next(UIRow *row) {
  if (!row || row->index >= row->count) return (UIRect){0};
  float width = (row->bounds.w - row->spacing * (row->count - 1)) / row->count;
  UIRect r = {row->bounds.x + row->index * (width + row->spacing), row->bounds.y,
              width, row->bounds.h};
  row->index++;
  return r;
}

static bool g_section_default_open = true;
static UISectionState *find_or_create_section_state(const char *key) {
  UISectionState *state = NULL;
  for (int i = 0; i < g_section_state_count; ++i) {
    if (strcmp(g_section_states[i].id, key) == 0) {
      state = &g_section_states[i];
      state->last_seen_frame = g_ui_frame_serial;
      return state;
    }
  }
  if (g_section_state_count < MAX_UI_SECTIONS) {
    state = &g_section_states[g_section_state_count++];
  } else {
    for (int i = 0; i < g_section_state_count; ++i) {
      UISectionState *candidate = &g_section_states[i];
      if (candidate->last_seen_frame == g_ui_frame_serial)
        continue;
      if (!state || candidate->last_seen_frame < state->last_seen_frame)
        state = candidate;
    }
  }
  if (!state) {
    static bool warned = false;
    if (!warned) {
      LOG_WARN("UI: troppe sezioni simultanee (max %d) — id '%s' ignorato",
               MAX_UI_SECTIONS, key);
      warned = true;
    }
    return NULL;
  }
  memset(state, 0, sizeof(*state));
  snprintf(state->id, sizeof(state->id), "%s", key);
  state->open = g_section_default_open;
  state->last_seen_frame = g_ui_frame_serial;
  return state;
}

/* Sezione che parte CHIUSA la prima volta (funzioni rare/avanzate: non
 * affollano il pannello); dopo il primo click lo stato e' quello scelto
 * dall'utente, come per ui_section_begin. */
UISection ui_section_begin_collapsed(APP *app, UILayout *layout, const char *id,
                                     const char *title, Font *font) {
  g_section_default_open = false;
  UISection s = ui_section_begin(app, layout, id, title, font);
  g_section_default_open = true;
  return s;
}

UISection ui_section_begin(APP *app, UILayout *layout, const char *id,
                           const char *title, Font *font) {
  if (!layout) return (UISection){0};
  const char *key = ui_scoped_id(id);
  UISectionState *state = find_or_create_section_state(key);
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
  if (ui_button_labeled(app, key, label, font, ANCHOR_TOP_LEFT, 0,
                        14.0f * layout->scale,
                        layout->bounds.x + layout->indent, y, &style) &&
      state) {
    state->open = open = !open;
#ifdef DEBUG
    LOG_INFO("[UI SECTION] %s -> %s", key, open ? "expanded" : "collapsed");
#endif
  }
  if (open)
    draw_quad(app, layout->bounds.x + layout->indent, y,
              fmaxf(theme.separator_thickness * 2.0f, 2.0f * layout->scale),
              layout->row_height, theme.accent.r, theme.accent.g,
              theme.accent.b);
  UILayout child = ui_layout_begin_nested(layout, ui_get_theme().indent);
  if (!open) child.cursor_y = layout->cursor_y;
  ui_push_id(id);
  return (UISection){.layout = child, .parent = layout, .open = open};
}

void ui_section_end(UISection *section) {
  if (!section) return;
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
static bool ui_any_slider_dragging(void);
static bool ui_any_dropdown_is_open(void);
static bool ui_scroll_wheel_allowed_for(UIRect viewport) {
  if (ui_any_slider_dragging() || ui_any_dropdown_is_open()) return false;
#if 1 /* Generic menu support. */
  if (ui_menu_hierarchy_is_open()) return false;
#endif
  if (g_modal_rect_valid) {
    float cx = viewport.x + viewport.w * 0.5f, cy = viewport.y + viewport.h * 0.5f;
    bool inside = cx >= g_modal_rect.x && cx <= g_modal_rect.x + g_modal_rect.w &&
                  cy >= g_modal_rect.y && cy <= g_modal_rect.y + g_modal_rect.h;
    if (!inside) return false;
  }
  return true;
}

UIScrollScope ui_scroll_begin(const char *id, UIRect viewport, float scale) {
  const char *key = ui_scoped_id(id);
  int index = -1;
  for (int i = 0; i < g_scroll_state_count; ++i)
    if (strcmp(g_scroll_states[i].id, key) == 0) index = i;
  if (index < 0 && g_scroll_state_count < MAX_UI_SCROLL_SCOPES) {
    index = g_scroll_state_count++;
    snprintf(g_scroll_states[index].id, sizeof(g_scroll_states[index].id), "%s", key);
  }
  UIScrollState *state = index >= 0 ? &g_scroll_states[index] : NULL;
  float max_scroll = state ? fmaxf(0.0f, state->content_height - viewport.h) : 0.0f;
  if (state) {
    state->wheel_consumed = false;
    state->viewport_height = viewport.h;
    if (!isfinite(state->offset)) state->offset = 0.0f;
    if (state->offset > max_scroll) state->offset = max_scroll;
    UITheme theme = ui_get_theme();
    float track_w = theme.scrollbar_width * scale;
    float thumb_h = max_scroll > 0.0f ? fmaxf(theme.row_height * scale,
        viewport.h * viewport.h / state->content_height) : viewport.h;
    float thumb_y = max_scroll > 0.0f ? viewport.y + (viewport.h - thumb_h) * (state->offset / max_scroll) : viewport.y;
    bool on_thumb = g_mouse_x >= viewport.x + viewport.w - track_w &&
                    g_mouse_x <= viewport.x + viewport.w && g_mouse_y >= thumb_y && g_mouse_y <= thumb_y + thumb_h;
    state->thumb_hovered = on_thumb;
    if (g_mouse_clicked && on_thumb) { state->dragging = true; state->drag_grab = g_mouse_y - thumb_y; }
    if (!g_mouse_down) state->dragging = false;
    if (state->dragging && max_scroll > 0.0f) {
      float travel = viewport.h - thumb_h;
      float pos = fmaxf(0.0f, fminf(travel, g_mouse_y - viewport.y - state->drag_grab));
      state->offset = travel > 0.0f ? (pos / travel) * max_scroll : 0.0f;
    }
    if (!state->dragging && g_mouse_x >= viewport.x && g_mouse_x <= viewport.x + viewport.w &&
        g_mouse_y >= viewport.y && g_mouse_y <= viewport.y + viewport.h &&
        ui_scroll_delta() != 0.0f && ui_scroll_wheel_allowed_for(viewport)) {
      state->offset = fmaxf(0.0f, fminf(max_scroll, state->offset - ui_scroll_delta() * ui_get_theme().row_height * scale));
      state->wheel_consumed = ui_consume_scroll_delta();
    }
  }
  ui_clip_push(viewport);
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
  return (UIScrollScope){.layout = layout, .viewport = viewport,
                         .offset = state ? state->offset : 0.0f,
                         .max_scroll = max_scroll, .state_index = index};
}

void ui_scroll_end(APP *app, UIScrollScope *scope) {
  if (!scope) return;
  ui_clip_pop();
  if (scope->state_index < 0 || scope->state_index >= g_scroll_state_count) return;
  UIScrollState *state = &g_scroll_states[scope->state_index];
  state->content_height = fmaxf(0.0f, scope->layout.cursor_y + state->offset - scope->viewport.y);
  float max_scroll = fmaxf(0.0f, state->content_height - scope->viewport.h);
  if (state->offset > max_scroll) state->offset = max_scroll;
  if (max_scroll <= 0.0f) return;
  UITheme t = ui_get_theme();
  float track_w = t.scrollbar_width * scope->layout.scale;
  float thumb_h = fmaxf(t.row_height * scope->layout.scale,
                         scope->viewport.h * scope->viewport.h / state->content_height);
  float thumb_y = scope->viewport.y + (scope->viewport.h - thumb_h) * (state->offset / max_scroll);
  draw_quad(app, scope->viewport.x + scope->viewport.w - track_w, scope->viewport.y,
            track_w, scope->viewport.h, t.separator.r, t.separator.g, t.separator.b);
  UIColor thumb = state->thumb_hovered || state->dragging ? t.accent
                                                           : t.field_hover;
  float inset = fminf(2.0f * scope->layout.scale, track_w * 0.25f);
  draw_quad(app, scope->viewport.x + scope->viewport.w - track_w + inset,
            thumb_y + inset, track_w - inset * 2.0f,
            fmaxf(2.0f, thumb_h - inset * 2.0f), thumb.r, thumb.g, thumb.b);
}

bool ui_scroll_get_diagnostics(const char *id, UIScrollDiagnostics *out) {
  if (!id || !id[0] || !out)
    return false;
  const char *key = ui_scoped_id(id);
  UIScrollState *state = NULL;
  const char *drag_owner = NULL;
  for (int i = 0; i < g_scroll_state_count; ++i) {
    if (g_scroll_states[i].dragging)
      drag_owner = g_scroll_states[i].id;
    if (strcmp(g_scroll_states[i].id, key) == 0)
      state = &g_scroll_states[i];
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

UIPanel ui_panel_begin(APP *app, const char *id, const char *title,
                       Font *font, UIRect bounds, float scale) {
  UITheme theme = ui_get_theme();
  if (scale <= 0.0f) scale = 1.0f;
  float pad = theme.panel_padding * scale;
  float border = theme.border_thickness * scale;
  draw_quad(app, bounds.x - border, bounds.y - border,
            bounds.w + border * 2.0f, bounds.h + border * 2.0f,
            theme.border.r, theme.border.g, theme.border.b);
  draw_quad(app, bounds.x, bounds.y, bounds.w, bounds.h, theme.panel.r,
            theme.panel.g, theme.panel.b);
  float header_h = 0.0f;
  if (title && title[0]) {
    header_h = theme.panel_header_height * scale;
    draw_quad(app, bounds.x, bounds.y, bounds.w, header_h,
              theme.panel_header.r, theme.panel_header.g, theme.panel_header.b);
    draw_quad(app, bounds.x, bounds.y + header_h - theme.separator_thickness,
              bounds.w, theme.separator_thickness, theme.separator.r,
              theme.separator.g, theme.separator.b);
    float title_scale = 14.0f * scale * theme.section_text_scale;
    draw_text_colored(app, font, title, bounds.x + pad,
                      bounds.y + (header_h + font->ascender * title_scale) * 0.5f,
                      title_scale, theme.text.r, theme.text.g, theme.text.b);
  }
  UIRect content = {bounds.x + pad, bounds.y + header_h + pad,
                    fmaxf(0.0f, bounds.w - pad * 2.0f),
                    fmaxf(0.0f, bounds.h - header_h - pad * 2.0f)};
  ui_input_region_register(ui_scoped_id(id), bounds);
  ui_clip_push(content);
  return (UIPanel){.outer = bounds, .content = content,
                   .layout = ui_layout_begin(content, scale), .open = true};
}

void ui_panel_end(UIPanel *panel) {
  if (panel && panel->open) { ui_layout_end(&panel->layout); ui_clip_pop(); panel->open = false; }
}

float ui_property_layout_next_row(UIPropertyLayout *layout) {
  if (!layout) return 0.0f;
  float y = layout->cursor_y;
  layout->cursor_y += layout->row_height + ui_get_theme().row_spacing * layout->scale;
  return y;
}

void ui_property_layout_gap(UIPropertyLayout *layout, float amount) {
  if (layout && amount > 0.0f) layout->cursor_y += amount * layout->scale;
}

void ui_property_label(APP *app, const UIPropertyLayout *layout, Font *font,
                       const char *label, float y) {
  if (!layout || !font || !label) return;
  UITheme theme = ui_get_theme();
  char display[160];
  ui_humanize_label(label, display, sizeof(display));
  float text_scale = 14.0f * layout->scale * theme.secondary_text_scale;
  /* L'etichetta non deve mai finire sotto il widget del valore: viene
     troncata con "..." alla colonna sinistra (meno il gap). Le righe di sola
     lettura (ui_property_value/ui_property_note) usano tutta la larghezza. */
  float max_w = layout->label_width - theme.label_value_gap * layout->scale;
  if (layout->label_width >= layout->width) max_w = layout->width;
  ui_ellipsize_right(font, display, text_scale, max_w, display, sizeof(display));
  draw_text_colored(app, font, display, layout->x,
                    y + (layout->row_height + font->ascender * text_scale) * 0.5f,
                    text_scale,
                    theme.text_muted.r, theme.text_muted.g, theme.text_muted.b);
}

void ui_property_value(APP *app, UIPropertyLayout *layout, Font *font,
                       const char *label, const char *value) {
  if (!layout || !font) return;
  UITheme theme = ui_get_theme();
  float y = ui_property_layout_next_row(layout);
  ui_property_label(app, layout, font, label, y);
  float x = layout->x + layout->label_width;
  float w = layout->width - layout->label_width;
  float text_scale = 14.0f * layout->scale * theme.text_scale;
  char display[192];
  ui_ellipsize_right(font, value ? value : "", text_scale,
                     w - theme.label_value_gap * layout->scale, display,
                     sizeof(display));
  draw_text_colored(app, font, display, x + theme.label_value_gap * layout->scale,
                    y + (layout->row_height + font->ascender * text_scale) * 0.5f,
                    text_scale, theme.text.r, theme.text.g, theme.text.b);
}

void ui_property_note(APP *app, UIPropertyLayout *layout, Font *font,
                      const char *text) {
  if (!layout || !font) return;
  UITheme theme = ui_get_theme();
  float y = ui_property_layout_next_row(layout);
  float text_scale = 14.0f * layout->scale * theme.secondary_text_scale;
  char display[256];
  ui_ellipsize_right(font, text ? text : "", text_scale, layout->width, display,
                     sizeof(display));
  draw_text_colored(app, font, display, layout->x,
                    y + (layout->row_height + font->ascender * text_scale) * 0.5f,
                    text_scale, theme.text_muted.r, theme.text_muted.g,
                    theme.text_muted.b);
}

bool ui_property_float(APP *app, UIPropertyLayout *layout, const char *id,
                       Font *font, const char *label, float *value,
                       float min, float max) {
  if (!layout) return false;
  float y = ui_property_layout_next_row(layout);
  ui_property_label(app, layout, font, label, y);
  float valueX = layout->x + layout->label_width;
  return ui_float_field(app, ui_scoped_id(id), font, valueX, y,
                        layout->width - layout->label_width,
                        layout->row_height, 14.0f * layout->scale,
                        value, min, max);
}

bool ui_property_int(APP *app, UIPropertyLayout *layout, const char *id,
                     Font *font, const char *label, int *value, int min, int max) {
  if (!layout || !value) return false;
  float value_f = (float)*value;
  bool changed = ui_property_float(app, layout, id, font, label, &value_f,
                                   (float)min, (float)max);
  if (changed) *value = (int)(value_f + (value_f >= 0.0f ? 0.5f : -0.5f));
  return changed;
}

bool ui_property_vec2(APP *app, UIPropertyLayout *layout, const char *id,
                      Font *font, const char *label, Vec2 *value,
                      float min, float max) {
  if (!layout || !value) return false;
  float y = ui_property_layout_next_row(layout);
  ui_property_label(app, layout, font, label, y);
  float x = layout->x + layout->label_width;
  float gap = ui_get_theme().row_spacing * layout->scale;
  float w = (layout->width - layout->label_width - gap) * 0.5f;
  bool changed = ui_float_field(app, ui_scoped_id(id), font, x, y, w,
                                layout->row_height, 14.0f * layout->scale,
                                &value->x, min, max);
  char yid[96]; snprintf(yid, sizeof(yid), "%s/y", id);
  changed |= ui_float_field(app, ui_scoped_id(yid), font, x + w + gap, y, w,
                            layout->row_height, 14.0f * layout->scale,
                            &value->y, min, max);
  return changed;
}

bool ui_property_slider(APP *app, UIPropertyLayout *layout, const char *id,
                        Font *font, const char *label, float *value,
                        float min, float max, bool *out_drag_started) {
  if (!layout) return false;
  float y = ui_property_layout_next_row(layout);
  return ui_slider(app, ui_scoped_id(id), font, ANCHOR_TOP_LEFT, 0, layout->scale,
                   layout->x, y, layout->width, value, min, max, label,
                   out_drag_started);
}

bool ui_property_toggle(APP *app, UIPropertyLayout *layout, const char *id,
                        Font *font, const char *label, bool *value) {
  if (!layout) return false;
  float y = ui_property_layout_next_row(layout);
  ui_property_label(app, layout, font, label, y);
  return ui_toggle(app, ui_scoped_id(id), "", font, layout->x + layout->label_width, y,
                   layout->scale, layout->width - layout->label_width, value);
}

bool ui_property_checkbox(APP *app, UIPropertyLayout *layout, const char *id,
                          Font *font, const char *label, bool *value) {
  if (!layout) return false;
  float y = ui_property_layout_next_row(layout);
  return ui_checkbox(app, ui_scoped_id(id), label, font,
                     layout->x, y, 14.0f * layout->scale, value);
}

bool ui_property_dropdown(APP *app, UIPropertyLayout *layout, const char *id,
                          Font *font, const char *label,
                          const char *const *options, int option_count,
                          int *selected) {
  if (!layout) return false;
  float y = ui_property_layout_next_row(layout);
  ui_property_label(app, layout, font, label, y);
  UIButtonStyle style = {.min_width = layout->width - layout->label_width,
                         .min_height = layout->row_height};
  return ui_dropdown(app, ui_scoped_id(id), font, ANCHOR_TOP_LEFT, 0, 14.0f * layout->scale,
                     layout->x + layout->label_width, y,
                     layout->width - layout->label_width, options, option_count,
                     selected, NULL, &style);
}

bool ui_property_text(APP *app, UIPropertyLayout *layout, const char *id,
                      Font *font, const char *label, char *buffer,
                      size_t buffer_size) {
  if (!layout) return false;
  float y = ui_property_layout_next_row(layout);
  ui_property_label(app, layout, font, label, y);
  UIButtonStyle style = {.min_width = layout->width - layout->label_width,
                         .min_height = layout->row_height};
  return ui_text_input(app, ui_scoped_id(id), font, ANCHOR_TOP_LEFT, 0,
                       14.0f * layout->scale,
                       layout->x + layout->label_width, y,
                       layout->width - layout->label_width, buffer, buffer_size,
                       &style);
}

bool ui_property_action(APP *app, UIPropertyLayout *layout, const char *id,
                        Font *font, const char *label, bool danger) {
  if (!layout) return false;
  float y = ui_property_layout_next_row(layout);
  UIButtonStyle style = {.min_width = layout->width,
                         .max_width = layout->width,
                         .min_height = layout->row_height,
                         .danger = danger};
  return ui_button_labeled(app, ui_scoped_id(id), label, font, ANCHOR_TOP_LEFT, 0,
                           14.0f * layout->scale, layout->x, y, &style);
}

unsigned ui_property_action_row(APP *app, UIPropertyLayout *layout,
                                const char *id, Font *font,
                                const char *const *labels, int count) {
  if (!layout || !labels || count < 1) return 0;
  float y = ui_property_layout_next_row(layout);
  float gap = ui_get_theme().row_spacing * layout->scale;
  float width = (layout->width - gap * (count - 1)) / count;
  unsigned result = 0;
  for (int i = 0; i < count && i < 31; ++i) {
    UIRect r = {layout->x + i * (width + gap), y, width, layout->row_height}; char action_id[96];
    snprintf(action_id, sizeof(action_id), "%s/%d", id, i);
    UIButtonStyle style = {.min_width = r.w, .max_width = r.w,
                           .min_height = r.h};
    if (ui_button_labeled(app, ui_scoped_id(action_id), labels[i], font,
                          ANCHOR_TOP_LEFT, 0, 14.0f * layout->scale, r.x, r.y, &style)) result |= 1u << i;
  }
  return result;
}

bool ui_property_asset(APP *app, UIPropertyLayout *layout, const char *id,
                       Font *font, const char *label, char *path,
                       size_t path_size, bool *out_browse) {
  if (!layout || !path || path_size == 0) return false;
  if (out_browse) *out_browse = false;
  float y = ui_property_layout_next_row(layout);
  ui_property_label(app, layout, font, label, y);
  UITheme theme = ui_get_theme();
  float x = layout->x + layout->label_width;
  float browse_w = theme.button_height * layout->scale;
  float field_w = layout->width - layout->label_width - browse_w - theme.row_spacing * layout->scale;
  bool hover = ui_pointer_hit(x, y, field_w,
                             layout->row_height);
  if (hover)
    g_any_hover = true;
  bool browse = hover && g_mouse_clicked &&
                !g_click_claimed_by_dropdown_list && !g_click_claimed_by_menu;
  UIColor fill = hover ? theme.field_hover : theme.field;
  UIColor border = hover ? theme.accent : theme.border;
  float thickness = theme.border_thickness * layout->scale;
  draw_quad(app, x - thickness, y - thickness, field_w + thickness * 2.0f,
            layout->row_height + thickness * 2.0f, border.r, border.g,
            border.b);
  draw_quad(app, x, y, field_w, layout->row_height, fill.r, fill.g, fill.b);
  float text_scale = 14.0f * layout->scale * theme.text_scale;
  char display[192];
  ui_ellipsize_left(font, path[0] ? path : "Nessun asset", text_scale,
                    field_w - theme.label_value_gap * layout->scale * 2.0f,
                    display, sizeof(display));
  UIColor text_color = path[0] ? theme.text : theme.text_muted;
  draw_text_colored(app, font, display,
                    x + theme.label_value_gap * layout->scale,
                    y + (layout->row_height + font->ascender * text_scale) * 0.5f,
                    text_scale, text_color.r, text_color.g, text_color.b);
  char browse_id[96]; snprintf(browse_id, sizeof(browse_id), "%s/browse", id);
  UIButtonStyle browse_style = {.min_width = browse_w, .max_width = browse_w,
                                .min_height = layout->row_height,
                                .selected = false};
  if (ui_button_labeled(app, ui_scoped_id(browse_id), "...", font, ANCHOR_TOP_LEFT, 0,
                        14.0f * layout->scale, x + field_w + theme.row_spacing * layout->scale, y, &browse_style)) {
    browse = true;
  }
  if (out_browse) *out_browse = browse;
  return browse;
}

typedef struct {
  char id[128];
  bool focused;
  int caret;
  char original[256];
  unsigned long long last_seen_frame;
} UITextInputState;

#define MAX_UI_TEXT_INPUTS 32
static UITextInputState text_input_states[MAX_UI_TEXT_INPUTS];
static int text_input_state_count = 0;

static UITextInputState *find_or_create_text_input_state(const char *id) {
  for (int i = 0; i < text_input_state_count; i++) {
    if (strcmp(text_input_states[i].id, id) == 0) {
      text_input_states[i].last_seen_frame = g_ui_frame_serial;
      return &text_input_states[i];
    }
  }
  if (text_input_state_count < MAX_UI_TEXT_INPUTS) {
    UITextInputState *s = &text_input_states[text_input_state_count++];
    memset(s, 0, sizeof(*s));
    snprintf(s->id, sizeof(s->id), "%s", id);
    s->last_seen_frame = g_ui_frame_serial;
    return s;
  }
  UITextInputState *reuse = NULL;
  for (int i = 0; i < text_input_state_count; ++i) {
    UITextInputState *candidate = &text_input_states[i];
    if (candidate->focused ||
        candidate->last_seen_frame == g_ui_frame_serial)
      continue;
    if (!reuse || candidate->last_seen_frame < reuse->last_seen_frame)
      reuse = candidate;
  }
  if (reuse) {
    memset(reuse, 0, sizeof(*reuse));
    snprintf(reuse->id, sizeof(reuse->id), "%s", id);
    reuse->last_seen_frame = g_ui_frame_serial;
    return reuse;
  }
  static bool warned = false;
  if (!warned) {
    LOG_WARN("UI: troppi campi testo simultanei (max %d) — id '%s' ignorato",
             MAX_UI_TEXT_INPUTS, id);
    warned = true;
  }
  return NULL;
}

bool ui_text_input_is_focused(const char *id) {
  for (int i = 0; i < text_input_state_count; i++) {
    if (strcmp(text_input_states[i].id, id) == 0)
      return text_input_states[i].focused;
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
  for (int i = 0; i < text_input_state_count; i++) {
    if (text_input_states[i].focused)
      return true;
  }
  return false;
}

void ui_text_input_focus(const char *id) {
  UITextInputState *state = find_or_create_text_input_state(id);
  if (state)
    state->focused = true;
}

static void ui_text_insert(char *buffer, int buffer_size, int *caret, char c) {
  int len = (int)strlen(buffer);
  if (len >= buffer_size - 1) return;
  if (*caret < 0) *caret = 0;
  if (*caret > len) *caret = len;
  memmove(buffer + *caret + 1, buffer + *caret, (size_t)(len - *caret + 1));
  buffer[*caret] = c;
  (*caret)++;
}

bool ui_text_input(APP *app, const char *id, Font *font, TextAnchor anchor,
                   float margin, float scale, float offset_x, float offset_y,
                   float width, char *buffer, int buffer_size,
                   const UIButtonStyle *style) {
  UITextInputState *state = find_or_create_text_input_state(id);
  if (!state || !buffer || buffer_size <= 0)
    return false;

  UITheme theme = ui_get_theme();
  float padding_x = (style && style->padding_x > 0)
                        ? style->padding_x
                        : theme.label_value_gap;
  float padding_y = (style && style->padding_y > 0)
                        ? style->padding_y
                        : theme.row_spacing;
  float ascent = font->ascender * scale;

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
    x = VIRTUAL_W - margin - w;
    y = margin;
    break;
  case ANCHOR_BOTTOM_LEFT:
    x = margin;
    y = VIRTUAL_H - margin - h;
    break;
  case ANCHOR_BOTTOM_RIGHT:
    x = VIRTUAL_W - margin - w;
    y = VIRTUAL_H - margin - h;
    break;
  case ANCHOR_CENTER:
    x = (VIRTUAL_W - w) * 0.5f;
    y = (VIRTUAL_H - h) * 0.5f;
    break;
  default:
    x = margin;
    y = margin;
  }
  x += offset_x;
  y += offset_y;

  bool hover = ui_pointer_hit(x, y, w, h);
  if (hover)
    g_any_hover = true;

  // Un solo edge-detect del click, condiviso da tutte le istanze: va bene
  // perché il mouse è uno solo — un click attiva il focus se cade dentro
  // questo campo, altrimenti (se il campo era già a fuoco) lo disattiva.
  // Il fronte di discesa del click è calcolato una volta sola per frame in
  // ui_begin_frame (g_mouse_clicked) — niente più static locale qui, che
  // altrimenti verrebbe "consumata" da questa chiamata e lascerebbe altri
  // widget dello stesso tipo a secco nello stesso frame.
  if (g_mouse_clicked && !g_click_claimed_by_dropdown_list &&
      !g_click_claimed_by_menu) {
    if (hover && !state->focused) {
      state->focused = true;
      state->caret = (int)strlen(buffer);
      snprintf(state->original, sizeof(state->original), "%s", buffer);
    } else if (!hover) {
      state->focused = false;
    }
  }

  bool confirmed = false;

  if (state->focused) {
    static bool enterLocked = false;
    bool enterPressed = glfwGetKey(app->window, GLFW_KEY_ENTER) == GLFW_PRESS;
    if (enterPressed && !enterLocked) {
      confirmed = true;
      state->focused = false;
    }
    enterLocked = enterPressed;

    static bool escLocked = false;
    bool escPressed = glfwGetKey(app->window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    bool menuOwnsEscape = false;
#if 1 /* Generic menu support. */
    menuOwnsEscape = ui_menu_hierarchy_is_open();
#endif
    if (escPressed && !escLocked && !menuOwnsEscape) {
      snprintf(buffer, (size_t)buffer_size, "%s", state->original);
      state->focused = false;
    }
    escLocked = escPressed;

    static bool bsLocked = false;
    bool bsPressed = glfwGetKey(app->window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
    if (bsPressed && !bsLocked) {
      int len = (int)strlen(buffer);
      if (state->caret > 0 && len > 0) {
        memmove(buffer + state->caret - 1, buffer + state->caret,
                (size_t)(len - state->caret + 1));
        state->caret--;
      }
    }
    bsLocked = bsPressed;

    // Lettere forzate minuscole: i nomi file di questo progetto sono tutti
    // minuscoli ("claw_attack", "player_slash") e su Linux il filesystem è
    // case-sensitive — meglio non introdurre un mismatch qui.
    // Con SHIFT tenuto la lettera resta maiuscola: scelta esplicita
    // dell'utente (nomi di layer, etichette), mai il default.
    bool shiftHeld = glfwGetKey(app->window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                     glfwGetKey(app->window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    static bool letterLocked[26] = {0};
    for (int k = GLFW_KEY_A; k <= GLFW_KEY_Z; k++) {
      int idx = k - GLFW_KEY_A;
      bool pressed = glfwGetKey(app->window, k) == GLFW_PRESS;
      if (pressed && !letterLocked[idx]) {
        ui_text_insert(buffer, buffer_size, &state->caret,
                       (char)((shiftHeld ? 'A' : 'a') + idx));
      }
      letterLocked[idx] = pressed;
    }

    // '.', '/' e spazio: servono ai campi percorso (file picker PERCORSO /
    // NOME FILE, sprite degli attachment) e ai nomi editoriali. Prima
    // erano impossibili da digitare.
    static bool punctLocked[3] = {0};
    const int punctKeys[3] = {GLFW_KEY_PERIOD, GLFW_KEY_SLASH, GLFW_KEY_SPACE};
    const char punctChars[3] = {'.', '/', ' '};
    for (int k = 0; k < 3; ++k) {
      bool pressed = glfwGetKey(app->window, punctKeys[k]) == GLFW_PRESS;
      if (pressed && !punctLocked[k])
        ui_text_insert(buffer, buffer_size, &state->caret, punctChars[k]);
      punctLocked[k] = pressed;
    }

    static bool digitLocked[10] = {0};
    for (int k = GLFW_KEY_0; k <= GLFW_KEY_9; k++) {
      int idx = k - GLFW_KEY_0;
      bool pressed = glfwGetKey(app->window, k) == GLFW_PRESS;
      if (pressed && !digitLocked[idx]) {
        ui_text_insert(buffer, buffer_size, &state->caret, (char)('0' + idx));
      }
      digitLocked[idx] = pressed;
    }

    // MINUS -> '_': niente gestione SHIFT, ma i nomi file di questo
    // progetto usano sempre l'underscore, quindi ha senso mappare il tasto
    // più vicino senza complicare l'input con i modificatori.
    static bool underscoreLocked = false;
    bool underscorePressed =
        glfwGetKey(app->window, GLFW_KEY_MINUS) == GLFW_PRESS;
    if (underscorePressed && !underscoreLocked) {
      ui_text_insert(buffer, buffer_size, &state->caret, '_');
    }
    underscoreLocked = underscorePressed;

    static bool leftLocked = false, rightLocked = false, homeLocked = false,
                endLocked = false, deleteLocked = false, allLocked = false;
    bool left = glfwGetKey(app->window, GLFW_KEY_LEFT) == GLFW_PRESS;
    bool right = glfwGetKey(app->window, GLFW_KEY_RIGHT) == GLFW_PRESS;
    bool home = glfwGetKey(app->window, GLFW_KEY_HOME) == GLFW_PRESS;
    bool end = glfwGetKey(app->window, GLFW_KEY_END) == GLFW_PRESS;
    bool del = glfwGetKey(app->window, GLFW_KEY_DELETE) == GLFW_PRESS;
    bool ctrl = glfwGetKey(app->window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                glfwGetKey(app->window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    bool all = ctrl && glfwGetKey(app->window, GLFW_KEY_A) == GLFW_PRESS;
    int len = (int)strlen(buffer);
    if (left && !leftLocked && state->caret > 0) state->caret--;
    if (right && !rightLocked && state->caret < len) state->caret++;
    if (home && !homeLocked) state->caret = 0;
    if (end && !endLocked) state->caret = len;
    if (del && !deleteLocked && state->caret < len)
      memmove(buffer + state->caret, buffer + state->caret + 1,
              (size_t)(len - state->caret));
    if (all && !allLocked) { buffer[0] = '\0'; state->caret = 0; }
    leftLocked = left; rightLocked = right; homeLocked = home; endLocked = end;
    deleteLocked = del; allLocked = all;
  }

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
    draw_hud_paper_panel(app,x,y,w,h,color,accent,state->focused,ui_hash_id(id));
  } else {
    if (border > 0.0f)
      draw_quad(app, x - border, y - border, w + border * 2, h + border * 2,
                borderColor.r, borderColor.g, borderColor.b);
    draw_quad(app, x, y, w, h, fill.r, fill.g, fill.b);
  }

  const char *shown = buffer;
  float availableTextW = w - padding_x * 2.0f;
  while (*shown && measure_text_width(font, shown, scale) > availableTextW)
    shown++;
  draw_text_colored(app, font, shown, x + padding_x, y + (h + ascent) * 0.5f,
                    scale, theme.text.r, theme.text.g, theme.text.b);
  if (state->focused) {
    char prefix[256];
    int caret = state->caret < 0 ? 0 : state->caret;
    int hidden = (int)(shown - buffer);
    if (caret < hidden) caret = hidden;
    snprintf(prefix, sizeof(prefix), "%.*s", caret - hidden, shown);
    float caretX = x + padding_x + measure_text_width(font, prefix, scale);
    draw_quad(app, caretX, y + padding_y, theme.border_thickness,
              h - padding_y * 2.0f, theme.accent.r, theme.accent.g,
              theme.accent.b);
  }

  return confirmed;
}

typedef struct {
  char id[128];
  bool open;
  unsigned long long last_seen_frame;
  int scrollOffset; // indice della prima opzione visibile, quando la
                    // lista ha più righe di quante ne stiano a schermo

  // Ultima geometria VERA calcolata da ui_dropdown per questo dropdown
  // — usata da ui_dropdown_row_position per allineare un sottomenu a
  // cascata esattamente alla riga che lo ha aperto, invece di
  // ricalcolare (e rischiare di disallineare) la stessa geometria da
  // fuori. Aggiornata ad ogni chiamata di ui_dropdown, aperto o chiuso
  // che sia.
  float lastX, lastRowsTop, lastRowH;

  // Geometria della LISTA completa (non solo le righe, vedi
  // lastRowsTop sopra) — larghezza, inizio verticale, altezza totale.
  // Usata dal controllo di priorità in ui_begin_frame: un dropdown
  // APERTO la cui lista si sovrappone a un bottone disegnato più in
  // basso non deve lasciar passare il click anche a quel bottone.
  float lastWidth, lastListTop, lastListH;

  // I dati della lista vengono copiati per il pass overlay. Non conserviamo
  // puntatori del chiamante: molte call-site compongono label su stack e il
  // popup viene disegnato quando quelle funzioni sono già ritornate.
  bool popupSubmitted;
  bool popupOpenRequested;
  int popupOrder;
  Font *popupFont;
  float popupScale, popupPaddingX, popupAscent, popupX, popupW;
  float popupRowH, popupListTop, popupListH, popupRowsTop, popupBorder;
  int popupVisibleRows, popupSelectedIndex;
  bool popupNeedsScroll, popupOpenUpward, popupHasRealSelection;
  bool popupCanScrollUp, popupCanScrollDown;
  UIColor popupNormal, popupHover, popupPressed, popupBorderColor;
  char popupOptions[128][128];
} UIDropdownState;

#define MAX_UI_DROPDOWNS 32
static UIDropdownState dropdown_states[MAX_UI_DROPDOWNS];
static int dropdown_state_count = 0;
static UIDropdownState *g_dropdown_click_owner = NULL;
static int g_dropdown_submission_serial = 0;
static bool g_dropdown_popup_pass_drawn = false;

static void ui_close_all_dropdowns(void) {
  for (int i = 0; i < dropdown_state_count; ++i)
    dropdown_states[i].open = false;
  g_dropdown_click_owner = NULL;
}

static void ui_reset_dropdown_popup_pass(void) {
  g_dropdown_submission_serial = 0;
  g_dropdown_popup_pass_drawn = false;
  for (int i = 0; i < dropdown_state_count; ++i) {
    /* popupSubmitted/OpenRequested still describe the completed previous
     * frame here. An open widget that was not emitted there is stale state,
     * not current ownership. */
    if (dropdown_states[i].open && !dropdown_states[i].popupSubmitted &&
        !dropdown_states[i].popupOpenRequested)
      dropdown_states[i].open = false;
    dropdown_states[i].popupSubmitted =
        dropdown_states[i].popupOpenRequested = false;
  }
}

// Definizione vera — dichiarata in anticipo vicino a
// g_click_claimed_by_dropdown_list, chiamata da ui_begin_frame.
// Controlla TUTTI i dropdown attualmente aperti: se il click di
// questo frame cade dentro la lista aperta di uno qualunque di loro,
// marca il click come "già rivendicato" — letto poi da
// ui_button_labeled (e simili) per non reagire allo stesso click.
static void ui_precheck_dropdown_click_priority(void) {
  g_click_claimed_by_dropdown_list = false;
  g_dropdown_click_owner = NULL;
  if (!g_mouse_clicked)
    return;

  // L'overlay con ordine più alto del frame precedente possiede anche il
  // click corrente. Qualunque click mentre un popup è aperto viene consumato:
  // fuori chiude il popup, ma non attiva il controllo sottostante.
  int topOrder = -1;
  for (int i = 0; i < dropdown_state_count; i++) {
    UIDropdownState *s = &dropdown_states[i];
    if (!s->open || s->lastListH <= 0.0f)
      continue;
    if (s->popupOrder > topOrder) {
      topOrder = s->popupOrder;
      g_dropdown_click_owner = s;
    }
  }
  g_click_claimed_by_dropdown_list = g_dropdown_click_owner != NULL;
}

static UIDropdownState *find_or_create_dropdown_state(const char *id) {
  for (int i = 0; i < dropdown_state_count; i++) {
    if (strcmp(dropdown_states[i].id, id) == 0) {
      dropdown_states[i].last_seen_frame = g_ui_frame_serial;
      return &dropdown_states[i];
    }
  }
  if (dropdown_state_count < MAX_UI_DROPDOWNS) {
    UIDropdownState *s = &dropdown_states[dropdown_state_count++];
    memset(s, 0, sizeof(*s));
    snprintf(s->id, sizeof(s->id), "%s", id);
    s->last_seen_frame = g_ui_frame_serial;
    return s;
  }
  UIDropdownState *reuse = NULL;
  for (int i = 0; i < dropdown_state_count; ++i) {
    UIDropdownState *candidate = &dropdown_states[i];
    if (candidate->open || candidate->popupSubmitted ||
        candidate->popupOpenRequested ||
        candidate->last_seen_frame == g_ui_frame_serial)
      continue;
    if (!reuse || candidate->last_seen_frame < reuse->last_seen_frame)
      reuse = candidate;
  }
  if (reuse) {
    if (g_dropdown_click_owner == reuse)
      g_dropdown_click_owner = NULL;
    memset(reuse, 0, sizeof(*reuse));
    snprintf(reuse->id, sizeof(reuse->id), "%s", id);
    reuse->last_seen_frame = g_ui_frame_serial;
    return reuse;
  }
  static bool warned = false;
  if (!warned) {
    LOG_WARN("UI: troppi dropdown simultanei (max %d) — id '%s' ignorato",
             MAX_UI_DROPDOWNS, id);
    warned = true;
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
    if (!open && g_dropdown_click_owner == state)
      g_dropdown_click_owner = NULL;
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
  float spaceBelow = VIRTUAL_H - 10.0f - (y + h);
  float spaceAbove = y - 10.0f;
  bool openUpward = (listH > spaceBelow) && (spaceAbove > spaceBelow);

  float top = openUpward ? (y - listH) : (y + h);
  if (top < 10.0f)
    top = 10.0f;
  if (top + listH > VIRTUAL_H - 10.0f)
    top = VIRTUAL_H - 10.0f - listH;
  if (top < 10.0f) // lista più alta dello schermo intero: resta ancorata
    top = 10.0f;  // in cima invece di sforare sopra

  if (out_open_upward)
    *out_open_upward = openUpward;
  return top;
}

#define UI_DROPDOWN_SCROLL_ARROW_H 34.0f
#define UI_DROPDOWN_SCROLL_GAP 6.0f

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
  float availH = VIRTUAL_H - 20.0f;
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
  for (int i = 0; i < dropdown_state_count; i++) {
    if (strcmp(dropdown_states[i].id, id) == 0)
      return dropdown_states[i].open;
  }
  return false;
}

static bool ui_any_dropdown_is_open(void) {
  for (int i = 0; i < dropdown_state_count; ++i) {
    if (dropdown_states[i].open)
      return true;
  }
  return false;
}

bool ui_dropdown(APP *app, const char *id, Font *font, TextAnchor anchor,
                 float margin, float scale, float offset_x, float offset_y,
                 float width, const char *const *options, int option_count,
                 int *selected_index, const char *header_label,
                 const UIButtonStyle *style) {
  UIDropdownState *state = find_or_create_dropdown_state(id);
  if (!state || !options || option_count <= 0 || !selected_index)
    return false;

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
  float ascent = font->ascender * scale;

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
    x = VIRTUAL_W - margin - w;
    y = margin;
    break;
  case ANCHOR_BOTTOM_LEFT:
    x = margin;
    y = VIRTUAL_H - margin - h;
    break;
  case ANCHOR_BOTTOM_RIGHT:
    x = VIRTUAL_W - margin - w;
    y = VIRTUAL_H - margin - h;
    break;
  case ANCHOR_CENTER:
    x = (VIRTUAL_W - w) * 0.5f;
    y = (VIRTUAL_H - h) * 0.5f;
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
  if (x + w > VIRTUAL_W - 10.0f)
    x = VIRTUAL_W - 10.0f - w;
  if (x < 10.0f)
    x = 10.0f;

  bool headerHover = ui_pointer_hit(x, y, w, h);
  if (headerHover)
    g_any_hover = true;

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
      g_any_hover = true;
    if (needsScroll) {
      upArrowHover = ui_pointer_hit(x, listTop, w,
                                   UI_DROPDOWN_SCROLL_ARROW_H);
      downArrowHover = ui_pointer_hit(x,
                                     listTop + fullListH - UI_DROPDOWN_SCROLL_ARROW_H,
                                     w, UI_DROPDOWN_SCROLL_ARROW_H);
    }
  }

  // Stesso fix di ui_text_input: fronte di click condiviso da ui_begin_frame,
  // non una static locale — quella faceva sì che il dropdown SLOT (disegnato
  // per primo, nella toolbar) consumasse il click prima che il dropdown
  // TEXTURE (disegnato dopo, nel pannello combat) potesse vederlo nello
  // stesso frame.
  bool clickedNow = g_mouse_clicked &&
                    (!g_dropdown_click_owner ||
                     g_dropdown_click_owner == state);

  int clickedOption = -1;
  if (clickedNow) {
    if (headerHover) {
      state->open = !state->open;
      g_dropdown_click_owner = state;
      g_click_claimed_by_dropdown_list = true;
    } else if (state->open && needsScroll && upArrowHover) {
      if (state->scrollOffset > 0)
        state->scrollOffset--;
    } else if (state->open && needsScroll && downArrowHover) {
      if (state->scrollOffset < maxScroll)
        state->scrollOffset++;
    } else if (state->open && listHover) {
      int rel = (int)((g_mouse_y - rowsTop) / rowH);
      if (rel >= 0 && rel < visibleRows) {
        int optIdx = state->scrollOffset + rel;
        if (optIdx >= 0 && optIdx < option_count)
          clickedOption = optIdx;
      }
    } else if (state->open) {
      state->open = false; // click fuori: chiude senza selezionare
    }
  }

  // ESC chiude la lista aperta senza cambiare la selezione: il dropdown
  // tiene la cattura della tastiera (ui_has_keyboard_capture) quindi deve
  // anche rilasciarla con lo stesso tasto usato da menu e modali.
  if (state->open && ui_consume_escape())
    state->open = false;

  bool changed = false;
  if (clickedOption >= 0) {
    *selected_index = clickedOption;
    changed = true;
    state->open = false;
  }

  state->lastListH = state->open ? fullListH : 0.0f;

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
    draw_hud_paper_panel(app,x,y,w,h,fill,accent,headerHover || state->open,ui_hash_id(id));
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
    draw_quad(app, x - border, y - border, w + border * 2, h + border * 2,
              br, bg, bb);
  }
  if (!theme.paper_panels)
    draw_quad(app, x, y, w, h, headerFill.r, headerFill.g, headerFill.b);

  char displayHeaderLabel[192];
  ui_humanize_label(header_label ? header_label : options[*selected_index],
                    displayHeaderLabel, sizeof(displayHeaderLabel));
  char headerText[192];
  ui_ellipsize_left(font, displayHeaderLabel, scale,
                    w - padding_x * 2.0f - 18.0f, headerText,
                    sizeof(headerText));
  draw_text_colored(app, font, headerText, x + padding_x,
                    y + (h + ascent) * 0.5f, scale, theme.text.r, theme.text.g,
                    theme.text.b);
  const char *arrow = state->open ? "^" : "v";
  float arrow_w = measure_text_width(font, arrow, scale);
  draw_text_colored(app, font, arrow, x + w - padding_x - arrow_w,
                    y + (h + ascent) * 0.5f, scale, theme.text_muted.r,
                    theme.text_muted.g, theme.text_muted.b);

  // La lista non viene più emessa qui: i controlli chiamati dopo questo
  // dropdown la coprirebbero. L'istantanea qui sotto è autonoma e viene
  // disegnata nel pass overlay al termine dell'HUD.
  state->popupSubmitted = state->open;
  if (state->popupSubmitted) {
    state->popupOrder = g_dropdown_submission_serial++;
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
    state->popupVisibleRows = visibleRows > 128 ? 128 : visibleRows;
    state->popupSelectedIndex = *selected_index;
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

  return changed;
}

static void ui_draw_one_dropdown_popup(APP *app,
                                       const UIDropdownState *state) {
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
    draw_quad(app, x, listTop, w, UI_DROPDOWN_SCROLL_ARROW_H, upFill.r,
              upFill.g, upFill.b);
    draw_quad(app, x, downY, w, UI_DROPDOWN_SCROLL_ARROW_H, downFill.r,
              downFill.g, downFill.b);
    const float arrowScale = 16.0f;
    const float arrowAscent = state->popupFont->ascender * arrowScale;
    const float upW = measure_text_width(state->popupFont, "^", arrowScale);
    const float downW = measure_text_width(state->popupFont, "v", arrowScale);
    draw_text_colored(app, state->popupFont, "^", x + (w - upW) * 0.5f,
                      listTop + (UI_DROPDOWN_SCROLL_ARROW_H + arrowAscent) * 0.5f,
                      arrowScale, theme.text.r, theme.text.g, theme.text.b);
    draw_text_colored(app, state->popupFont, "v", x + (w - downW) * 0.5f,
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
    draw_quad(app, x, rowY, w, rowH, rowFill.r, rowFill.g, rowFill.b);
    draw_text_colored(app, state->popupFont, state->popupOptions[i],
                      x + state->popupPaddingX,
                      rowY + (rowH + state->popupAscent) * 0.5f,
                      state->popupScale, theme.text.r, theme.text.g,
                      theme.text.b);
  }

  if (state->popupBorder > 0.0f) {
    UIColor border = state->popupBorderColor;
    draw_quad(app, x - state->popupBorder, listTop, state->popupBorder,
              listH, border.r, border.g, border.b);
    draw_quad(app, x + w, listTop, state->popupBorder, listH, border.r,
              border.g, border.b);
    float farEdgeY = state->popupOpenUpward ? listTop : listTop + listH;
    draw_quad(app, x, farEdgeY, w, state->popupBorder, border.r, border.g,
              border.b);
  }
}

void ui_draw_dropdown_popups(APP *app) {
  if (g_dropdown_popup_pass_drawn)
    return;
  g_dropdown_popup_pass_drawn = true;

  // Chiudi stato rimasto da una schermata che non ha più emesso quel widget.
  // Un'apertura programmatica nello stesso frame (sottomenu FILE) è invece
  // intenzionale e viene conservata fino alla sua prima draw nel frame dopo.
  for (int i = 0; i < dropdown_state_count; ++i) {
    UIDropdownState *state = &dropdown_states[i];
    if (state->open && !state->popupSubmitted && !state->popupOpenRequested)
      state->open = false;
  }

  for (int order = 0; order < g_dropdown_submission_serial; ++order) {
    for (int i = 0; i < dropdown_state_count; ++i) {
      UIDropdownState *state = &dropdown_states[i];
      if (state->open && state->popupSubmitted && state->popupOrder == order)
        ui_draw_one_dropdown_popup(app, state);
    }
  }
}

/* UIModalOpenState / g_modal_states: dichiarati in cima (servono a ui_begin_frame). */

void ui_release_input_capture(void) {
  for (int i = 0; i < ui_state_count; ++i)
    ui_states[i].is_pressed = false;
  for (int i = 0; i < slider_state_count; ++i)
    slider_states[i].dragging = false;
  for (int i = 0; i < g_scroll_state_count; ++i)
    g_scroll_states[i].dragging = false;
  for (int i = 0; i < text_input_state_count; ++i)
    text_input_states[i].focused = false;
  for (int i = 0; i < float_field_state_count; ++i)
    float_field_states[i].focused = false;
  ui_close_all_dropdowns();
}

/* Dropdown e campi editabili sono capture transitorie, non dialoghi: F1/F2
 * e Ctrl+S/Ctrl+L possono chiuderli in modo esplicito. Questi tre sistemi,
 * invece, rappresentano una scelta ancora pendente e restano esclusivi. */
bool ui_has_blocking_overlay(void) {
  if (ui_any_modal_open() || ui_file_picker_capture_active())
    return true;
#if 1 /* Generic menu support. */
  return ui_menu_hierarchy_is_open();
#else
  return false;
#endif
}

static void ui_modal_input_begin_frame(void) {
  for (int i = 0; i < g_modal_state_count; ++i)
    g_modal_states[i].submittedThisFrame = false;
}

static void ui_modal_input_end_frame(void) {
  bool closed_stale_modal = false;
  for (int i = 0; i < g_modal_state_count; ++i) {
    if (g_modal_states[i].wasOpen &&
        !g_modal_states[i].submittedThisFrame) {
      g_modal_states[i].wasOpen = false;
      closed_stale_modal = true;
    }
  }
  if (closed_stale_modal && !ui_any_modal_open() &&
      !ui_file_picker_capture_active())
    ui_release_input_capture();
}

static bool ui_any_modal_open(void) {
  for (int i = 0; i < g_modal_state_count; ++i) {
    if (g_modal_states[i].wasOpen)
      return true;
  }
  return false;
}

bool ui_has_input_capture(void) {
  return ui_has_pointer_capture() || ui_has_keyboard_capture();
}

bool ui_has_pointer_capture(void) {
  bool active_widget = false;
  for (int i = 0; i < ui_state_count; ++i)
    active_widget |= ui_states[i].is_pressed;
  for (int i = 0; i < slider_state_count; ++i)
    active_widget |= slider_states[i].dragging;
  for (int i = 0; i < g_scroll_state_count; ++i)
    active_widget |= g_scroll_states[i].dragging;
  if (active_widget || ui_any_dropdown_is_open() || ui_any_modal_open() ||
      ui_file_picker_capture_active())
    return true;
#if 1 /* Generic menu support. */
  return ui_menu_hierarchy_is_open();
#else
  return false;
#endif
}

bool ui_has_keyboard_capture(void) {
  bool float_focused = false;
  for (int i = 0; i < float_field_state_count; ++i)
    float_focused |= float_field_states[i].focused;
  if (ui_any_text_input_focused() || float_focused ||
      ui_any_dropdown_is_open() || ui_any_modal_open() ||
      ui_file_picker_capture_active())
    return true;
#if 1 /* Generic menu support. */
  return ui_menu_hierarchy_is_open();
#else
  return false;
#endif
}

void ui_modal_end(const char *id, bool is_open) {
  if (!id || !id[0])
    return;
  for (int i = 0; i < g_modal_state_count; ++i) {
    if (strcmp(g_modal_states[i].id, id) != 0)
      continue;
    bool closed_now = g_modal_states[i].wasOpen && !is_open;
    g_modal_states[i].wasOpen = is_open;
    g_modal_states[i].submittedThisFrame = is_open;
    if (g_modal_states[i].scopeOpen) {
      g_modal_states[i].scopeOpen = false;
      if (g_modal_scope_depth > 0)
        g_modal_scope_depth--;
    }
    if (closed_now && !ui_any_modal_open() &&
        !ui_file_picker_capture_active())
      ui_release_input_capture();
    return;
  }
}

bool ui_modal_begin(APP *app, const char *id, const char *title, Font *font,
                    float width, float height, bool *is_open,
                    float *out_content_x, float *out_content_y,
                    float *out_content_w, float *out_content_h) {
  // Modale sopra popup: una lista aperta nel normale HUD viene chiusa QUANDO
  // il dialogo si apre. Non farlo a ogni frame: alcuni dialoghi contengono a
  // loro volta dropdown, che devono poter restare aperti dal frame seguente.
  UIModalOpenState *modalState = NULL;
  for (int i = 0; i < g_modal_state_count; ++i) {
    if (strcmp(g_modal_states[i].id, id) == 0) {
      modalState = &g_modal_states[i];
      break;
    }
  }
  if (!modalState && g_modal_state_count < MAX_UI_MODALS) {
    modalState = &g_modal_states[g_modal_state_count++];
    memset(modalState, 0, sizeof(*modalState));
    strncpy(modalState->id, id, sizeof(modalState->id) - 1);
  }
  if (!*is_open) {
    if (modalState) {
      modalState->wasOpen = false;
      modalState->submittedThisFrame = false;
    }
    return false;
  }
  if (ui_consume_escape()) {
    *is_open = false;
    if (modalState) {
      modalState->wasOpen = false;
      modalState->submittedThisFrame = false;
    }
    if (!ui_any_modal_open() && !ui_file_picker_capture_active())
      ui_release_input_capture();
    return false;
  }
  if (!modalState || !modalState->wasOpen) {
    ui_close_all_dropdowns();
#if 1 /* Generic menu support. */
    ui_menu_close_hierarchy();
#endif
  }
  if (modalState) {
    modalState->wasOpen = true;
    modalState->submittedThisFrame = true;
    modalState->scopeOpen = true;
  }
  g_modal_scope_depth++; /* da qui fino a ui_modal_end: widget del modale */

  UITheme theme = ui_get_theme();
  float screen_margin = theme.modal_padding;
  width = fminf(width, VIRTUAL_W - screen_margin * 2.0f);
  height = fminf(height, VIRTUAL_H - screen_margin * 2.0f);
  float x = (VIRTUAL_W - width) * 0.5f;
  float y = (VIRTUAL_H - height) * 0.5f;
  float titleBarH = theme.panel_header_height + theme.row_spacing;
  float outer_border = theme.border_thickness * 2.0f;

  /* MACROTASK UI residui: sfondo SEMITRASPARENTE (draw_quad_alpha, la
   * pipeline HUD ha il blending): l'editor sotto resta visibile ma
   * scurito; i suoi widget sono inerti (ui_pointer_hit). */
  draw_quad_alpha(app, 0.0f, 0.0f, VIRTUAL_W, VIRTUAL_H,
                  theme.editor_background.r * 0.25f,
                  theme.editor_background.g * 0.25f,
                  theme.editor_background.b * 0.25f, 0.62f);

  // Rettangolo del modale visibile a ui_scroll_begin (la rotellina scorre
  // solo i pannelli dentro il modale finche' questo e' aperto).
  g_modal_rect = (UIRect){x, y, width, height};
  g_modal_rect_seen_this_frame = true;

  // Bordo esterno + corpo del pannello
  draw_quad(app, x - outer_border, y - outer_border,
            width + outer_border * 2.0f, height + outer_border * 2.0f,
            theme.border.r, theme.border.g, theme.border.b);
  draw_quad(app, x, y, width, height, theme.panel.r, theme.panel.g,
            theme.panel.b);

  // Barra del titolo, leggermente più chiara del corpo per distinguerla
  draw_quad(app, x, y, width, titleBarH, theme.panel_header.r,
            theme.panel_header.g, theme.panel_header.b);
  draw_quad(app, x, y, 3.0f, titleBarH, theme.accent.r, theme.accent.g,
            theme.accent.b);
  float titleScale = 16.0f * theme.section_text_scale;
  float titleAscent = font->ascender * titleScale;
  draw_text_colored(app, font, title, x + theme.modal_padding,
                    y + (titleBarH + titleAscent) * 0.5f, titleScale, theme.text.r,
                    theme.text.g, theme.text.b);

  // Bottone di chiusura (X), in cima a destra della barra del titolo
  char closeId[128];
  snprintf(closeId, sizeof(closeId), "%s_modal_close", id);
  float closeSize = theme.compact_button_height;
  UIButtonStyle closeStyle = {.min_width = closeSize,
                              .max_width = closeSize,
                              .min_height = closeSize,
                              .padding_x = theme.row_spacing,
                              .padding_y = theme.row_spacing,
                              .has_custom_color = true,
                              .color_normal = theme.panel_header,
                              .color_hover = theme.danger,
                              .color_pressed = theme.error,
                              .color_border = theme.border};
  if (ui_button_labeled(app, closeId, "X", font, ANCHOR_TOP_LEFT, 0, 14.0f,
                        x + width - closeSize - theme.row_spacing,
                        y + (titleBarH - closeSize) * 0.5f, &closeStyle)) {
    *is_open = false;
  }

  if (!*is_open) {
    if (modalState) {
      modalState->wasOpen = false;
      modalState->submittedThisFrame = false;
    }
    if (!ui_any_modal_open() && !ui_file_picker_capture_active())
      ui_release_input_capture();
    return false;
  }

  *out_content_x = x + theme.modal_padding;
  *out_content_y = y + titleBarH + theme.modal_padding;
  *out_content_w = width - theme.modal_padding * 2.0f;
  *out_content_h = height - titleBarH - theme.modal_padding * 2.0f;
  return true;
}

/* ------------------------------------------------------------------------- */
/* Canonical bounded file picker.  This is deliberately UI-core state rather
 * than a family of editor-specific filename popups: callers provide the
 * project root/filter and retain ownership of the resulting operation. */

#define MAX_UI_FILE_PICKER_ENTRIES 256
#define UI_PICKER_FAVORITES_MAX 24
#define UI_PICKER_RECENTS_MAX 10
typedef struct {
  char name[128];
  bool directory;
} UIFilePickerEntry;
typedef struct {
  bool open, overwrite_pending;
  char id[96], title[96];
  UIFilePickerMode mode;
  char root[APP_PATH_MAX], current[APP_PATH_MAX];
  char extension[24], filename[128], search[96], selected[APP_PATH_MAX], error[128];
  UIFilePickerEntry entries[MAX_UI_FILE_PICKER_ENTRIES];
  bool image_preview;
  APP *app;
  int entry_count, refresh_count;
  /* palette preferiti/recenti (2026-09-18), percorsi relativi alla root */
  char favorites_key[32];
  char favorites[UI_PICKER_FAVORITES_MAX][160];
  int favorite_count;
  char recents[UI_PICKER_RECENTS_MAX][160];
  int recent_count;
} UIFilePickerState;
static UIFilePickerState g_file_picker = {0};

/* --- palette preferiti/recenti -------------------------------------------- */
static bool ui_picker_palette_file(char *out, size_t size) {
  char dir[APP_PATH_MAX];
  if (!g_file_picker.favorites_key[0] || !paths_user_config_dir(dir, sizeof(dir), true))
    return false;
  int n = snprintf(out, size, "%s/picker_%s.txt", dir, g_file_picker.favorites_key);
  return n > 0 && (size_t)n < size;
}

static void ui_picker_palette_load(void) {
  g_file_picker.favorite_count = g_file_picker.recent_count = 0;
  char path[APP_PATH_MAX];
  if (!ui_picker_palette_file(path, sizeof(path)))
    return;
  FILE *f = fopen(path, "r");
  if (!f)
    return;
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
      line[--n] = '\0';
    if (n < 3 || line[1] != '|' || strstr(line + 2, "..") ||
        n - 2 >= sizeof(g_file_picker.favorites[0]))
      continue; /* riga malformata o percorso troppo lungo: ignorata */
    if (line[0] == 'F' && g_file_picker.favorite_count < UI_PICKER_FAVORITES_MAX)
      memcpy(g_file_picker.favorites[g_file_picker.favorite_count++], line + 2, n - 1);
    else if (line[0] == 'R' && g_file_picker.recent_count < UI_PICKER_RECENTS_MAX)
      memcpy(g_file_picker.recents[g_file_picker.recent_count++], line + 2, n - 1);
  }
  fclose(f);
}

static void ui_picker_palette_save(void) {
  char path[APP_PATH_MAX];
  if (!ui_picker_palette_file(path, sizeof(path)))
    return;
  FILE *f = fopen(path, "w");
  if (!f)
    return;
  for (int i = 0; i < g_file_picker.favorite_count; ++i)
    fprintf(f, "F|%s\n", g_file_picker.favorites[i]);
  for (int i = 0; i < g_file_picker.recent_count; ++i)
    fprintf(f, "R|%s\n", g_file_picker.recents[i]);
  fclose(f);
}

/* percorso assoluto -> relativo alla root del picker ("" se fuori root) */
static const char *ui_picker_relative(const char *absolute) {
  size_t rl = strlen(g_file_picker.root);
  if (strncmp(absolute, g_file_picker.root, rl) != 0 || absolute[rl] != '/')
    return "";
  return absolute + rl + 1;
}

static int ui_picker_favorite_index(const char *relative) {
  for (int i = 0; i < g_file_picker.favorite_count; ++i)
    if (strcmp(g_file_picker.favorites[i], relative) == 0)
      return i;
  return -1;
}

static void ui_picker_favorite_toggle(const char *relative) {
  if (!relative || !relative[0])
    return;
  int idx = ui_picker_favorite_index(relative);
  if (idx >= 0) {
    for (int i = idx; i + 1 < g_file_picker.favorite_count; ++i)
      memcpy(g_file_picker.favorites[i], g_file_picker.favorites[i + 1],
             sizeof(g_file_picker.favorites[0]));
    g_file_picker.favorite_count--;
  } else if (g_file_picker.favorite_count < UI_PICKER_FAVORITES_MAX) {
    snprintf(g_file_picker.favorites[g_file_picker.favorite_count++],
             sizeof(g_file_picker.favorites[0]), "%s", relative);
  }
  ui_picker_palette_save();
}

static void ui_picker_recent_push(const char *relative) {
  if (!g_file_picker.favorites_key[0] || !relative || !relative[0])
    return;
  /* rimuovi un duplicato, poi inserisci in testa */
  for (int i = 0; i < g_file_picker.recent_count; ++i) {
    if (strcmp(g_file_picker.recents[i], relative) == 0) {
      for (int j = i; j + 1 < g_file_picker.recent_count; ++j)
        memcpy(g_file_picker.recents[j], g_file_picker.recents[j + 1],
               sizeof(g_file_picker.recents[0]));
      g_file_picker.recent_count--;
      break;
    }
  }
  if (g_file_picker.recent_count < UI_PICKER_RECENTS_MAX)
    g_file_picker.recent_count++;
  for (int j = g_file_picker.recent_count - 1; j > 0; --j)
    memcpy(g_file_picker.recents[j], g_file_picker.recents[j - 1],
           sizeof(g_file_picker.recents[0]));
  snprintf(g_file_picker.recents[0], sizeof(g_file_picker.recents[0]), "%s", relative);
  ui_picker_palette_save();
}

static void ui_file_picker_refresh(UIFilePickerState *picker);

/* Seleziona un percorso relativo della palette: naviga nella sua cartella e
 * lo evidenzia (come un click sulla riga). */
static void ui_picker_select_relative(const char *relative) {
  char absolute[APP_PATH_MAX];
  int n = snprintf(absolute, sizeof(absolute), "%s/%s", g_file_picker.root, relative);
  if (n <= 0 || (size_t)n >= sizeof(absolute))
    return;
  if (!platform_file_exists(absolute)) {
    snprintf(g_file_picker.error, sizeof(g_file_picker.error), "File non trovato: %.100s", relative);
    return;
  }
  const char *slash = strrchr(relative, '/');
  if (slash) {
    char dir[160];
    size_t dl = (size_t)(slash - relative);
    if (dl >= sizeof(dir)) dl = sizeof(dir) - 1;
    memcpy(dir, relative, dl);
    dir[dl] = '\0';
    int w = snprintf(g_file_picker.current, sizeof(g_file_picker.current), "%s/%s",
                     g_file_picker.root, dir);
    if (w < 0 || (size_t)w >= sizeof(g_file_picker.current))
      snprintf(g_file_picker.current, sizeof(g_file_picker.current), "%s", g_file_picker.root);
  } else {
    snprintf(g_file_picker.current, sizeof(g_file_picker.current), "%s", g_file_picker.root);
  }
  snprintf(g_file_picker.filename, sizeof(g_file_picker.filename), "%.127s", slash ? slash + 1 : relative);
  snprintf(g_file_picker.selected, sizeof(g_file_picker.selected), "%s", absolute);
  ui_file_picker_refresh(&g_file_picker);
}

static bool ui_file_picker_capture_active(void) {
  return g_file_picker.open || g_file_picker.overwrite_pending;
}

void ui_file_picker_cancel(void) {
  char picker_id[sizeof(g_file_picker.id)];
  snprintf(picker_id, sizeof(picker_id), "%s", g_file_picker.id);
  if (g_file_picker.image_preview && g_file_picker.app)
    texture_manager_preview_clear(g_file_picker.app);
  g_file_picker.open = false;
  g_file_picker.overwrite_pending = false;
  ui_modal_end("file_picker_overwrite", false);
  if (picker_id[0])
    ui_modal_end(picker_id, false);
  if (!ui_any_modal_open())
    ui_release_input_capture();
}

static bool ui_file_picker_accepts(const UIFilePickerState *picker,
                                   const char *name, bool directory) {
  if (directory)
    return true;
  if (picker->extension[0]) {
    size_t name_len = strlen(name), ext_len = strlen(picker->extension);
    if (name_len < ext_len || strcmp(name + name_len - ext_len,
                                     picker->extension) != 0)
      return false;
  }
  if (picker->search[0] && !strstr(name, picker->search))
    return false;
  return true;
}

static int ui_file_picker_entry_compare(const void *left, const void *right) {
  const UIFilePickerEntry *a = left, *b = right;
  if (a->directory != b->directory)
    return a->directory ? -1 : 1;
  return strcmp(a->name, b->name);
}

static void ui_file_picker_refresh(UIFilePickerState *picker) {
  picker->entry_count = 0;
  picker->error[0] = '\0';
  PlatformDirIter directory;
  if (!platform_dir_open(&directory, picker->current)) {
    /* Il PERCORSO nel messaggio, non solo il fatto: "Cartella non
     * accessibile" da sola non dice se il problema e' la cartella di lavoro,
     * un errore di composizione del percorso o i permessi. Con il percorso
     * sott'occhio la diagnosi e' immediata. */
    snprintf(picker->error, sizeof(picker->error),
             "Cartella non accessibile: %.70s", picker->current);
    return;
  }
  PlatformDirEntry entry;
  while (platform_dir_next(&directory, &entry)) {
    char full_path[APP_PATH_MAX];
    int written = snprintf(full_path, sizeof(full_path), "%s/%s",
                           picker->current, entry.name);
    if (written < 0 || (size_t)written >= sizeof(full_path))
      continue;
    bool is_directory = platform_dir_entry_is_dir(picker->current, &entry);
    /* Una voce che non e' ne' cartella ne' file regolare (link rotto,
     * socket) non deve comparire nel selettore: prima la scartava stat()
     * fallendo, ora serve dirlo esplicitamente. */
    if (!is_directory && !platform_file_exists(full_path))
      continue;
    if (!ui_file_picker_accepts(picker, entry.name, is_directory))
      continue;
    if (picker->entry_count >= MAX_UI_FILE_PICKER_ENTRIES) {
      snprintf(picker->error, sizeof(picker->error),
               "Elenco limitato a %d elementi", MAX_UI_FILE_PICKER_ENTRIES);
      break;
    }
    UIFilePickerEntry *out = &picker->entries[picker->entry_count++];
    size_t entry_name_length = strlen(entry.name);
    if (entry_name_length >= sizeof(out->name))
      entry_name_length = sizeof(out->name) - 1;
    memcpy(out->name, entry.name, entry_name_length);
    out->name[entry_name_length] = '\0';
    out->directory = is_directory;
  }
  platform_dir_close(&directory);
  qsort(picker->entries, (size_t)picker->entry_count, sizeof(picker->entries[0]),
        ui_file_picker_entry_compare);
  ++picker->refresh_count;
}

static bool ui_file_picker_path_is_inside_root(const UIFilePickerState *picker,
                                               const char *path) {
  size_t root_length = strlen(picker->root);
  return root_length > 0 && strncmp(path, picker->root, root_length) == 0 &&
         (path[root_length] == '\0' || path[root_length] == '/');
}

static bool ui_file_picker_build_path(const UIFilePickerState *picker,
                                      const char *name, char *out,
                                      size_t out_size) {
  if (!name || !name[0])
    return false;
  int written = snprintf(out, out_size, "%s/%s", picker->current, name);
  return written >= 0 && (size_t)written < out_size &&
         ui_file_picker_path_is_inside_root(picker, out);
}

void ui_file_picker_open(const char *id, const char *title,
                         const UIFilePickerRequest *request) {
  if (!id || !request || !request->root_directory || !request->root_directory[0])
    return;
  if (ui_file_picker_capture_active())
    ui_file_picker_cancel();
  memset(&g_file_picker, 0, sizeof(g_file_picker));
  snprintf(g_file_picker.id, sizeof(g_file_picker.id), "%s", id);
  snprintf(g_file_picker.title, sizeof(g_file_picker.title), "%s",
           title ? title : "FILE");
  snprintf(g_file_picker.root, sizeof(g_file_picker.root), "%s",
           request->root_directory);
  snprintf(g_file_picker.current, sizeof(g_file_picker.current), "%s",
           request->initial_directory && request->initial_directory[0]
               ? request->initial_directory : request->root_directory);
  snprintf(g_file_picker.extension, sizeof(g_file_picker.extension), "%s",
           request->extension ? request->extension : "");
  snprintf(g_file_picker.filename, sizeof(g_file_picker.filename), "%s",
           request->initial_name ? request->initial_name : "");
  g_file_picker.mode = request->mode;
  g_file_picker.image_preview = request->image_preview;
  snprintf(g_file_picker.favorites_key, sizeof(g_file_picker.favorites_key), "%s",
           request->favorites_key ? request->favorites_key : "");
  ui_picker_palette_load();
  g_file_picker.open = true;
  ui_file_picker_refresh(&g_file_picker);
}

bool ui_file_picker_selected_path(const char *id, char *out_path, size_t out_path_size) {
  if (!ui_file_picker_is_open(id) || !out_path || out_path_size == 0 ||
      !g_file_picker.selected[0])
    return false;
  snprintf(out_path, out_path_size, "%s", g_file_picker.selected);
  return true;
}

bool ui_file_picker_is_open(const char *id) {
  return id && g_file_picker.open && strcmp(g_file_picker.id, id) == 0;
}

static bool ui_file_picker_confirm_overwrite(APP *app, Font *font,
                                             char *out_path,
                                             size_t out_path_size) {
  if (!g_file_picker.overwrite_pending)
    return false;
  bool confirm_open = true;
  float x, y, w, h;
  if (!ui_modal_begin(app, "file_picker_overwrite", "SOVRASCRIVERE FILE?",
                      font, 460.0f, 150.0f, &confirm_open, &x, &y, &w, &h)) {
    g_file_picker.overwrite_pending = false;
    ui_modal_end("file_picker_overwrite", false);
    return false;
  }
  UILayout layout = ui_layout_begin((UIRect){x, y, w, h}, 1.0f);
  UITheme theme = ui_get_theme();
  draw_text_colored(app, font, "Il file esiste gia'. Questa azione non e' annullabile.",
                    layout.bounds.x, layout.cursor_y + 16.0f, 13.0f,
                    theme.text_muted.r, theme.text_muted.g,
                    theme.text_muted.b);
  layout.cursor_y += theme.row_height + theme.row_spacing;
  UIRow row = ui_row_begin(&layout, 2);
  UIRect cancel = ui_row_next(&row), overwrite = ui_row_next(&row);
  UIButtonStyle cancel_style = {.min_width = cancel.w,
                                .max_width = cancel.w,
                                .min_height = cancel.h};
  UIButtonStyle overwrite_style = {.min_width = overwrite.w,
                                   .max_width = overwrite.w,
                                   .min_height = overwrite.h,
                                   .danger = true};
  unsigned action = 0;
  if (ui_button_labeled(app, "overwrite_cancel", "ANNULLA", font,
                        ANCHOR_TOP_LEFT, 0, 14.0f, cancel.x, cancel.y,
                        &cancel_style))
    action |= 1u;
  if (ui_button_labeled(app, "overwrite_confirm", "SOVRASCRIVI", font,
                        ANCHOR_TOP_LEFT, 0, 14.0f, overwrite.x, overwrite.y,
                        &overwrite_style))
    action |= 2u;
  if (action & 1u) {
    g_file_picker.overwrite_pending = false;
    ui_modal_end("file_picker_overwrite", false);
  } else if (action & 2u) {
    snprintf(out_path, out_path_size, "%s", g_file_picker.selected);
    ui_file_picker_cancel();
    return true;
  }
  ui_modal_end("file_picker_overwrite", g_file_picker.overwrite_pending);
  return false;
}

bool ui_file_picker_draw(APP *app, Font *font, char *out_path,
                         size_t out_path_size) {
  if (!g_file_picker.open || !out_path || out_path_size == 0)
    return false;
  g_file_picker.app = app;
  if (g_file_picker.overwrite_pending)
    return ui_file_picker_confirm_overwrite(app, font, out_path,
                                            out_path_size);

  bool open = true;
  float x, y, width, height;
  if (!ui_modal_begin(app, g_file_picker.id, g_file_picker.title, font,
                      760.0f, 620.0f, &open, &x, &y, &width, &height)) {
    ui_file_picker_cancel();
    return false;
  }
  UIPanel panel = ui_panel_begin(app, "file_picker_panel", "", font,
                                 (UIRect){x, y, width, height}, 1.0f);
  UILayout top = panel.layout;
  UITheme theme = ui_get_theme();
  UIPropertyLayout properties = ui_property_layout_begin(
      top.bounds.x, top.cursor_y, top.bounds.w, 1.0f);
  const char *navigation[] = {"ROOT", "SU", "AGGIORNA"};
  unsigned navigation_action = ui_property_action_row(
      app, &properties, "navigation", font, navigation, 3);
  if (navigation_action & 1u) {
    snprintf(g_file_picker.current, sizeof(g_file_picker.current), "%s",
             g_file_picker.root);
    ui_file_picker_refresh(&g_file_picker);
  }
  if (navigation_action & 2u) {
    char *slash = strrchr(g_file_picker.current, '/');
    if (slash && slash != g_file_picker.current) {
      *slash = '\0';
      if (!ui_file_picker_path_is_inside_root(&g_file_picker,
                                              g_file_picker.current))
        snprintf(g_file_picker.current, sizeof(g_file_picker.current), "%s",
                 g_file_picker.root);
      ui_file_picker_refresh(&g_file_picker);
    }
  }
  if (navigation_action & 4u)
    ui_file_picker_refresh(&g_file_picker);
  bool path_changed = ui_property_text(app, &properties, "directory", font,
                                       "PERCORSO", g_file_picker.current,
                                       sizeof(g_file_picker.current));
  if (path_changed) {
    if (!ui_file_picker_path_is_inside_root(&g_file_picker,
                                            g_file_picker.current))
      snprintf(g_file_picker.current, sizeof(g_file_picker.current), "%s",
               g_file_picker.root);
    ui_file_picker_refresh(&g_file_picker);
  }
  if (ui_property_text(app, &properties, "search", font, "CERCA",
                       g_file_picker.search, sizeof(g_file_picker.search)))
    ui_file_picker_refresh(&g_file_picker);
  ui_property_layout_commit(&top, &properties);

  char filter_label[96];
  snprintf(filter_label, sizeof(filter_label), "%d elementi  |  filtro: %s",
           g_file_picker.entry_count,
           g_file_picker.extension[0] ? g_file_picker.extension : "tutti i file");
  float filter_scale = 12.0f;
  float filter_width = measure_text_width(font, filter_label, filter_scale);
  draw_text_colored(app, font, filter_label,
                    top.bounds.x + top.bounds.w - filter_width,
                    top.cursor_y + font->ascender * filter_scale,
                    filter_scale, theme.text_muted.r,
                    theme.text_muted.g, theme.text_muted.b);
  top.cursor_y += theme.compact_button_height;

  int footer_rows = g_file_picker.mode == UI_FILE_PICKER_SAVE ? 2 : 1;
  float action_height = footer_rows * (theme.row_height + theme.row_spacing) +
                        (g_file_picker.error[0] ? theme.compact_button_height
                                                : 0.0f);
  bool palette = g_file_picker.favorites_key[0] != '\0';
  float preview_gap = (g_file_picker.image_preview || palette) ? theme.row_spacing : 0.0f;
  float preview_width = g_file_picker.image_preview
                            ? fminf(260.0f, top.bounds.w * 0.38f)
                            : (palette ? fminf(280.0f, top.bounds.w * 0.40f) : 0.0f);
  UIRect list_bounds = {top.bounds.x, top.cursor_y,
                        top.bounds.w - preview_width - preview_gap,
                        fmaxf(0.0f, top.bounds.y + top.bounds.h - top.cursor_y -
                                           action_height)};
  UIScrollScope list = ui_scroll_begin("file_entries", list_bounds, 1.0f);
  UIPropertyLayout rows = ui_property_layout_begin(
      list.layout.bounds.x, list.layout.cursor_y, list.layout.bounds.w, 1.0f);
  for (int i = 0; i < g_file_picker.entry_count; ++i) {
    UIFilePickerEntry *entry = &g_file_picker.entries[i];
    ui_push_id(entry->name);
    float row_y = ui_property_layout_next_row(&rows);
    // BUGFIX (segnalato dall'utente): ui_button_labeled testa il click
    // sulle coordinate ASSOLUTE del bottone, senza mai consultare il
    // clip rect attivo (ui_clip_push qui sopra e' puramente visivo, un
    // limite generale di questo framework UI, non solo del file picker).
    // Con piu' voci di quante ne stanno nel riquadro visibile, le righe
    // scrollate FUORI dal viewport restavano comunque interattive alle
    // loro coordinate reali -- potendo finire, a seconda dello scroll,
    // sotto ad altri controlli disegnati SOTTO la lista (bottoni APRI/
    // ANNULLA, campo PERCORSO) e intercettarne il click, oppure
    // accendersi come "hover" quando scorrevano sotto al cursore fermo
    // durante lo scroll. Righe fuori dal viewport: MAI disegnate, MAI
    // testate -- il cursore avanza comunque (ui_property_layout_next_row
    // sopra), cosi' l'altezza del contenuto/la scrollbar restano corrette.
    if (row_y + rows.row_height <= list_bounds.y ||
        row_y >= list_bounds.y + list_bounds.h) {
      ui_pop_id();
      continue;
    }
    char raw_label[160], label[160], entry_path[APP_PATH_MAX];
    snprintf(raw_label, sizeof(raw_label), "%s%s",
             entry->directory ? ">  " : "   ", entry->name);
    ui_ellipsize_left(font, raw_label, 14.0f,
                      rows.width - theme.label_value_gap * 2.0f,
                      label, sizeof(label));
    bool has_entry_path = ui_file_picker_build_path(
        &g_file_picker, entry->name, entry_path, sizeof(entry_path));
    bool selected = !entry->directory && has_entry_path &&
                    !strcmp(entry_path, g_file_picker.selected);
    UIButtonStyle row_style = {
        .min_width = rows.width,
        .max_width = rows.width,
        .min_height = rows.row_height,
        .border = selected ? theme.border_thickness : -1.0f,
        .selected = selected,
        .align_left = true,
        .has_custom_color = !selected,
        .color_normal = entry->directory ? theme.panel_header : theme.panel,
        .color_hover = theme.field_hover,
        .color_pressed = theme.selected,
        .color_border = theme.border};
    bool chosen = ui_button_labeled(app, ui_scoped_id("entry"), label, font,
                                    ANCHOR_TOP_LEFT, 0, 14.0f,
                                    rows.x, row_y, &row_style);
    ui_pop_id();
    if (!chosen)
      continue;
    if (entry->directory) {
      char next[APP_PATH_MAX];
      if (ui_file_picker_build_path(&g_file_picker, entry->name, next,
                                    sizeof(next))) {
        snprintf(g_file_picker.current, sizeof(g_file_picker.current), "%s", next);
        g_file_picker.selected[0] = '\0';
        ui_file_picker_refresh(&g_file_picker);
      }
    } else {
      snprintf(g_file_picker.filename, sizeof(g_file_picker.filename), "%s",
               entry->name);
      ui_file_picker_build_path(&g_file_picker, entry->name,
                                g_file_picker.selected,
                                sizeof(g_file_picker.selected));
    }
  }
  ui_property_layout_commit(&list.layout, &rows);
  ui_scroll_end(app, &list);



  if (g_file_picker.image_preview) {
    UIRect preview = {list_bounds.x + list_bounds.w + preview_gap,
                      list_bounds.y, preview_width, list_bounds.h};
    draw_quad(app, preview.x - theme.border_thickness,
              preview.y - theme.border_thickness,
              preview.w + theme.border_thickness * 2.0f,
              preview.h + theme.border_thickness * 2.0f,
              theme.border.r, theme.border.g, theme.border.b);
    draw_quad(app, preview.x, preview.y, preview.w, preview.h,
              theme.field.r, theme.field.g, theme.field.b);
    float title_scale = 13.0f * theme.secondary_text_scale;
    draw_text_colored(app, font, "ANTEPRIMA", preview.x + theme.row_spacing,
                      preview.y + theme.row_spacing + font->ascender * title_scale,
                      title_scale, theme.text_muted.r, theme.text_muted.g,
                      theme.text_muted.b);

    if (g_file_picker.selected[0]) {
      int preview_slot = texture_manager_preview_load(app,
                                                       g_file_picker.selected);
      int texture_width = 0, texture_height = 0;
      if (preview_slot >= 0 &&
          texture_manager_get_size(preview_slot, &texture_width,
                                   &texture_height) &&
          texture_width > 0 && texture_height > 0) {
        float inner = theme.row_spacing * 2.0f;
        float area_x = preview.x + inner;
        float area_y = preview.y + theme.row_height + inner;
        float area_w = fmaxf(1.0f, preview.w - inner * 2.0f);
        float area_h = fmaxf(1.0f, preview.h - theme.row_height - inner * 2.0f);
        float draw_scale = fminf(area_w / (float)texture_width,
                                 area_h / (float)texture_height);
        float draw_w = texture_width * draw_scale;
        float draw_h = texture_height * draw_scale;
        draw_textured_quad(app, area_x + (area_w - draw_w) * 0.5f,
                           area_y + (area_h - draw_h) * 0.5f, draw_w, draw_h,
                           preview_slot, 0.0f, 0.0f, 1.0f, 1.0f);
      } else {
        draw_text_colored(app, font, "Immagine non caricabile",
                          preview.x + theme.row_spacing,
                          preview.y + preview.h * 0.5f, title_scale,
                          theme.warning.r, theme.warning.g, theme.warning.b);
      }
    } else {
      draw_text_colored(app, font, "Seleziona un PNG",
                        preview.x + theme.row_spacing,
                        preview.y + preview.h * 0.5f, title_scale,
                        theme.text_muted.r, theme.text_muted.g,
                        theme.text_muted.b);
    }
  }

  if (palette) {
    // PALETTE (2026-09-18): preferiti e recenti a destra dell'elenco; un
    // click evidenzia il file (e naviga nella sua cartella); la stella
    // aggiunge/toglie il file evidenziato dai preferiti.
    UIRect pal = {list_bounds.x + list_bounds.w + preview_gap, list_bounds.y, preview_width,
                  list_bounds.h};
    draw_quad(app, pal.x - theme.border_thickness, pal.y - theme.border_thickness,
              pal.w + theme.border_thickness * 2.0f, pal.h + theme.border_thickness * 2.0f,
              theme.border.r, theme.border.g, theme.border.b);
    draw_quad(app, pal.x, pal.y, pal.w, pal.h, theme.field.r, theme.field.g, theme.field.b);
    UIScrollScope palScroll = ui_scroll_begin("picker_palette", pal, 1.0f);
    UIPropertyLayout prow = ui_property_layout_begin(palScroll.layout.bounds.x + theme.row_spacing,
                                                     palScroll.layout.cursor_y,
                                                     palScroll.layout.bounds.w - theme.row_spacing * 2.0f,
                                                     1.0f);
    const char *selRel = g_file_picker.selected[0] ? ui_picker_relative(g_file_picker.selected) : "";
    bool isFav = selRel[0] && ui_picker_favorite_index(selRel) >= 0;
    {
      float ry = ui_property_layout_next_row(&prow);
      UIButtonStyle st = {.min_width = prow.width, .max_width = prow.width,
                          .min_height = prow.row_height, .selected = isFav};
      const char *lbl = !selRel[0] ? "* (evidenzia un file)"
                        : isFav ? "* TOGLI DAI PREFERITI" : "* AGGIUNGI AI PREFERITI";
      if (ui_button_labeled(app, ui_scoped_id("palette/fav_toggle"), lbl, font, ANCHOR_TOP_LEFT, 0,
                            13.0f, prow.x, ry, &st) && selRel[0])
        ui_picker_favorite_toggle(selRel);
    }
    const char *clicked = NULL;
    static char clickedBuf[160];
    for (int section = 0; section < 2; ++section) {
      int count = section == 0 ? g_file_picker.favorite_count : g_file_picker.recent_count;
      float hy = ui_property_layout_next_row(&prow);
      char head[48];
      snprintf(head, sizeof(head), "%s (%d)", section == 0 ? "PREFERITI" : "RECENTI", count);
      draw_text_colored(app, font, head, prow.x, hy + font->ascender * 13.0f + 4.0f, 13.0f,
                        theme.text_muted.r, theme.text_muted.g, theme.text_muted.b);
      for (int i = 0; i < count; ++i) {
        const char *rel = section == 0 ? g_file_picker.favorites[i] : g_file_picker.recents[i];
        char id[16];
        snprintf(id, sizeof(id), "p%d_%d", section, i);
        ui_push_id(id);
        float ry = ui_property_layout_next_row(&prow);
        if (ry + prow.row_height > pal.y && ry < pal.y + pal.h) {
          char label[160];
          const char *base = strrchr(rel, '/');
          ui_ellipsize_left(font, base ? base + 1 : rel, 13.0f,
                            prow.width - theme.label_value_gap * 2.0f, label, sizeof(label));
          bool cur = selRel[0] && strcmp(rel, selRel) == 0;
          UIButtonStyle st = {.min_width = prow.width, .max_width = prow.width,
                              .min_height = prow.row_height, .align_left = true,
                              .selected = cur, .border = cur ? theme.border_thickness : -1.0f,
                              .has_custom_color = !cur, .color_normal = theme.panel,
                              .color_hover = theme.field_hover, .color_pressed = theme.selected,
                              .color_border = theme.border};
          if (ui_button_labeled(app, ui_scoped_id("entry"), label, font, ANCHOR_TOP_LEFT, 0, 13.0f,
                                prow.x, ry, &st)) {
            snprintf(clickedBuf, sizeof(clickedBuf), "%s", rel);
            clicked = clickedBuf;
          }
        }
        ui_pop_id();
      }
    }
    ui_property_layout_commit(&palScroll.layout, &prow);
    ui_scroll_end(app, &palScroll);
    if (clicked)
      ui_picker_select_relative(clicked);
  }

  UIPropertyLayout actions = ui_property_layout_begin(
      top.bounds.x, list_bounds.y + list_bounds.h, top.bounds.w, 1.0f);
  if (g_file_picker.mode == UI_FILE_PICKER_SAVE)
    ui_property_text(app, &actions, "filename", font, "NOME FILE",
                     g_file_picker.filename, sizeof(g_file_picker.filename));
  float action_y = ui_property_layout_next_row(&actions);
  float action_gap = theme.row_spacing;
  float action_w = (actions.width - action_gap) * 0.5f;
  UIButtonStyle cancel_style = {.min_width = action_w,
                                .max_width = action_w,
                                .min_height = actions.row_height};
  UIButtonStyle confirm_style = {.min_width = action_w,
                                 .max_width = action_w,
                                 .min_height = actions.row_height,
                                 .selected = true};
  unsigned action = 0;
  if (ui_button_labeled(app, ui_scoped_id("actions/cancel"), "ANNULLA", font,
                        ANCHOR_TOP_LEFT, 0, 14.0f, actions.x, action_y,
                        &cancel_style))
    action |= 1u;
  const char *confirm_label = g_file_picker.mode == UI_FILE_PICKER_SAVE
                                  ? "SALVA"
                                  : (g_file_picker.mode == UI_FILE_PICKER_DIRECTORY
                                         ? "SELEZIONA CARTELLA"
                                         : "APRI");
  if (ui_button_labeled(app, ui_scoped_id("actions/confirm"), confirm_label,
                        font, ANCHOR_TOP_LEFT, 0, 14.0f,
                        actions.x + action_w + action_gap, action_y,
                        &confirm_style))
    action |= 2u;
  if (action & 1u) {
    ui_file_picker_cancel();
  } else if (action & 2u) {
    char selected[APP_PATH_MAX] = "";
    if (g_file_picker.mode == UI_FILE_PICKER_DIRECTORY) {
      snprintf(selected, sizeof(selected), "%s", g_file_picker.current);
    } else if (g_file_picker.mode == UI_FILE_PICKER_SAVE) {
      snprintf(selected, sizeof(selected), "%s", g_file_picker.filename);
      size_t name_length = strlen(selected), extension_length = strlen(g_file_picker.extension);
      if (g_file_picker.extension[0] &&
          (name_length < extension_length ||
           strcmp(selected + name_length - extension_length,
                  g_file_picker.extension) != 0))
        strncat(selected, g_file_picker.extension,
                sizeof(selected) - strlen(selected) - 1);
      if (!ui_file_picker_build_path(&g_file_picker, selected,
                                     g_file_picker.selected,
                                     sizeof(g_file_picker.selected)))
        snprintf(g_file_picker.error, sizeof(g_file_picker.error),
                 "Nome file non valido");
      else {
        if (platform_file_exists(g_file_picker.selected))
          g_file_picker.overwrite_pending = true;
        else {
          snprintf(out_path, out_path_size, "%s", g_file_picker.selected);
          ui_file_picker_cancel();
          ui_panel_end(&panel);
          return true;
        }
      }
    } else if (g_file_picker.selected[0]) {
      snprintf(out_path, out_path_size, "%s", g_file_picker.selected);
      ui_picker_recent_push(ui_picker_relative(g_file_picker.selected));
      ui_file_picker_cancel();
      ui_panel_end(&panel);
      return true;
    }
  }
  if (g_file_picker.error[0]) {
    float row = ui_property_layout_next_row(&actions);
    draw_text_colored(app, font, g_file_picker.error, actions.x,
                      row + (actions.row_height + font->ascender * 13.0f) * 0.5f,
                      13.0f, theme.error.r, theme.error.g, theme.error.b);
  }
  ui_panel_end(&panel);
  ui_modal_end(g_file_picker.id, g_file_picker.open);
  return false;
}

bool ui_get_input_diagnostics(UIInputDiagnostics *out) {
  if (!out)
    return false;
  memset(out, 0, sizeof(*out));
  out->mouse_x = g_mouse_x;
  out->mouse_y = g_mouse_y;
  out->mouse_down = g_mouse_down;
  out->mouse_pressed = g_mouse_clicked;
  out->mouse_released = g_mouse_released;
  out->pointer_capture = ui_has_pointer_capture();
  out->keyboard_capture = ui_has_keyboard_capture();
  out->any_capture = out->pointer_capture || out->keyboard_capture;
  out->escape_pressed = g_escape_pressed;
  out->escape_consumed = g_escape_consumed;
  out->file_picker_open = g_file_picker.open;
  out->file_picker_id = g_file_picker.open ? g_file_picker.id : NULL;
#if 1 /* Generic menu support. */
  out->menu_depth = g_menu.depth;
#endif
  out->input_region_count = g_input_region_count;

  for (int i = ui_state_count - 1; i >= 0; --i) {
    if (!out->hovered_widget_id && ui_states[i].is_hover)
      out->hovered_widget_id = ui_states[i].id;
    if (!out->active_widget_id && ui_states[i].is_pressed)
      out->active_widget_id = ui_states[i].id;
  }
  for (int i = slider_state_count - 1; i >= 0; --i)
    if (slider_states[i].dragging) {
      out->active_widget_id = slider_states[i].id;
      break;
    }
  for (int i = text_input_state_count - 1; i >= 0; --i)
    if (text_input_states[i].focused) {
      out->focused_widget_id = text_input_states[i].id;
      break;
    }
  if (!out->focused_widget_id)
    for (int i = float_field_state_count - 1; i >= 0; --i)
      if (float_field_states[i].focused) {
        out->focused_widget_id = float_field_states[i].id;
        break;
      }
  for (int i = g_modal_state_count - 1; i >= 0; --i)
    if (g_modal_states[i].wasOpen) {
      out->active_modal_id = g_modal_states[i].id;
      break;
    }
  for (int i = dropdown_state_count - 1; i >= 0; --i)
    if (dropdown_states[i].open) {
      out->open_dropdown_id = dropdown_states[i].id;
      break;
    }
  for (int i = g_scroll_state_count - 1; i >= 0; --i) {
    if (!out->scrollbar_drag_owner && g_scroll_states[i].dragging)
      out->scrollbar_drag_owner = g_scroll_states[i].id;
    if (!out->scroll_consumer && g_scroll_states[i].wheel_consumed)
      out->scroll_consumer = g_scroll_states[i].id;
  }
  for (int i = g_input_region_count - 1; i >= 0; --i) {
    UIRect rect = g_input_regions[i].rect;
    if (ui_pointer_hit(rect.x, rect.y, rect.w, rect.h)) {
      out->pointer_region_owner = g_input_regions[i].id;
      break;
    }
  }
  out->highest_priority_owner =
      out->active_modal_id ? out->active_modal_id
      : out->file_picker_id ? out->file_picker_id
#if 1 /* Generic menu support. */
      : out->menu_depth > 0 ? g_menu.levels[0].id
#endif
      : out->open_dropdown_id ? out->open_dropdown_id
      : out->active_widget_id ? out->active_widget_id
      : out->focused_widget_id ? out->focused_widget_id
      : out->pointer_region_owner;
  return true;
}

void ui_debug_log_input_events(APP *app) {
#ifdef DEBUG
  static int enabled = -1;
  static bool previous_capture = false;
  static char previous_owner[128] = "";
  static bool key_was_down = false;
  if (enabled < 0)
    enabled = getenv("UI_INPUT_DEBUG") ? 1 : 0;
  if (!enabled || !app)
    return;

  bool key_down = glfwGetKey(app->window, GLFW_KEY_F3) == GLFW_PRESS ||
                  glfwGetKey(app->window, GLFW_KEY_G) == GLFW_PRESS ||
                  glfwGetKey(app->window, GLFW_KEY_R) == GLFW_PRESS ||
                  glfwGetKey(app->window, GLFW_KEY_S) == GLFW_PRESS;
  bool key_pressed = key_down && !key_was_down;
  key_was_down = key_down;
  UIInputDiagnostics state;
  if (!ui_get_input_diagnostics(&state))
    return;
  const char *owner = state.highest_priority_owner
                          ? state.highest_priority_owner
                          : "world";
  bool transitioned = state.any_capture != previous_capture ||
                      strcmp(owner, previous_owner) != 0;
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
  previous_capture = state.any_capture;
  snprintf(previous_owner, sizeof(previous_owner), "%s", owner);
#else
  (void)app;
#endif
}
