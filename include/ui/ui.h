#pragma once

#include "math/types.h"
#include "text/font_types.h"
#include "ui/renderer.h"
#include "ui/error.h"
#include "ui/version.h"
#include "text/anchor.h"
#include <stdbool.h>
#include "input/events.h"

/* Contesto UI posseduto dal chiamante. Le API dei widget usano il contesto
 * corrente selezionato esplicitamente, sul thread principale. Nessun default
 * implicito: selezionare un contesto prima di chiamare API UI con stato.
 * Reset ripristina anche tema, callback feedback e notifiche; conserva il backend. Reset/destroy/switch
 * restituiscono false se interromperebbero un frame aperto.
 * Destroy(NULL) e valido; un puntatore gia distrutto non va riutilizzato.
 * Completare anche il submit del renderer prima di cambiare contesto: geometria
 * e texture rimangono condivise. Il cambio rilascia la preview GPU transiente.
 * Distruggere i contesti prima delle risorse prese in prestito dal backend. */
typedef struct UIContext UIContext;
UIContext *ui_context_create(void);
/* Copia il backend; NULL lo scollega. Rifiuta tabelle incomplete e frame aperti.
 * Libera la preview col vecchio backend. Reset conserva questo collegamento. */
bool ui_context_set_renderer(UIContext *context, const UIRenderer *renderer, void *user);
UIContext *ui_context_get_current(void);
bool ui_context_set_current(UIContext *context);
bool ui_context_reset(UIContext *context);
bool ui_context_destroy(UIContext *context);
/* First reported widget/context error; successful calls do not clear it.
 * Returned data is owned by the value, independent of future calls/reset.
 * NULL context returns ARGUMENT/CONTEXT. clear(NULL) is a no-op.
 * Frame-admission failures use the status return below, preserving context. */
UIErrorInfo ui_context_error(const UIContext *context);
void ui_context_clear_error(UIContext *context);

/* Optional host clipboard, copied per context; main thread, no reentrant UI calls.
 * read returns a borrowed UTF-8 span (NUL not required), valid until release.
 * On successful read, release is called exactly once when supplied, even if
 * the span is rejected. On failure the host retains responsibility for cleanup.
 * write borrows a NUL-terminated string for the call; false must leave cut intact.
 * NULL callbacks mean unavailable. user must outlive the binding. Reset preserves
 * the binding; replacing it is allowed only between frames. */
typedef struct {
    bool (*read)(void *user, const char **text, size_t *bytes);
    void (*release)(void *user, const char *text);
    bool (*write)(void *user, const char *text);
} UIClipboard;
bool ui_context_set_clipboard(UIContext *context, const UIClipboard *clipboard, void *user);

typedef struct {
    float r, g, b;
} UIColor;

typedef struct {
    UIColor button_normal;
    UIColor button_hover;
    UIColor button_pressed;
    UIColor button_border;
    UIColor text;

    /* Editor chrome belongs to the established theme, never a parallel
     * per-panel palette. */
    UIColor editor_background, panel, panel_header, field, field_hover;
    UIColor selected, disabled, text_muted, accent, warning, error, success;
    UIColor border, separator, danger;

    /* Base virtual-pixel metrics. Explicit editor layouts multiply their
     * dimensions by uiScale; canonical widgets consume these defaults. */
    float row_height, button_height, compact_button_height, field_height;
    float panel_padding, row_spacing, section_spacing, indent;
    float label_value_gap, scrollbar_width, border_thickness;
    float separator_thickness, popup_padding, modal_padding;
    float menu_height, panel_header_height, status_height;
    float slider_track_height, numeric_field_width;
    float text_scale, secondary_text_scale, section_text_scale;
    bool paper_panels; /* opt-in app skin; default/editor themes remain flat */
} UITheme;

typedef struct {
    float min_width;
    float max_width;
    float min_height;
    float padding_x;
    float padding_y;
    float border;

    bool disabled;
    bool selected;
    bool danger;
    bool align_left;

    bool has_custom_color;
    UIColor color_normal;
    UIColor color_hover;
    UIColor color_pressed;
    UIColor color_border;
    /* Colore testo per-bottone (linguaggio "orange active" del mockup) —
     * senza questo ogni bottone usava theme.text. */
    bool has_custom_text;
    UIColor color_text;
} UIButtonStyle;

typedef struct {
    float x, y, w, h;
} UIRect;

/* Equal-width tabs; one Tab focus stop, arrows/Home/End select enabled tabs.
 * Caller owns selection and draws only the selected page. Invalid selection
 * selects the first enabled tab; all disabled yields -1. Maximum 64 tabs. */
typedef struct { const char *label; bool disabled; } UITab;
bool ui_tabs(const char *id,Font *font,UIRect bounds,const UITab *tabs,int count,int *selected);
typedef enum { UI_SPLIT_COLUMNS, UI_SPLIT_ROWS } UISplitAxis;
/* Caller owns ratio [0,1] of space excluding the handle; returned rectangles
 * do not install clips. Drag or focus+arrows/Home/End changes ratio. Minimums
 * shrink proportionally if the area is too small. NaN ratio initializes at
 * the middle of the permitted interval. Returns whether ratio changed;
 * invalid arguments return false and zero output rectangles. */
bool ui_splitter(const char *id,UIRect bounds,UISplitAxis axis,float thickness,
                 float min_first,float min_second,float *ratio,UIRect *first,UIRect *second);

/* Colore da esadecimale sRGB (#RRGGBB) convertito nello spazio LINEARE
 * che la swapchain SRGB si aspetta — stesso percorso usato dal tema. */
UIColor ui_color_srgb(uint32_t rgb);

UITheme ui_theme_dark(void);
UITheme ui_theme_light(void);
UITheme ui_theme_warm(void);

void ui_set_theme(UITheme theme);
UITheme ui_get_theme(void);

/* Input esplicito: array copiato durante begin, mai modificato dalla UI.
 * Coordinate e canvas nella stessa unita logica; dimensioni finite e positive.
 * Nessun polling: il chiamante consegna un solo batch per frame.
 * focus_lost annulla pressioni/drag e focus degli editor, chiude i dropdown
 * e ignora mouse_down, rotella e tasti del batch senza generare un click.
 * false per contesto assente, frame gia aperto, batch troppo grande/invalido
 * numeri non finiti o memoria insufficiente per il routing del batch;
 * in tal caso il contesto non cambia. */
typedef struct {
  float width, height;
  float mouse_x, mouse_y, delta_time, scroll_delta;
  bool mouse_down, focus_lost, debug_keys_down;
  const InputKeyEvent *events;
  size_t event_count;
} UIFrameInput;
bool ui_begin_frame_input(const UIFrameInput *input);
/* Same operation as begin_frame_input, with a precise, allocation-free result.
 * NONE means the frame opened. Any other result leaves the context, stored
 * diagnostics and caller input unchanged. Missing context/open frame: STATE;
 * bad input: ARGUMENT; excessive event count: LIMIT; reserve failure: MEMORY. */
UIErrorCode ui_begin_frame_input_status(const UIFrameInput *input);
/* Stable ID of the keyboard navigation target, or an empty string. Borrowed
 * from the current context, valid until its focus changes/reset/destruction. */
const char *ui_keyboard_focus_id(void);

void ui_end_frame(void);

typedef void (*UIFeedbackFn)(const char *what, void *user);
void ui_set_feedback(UIFeedbackFn fn, void *user);

Vec2 ui_mouse_position(void);
bool ui_mouse_down(void);
/* Numero progressivo del frame UI (cambia a ogni ui_begin_frame): per
 * stati "validi solo in questo frame" senza tenere un flag da azzerare. */
unsigned long long ui_frame_serial_hint(void);
void ui_set_scroll_delta(float delta);
float ui_scroll_delta(void);
bool ui_consume_scroll_delta(void);

typedef enum {
  UI_TOAST_INFO,
  UI_TOAST_SUCCESS,
  UI_TOAST_WARNING,
  UI_TOAST_ERROR,
} UIToastKind;
void ui_notify(UIToastKind kind, const char *message);
void ui_draw_notifications(Font *font);
/* Quota (px virtuali) da cui partono i toast: il chiamante la imposta sotto
 * il proprio chrome fisso (editor: sotto topbar+subbar). < 0 = default. */
void ui_set_notification_top(float y);

/* Stable scoped IDs for new canonical UI callers.  Scopes are semantic
 * (panel/section/item), never geometry-derived. */
#define UI_ID_CAPACITY UI_ERROR_ID_CAPACITY
/* IDs (including composed scopes) are limited to 255 bytes. Invalid IDs
 * are rejected without allocating widget state. Failed pushes do not change
 * the stack and must not be popped. */
bool ui_push_id(const char *scope);
void ui_pop_id(void);
/* Returns an empty string on overflow. Storage is reused after 16 calls. */
const char *ui_scoped_id(const char *local_id);

bool ui_button_styled(const char *id, Font *font,
                      TextAnchor anchor, float margin, float scale,
                      float offset_x, float offset_y,
                      const UIButtonStyle *style);

// Slider orizzontale legato a *value. Ritorna true nel frame in cui il
// valore cambia. out_drag_started (opzionale, può essere NULL): true nel
// frame esatto in cui inizia il trascinamento — utile per pushare undo
// PRIMA che il valore cambi. 'scale' funziona come per ui_button_labeled/
// ui_dropdown: 1.0 = dimensioni base, il chiamante moltiplica per il
// proprio fattore (es. g_uiScale) se il pannello scala con la DIMENSIONE
// UI, o passa 1.0 per elementi a dimensione fissa (es. i modali).
bool ui_slider(const char *id, Font *font, TextAnchor anchor,
              float margin, float scale, float offset_x, float offset_y,
              float width, float *value, float min, float max,
              const char *label, bool *out_drag_started);

// True se lo slider con questo id (lo STESSO passato a ui_slider/
// ui_property_slider, gia' scoped con ui_scoped_id se applicabile) sta
// trascinando la maniglia PROPRIO ORA -- sola lettura dello stato interno
// che ui_slider gia' mantiene per il proprio rilascio, stesso principio
// di ui_dropdown_is_open/ui_text_input_is_focused. Non esiste ancora
// nessuno slider con questo id: false. Nessun effetto su ui_slider stesso
// o su chi non la chiama -- puramente additiva.
bool ui_slider_is_dragging(const char *id);

/* Canonical exact numeric editor used by sliders and property rows. */
bool ui_parse_float_clamped(const char *text, float min, float max,
                            float *out_value);
bool ui_parse_int_clamped(const char *text, int min, int max,
                          int *out_value);
bool ui_float_field(const char *id, Font *font, float x, float y,
                    float width, float height, float scale, float *value,
                    float min, float max);

typedef struct {
    float x, y, width, cursor_y, label_width, row_height, scale;
} UIPropertyLayout;
typedef struct {
    UIRect bounds;
    float cursor_y, indent, row_height, spacing, scale;
} UILayout;
typedef struct {
    UIRect outer, content;
    UILayout layout;
    bool open, clip_pushed;
} UIPanel;
typedef struct { UIRect bounds; int count, index; float spacing; } UIRow;
typedef struct { UILayout layout; UILayout *parent; bool open; } UISection;
/* End scopes once, in reverse begin order, on the same context and frame.
 * clip_pushed reports whether clipping was acquired: skip content when false.
 * End is safe after a failed push and repeated end on the same object is inert.
 * Do not copy an active scope and close both copies. */
typedef struct {
    UILayout layout;
    UIRect viewport;
    float offset, max_scroll;
    int state_index;
    bool nav_pushed;
    bool open, clip_pushed;
} UIScrollScope;
typedef struct {
    const char *id;
    const char *scrollbar_drag_owner;
    float offset, max_scroll, content_height, viewport_height;
    bool wheel_consumed, scrollbar_dragging;
} UIScrollDiagnostics;

UILayout ui_layout_begin(UIRect bounds, float scale);
void ui_layout_end(UILayout *layout);
float ui_layout_next_row(UILayout *layout);
void ui_layout_space(UILayout *layout, float amount);
void ui_layout_indent(UILayout *layout, float amount);
UILayout ui_layout_begin_nested(UILayout *parent, float indent);
void ui_layout_end_nested(UILayout *layout);
UIRow ui_row_begin(UILayout *layout, int item_count);
UIRect ui_row_next(UIRow *row);
UISection ui_section_begin(UILayout *layout, const char *id,
                           const char *title, Font *font);
/* Come ui_section_begin ma chiusa alla prima apertura (sezioni rare o
 * avanzate); lo stato scelto dall'utente resta poi persistente per id. */
UISection ui_section_begin_collapsed(UILayout *layout, const char *id,
                                     const char *title, Font *font);
void ui_section_end(UISection *section);
UIScrollScope ui_scroll_begin(const char *id, UIRect viewport, float scale);
void ui_scroll_end(UIScrollScope *scope);
bool ui_scroll_get_diagnostics(const char *id, UIScrollDiagnostics *out);
/* The same scope/clip_pushed contract as UIScrollScope applies to UIPanel. */
UIPanel ui_panel_begin(const char *id, const char *title,
                       Font *font, UIRect bounds, float scale);
void ui_panel_end(UIPanel *panel);
UIPropertyLayout ui_property_layout_begin(float x, float y, float width,
                                          float scale);
void ui_property_layout_commit(UILayout *parent, const UIPropertyLayout *layout);
float ui_property_layout_next_row(UIPropertyLayout *layout);
void ui_property_layout_gap(UIPropertyLayout *layout, float amount);
void ui_property_label(const UIPropertyLayout *layout,
                       Font *font, const char *label, float y);
/* Righe di SOLA LETTURA canoniche: etichetta + valore (colonna destra,
 * troncato con "...") e nota a tutta larghezza (testo muted, troncato).
 * Sostituiscono le ui_property_label "libere" con testo lungo che venivano
 * tagliate a 47 caratteri. Nessuno stato, nessun id. */
void ui_property_value(UIPropertyLayout *layout, Font *font,
                       const char *label, const char *value);
void ui_property_note(UIPropertyLayout *layout, Font *font,
                      const char *text);
bool ui_property_float(UIPropertyLayout *layout, const char *id,
                       Font *font, const char *label, float *value,
                       float min, float max);
bool ui_property_int(UIPropertyLayout *layout, const char *id,
                     Font *font, const char *label, int *value, int min, int max);
bool ui_property_vec2(UIPropertyLayout *layout, const char *id,
                      Font *font, const char *label, Vec2 *value,
                      float min, float max);
bool ui_property_slider(UIPropertyLayout *layout, const char *id,
                        Font *font, const char *label, float *value,
                        float min, float max, bool *out_drag_started);
/* Inline RGB picker: presets, sRGB channels 0..255, read-only hex and preview.
 * Caller owns linear UIColor. Edits quantize to 8-bit sRGB; unchanged colors
 * retain full precision. Invalid components normalize into [0,1]. No alpha.
 * Uses six property rows. IDs are scoped under id; no persistent allocation. */
bool ui_property_color(UIPropertyLayout *layout,const char *id,Font *font,
                       const char *label,UIColor *color);
/* Canonical non-numeric property rows.  Editor code supplies semantics, never
 * geometry: the property layout owns label/value columns and row progression. */
bool ui_property_toggle(UIPropertyLayout *layout, const char *id,
                        Font *font, const char *label, bool *value);
bool ui_property_checkbox(UIPropertyLayout *layout, const char *id,
                          Font *font, const char *label, bool *value);
bool ui_property_dropdown(UIPropertyLayout *layout, const char *id,
                          Font *font, const char *label,
                          const char *const *options, int option_count,
                          int *selected);
bool ui_property_text(UIPropertyLayout *layout, const char *id,
                      Font *font, const char *label, char *buffer,
                      size_t buffer_size);
bool ui_property_action(UIPropertyLayout *layout, const char *id,
                        Font *font, const char *label, bool danger);
unsigned ui_property_action_row(UIPropertyLayout *layout,
                                const char *id, Font *font,
                                const char *const *labels, int count);
/* Read-only project asset row. Clicking either the value or ellipsis asks the
 * caller to configure the single canonical picker. */
bool ui_property_asset(UIPropertyLayout *layout, const char *id,
                       Font *font, const char *label, char *path,
                       size_t path_size, bool *out_browse);

/* Optional filesystem picker; declarations retained through this include. */
#include "ui/file_picker.h"

bool ui_any_hover(void);
bool ui_any_text_input_focused(void);
bool ui_has_pointer_capture(void);
bool ui_has_keyboard_capture(void);
/* True while a persistent UI overlay owns input (text editing, a dropdown,
 * a modal, or an editor context menu). */
bool ui_has_input_capture(void);
/* Modali, file picker e menu richiedono input esclusivo. I normali campi,
 * slider e dropdown non sono overlay esclusivi: le scorciatoie globali e
 * FILE possono chiuderne il focus e continuare a funzionare. */
bool ui_has_blocking_overlay(void);

bool ui_pointer_blocked_by_modal(void);
void ui_release_input_capture(void);

typedef struct {
  float mouse_x, mouse_y;
  bool mouse_down, mouse_pressed, mouse_released;
  bool pointer_capture, keyboard_capture, any_capture;
  bool escape_pressed, escape_consumed;
  bool file_picker_open;
  int menu_depth, input_region_count;
  const char *hovered_widget_id;
  const char *active_widget_id;
  const char *focused_widget_id;
  const char *active_modal_id;
  const char *file_picker_id;
  const char *open_dropdown_id;
  const char *scrollbar_drag_owner;
  const char *scroll_consumer;
  const char *pointer_region_owner;
  const char *highest_priority_owner;
} UIInputDiagnostics;

bool ui_get_input_diagnostics(UIInputDiagnostics *out);
/* DEBUG-only transition/event trace. Enabled with UI_INPUT_DEBUG=1. */
void ui_debug_log_input_events(void);

/* Current-frame, draw-independent ownership regions.  The editor layout
 * prepass registers persistent UI surfaces before viewport input runs. */
void ui_input_regions_begin(void);
bool ui_input_region_register(const char *id, UIRect rect);
bool ui_input_region_contains(float x, float y);
int ui_input_region_count(void);
const UIRect *ui_input_region_at(int index, const char **out_id);

/* Nested HUD clipping. Deferred popup rendering runs after callers pop the
 * parent clip, so popups retain the top-level screen clip. */
bool ui_clip_push(UIRect rect);
bool ui_clip_pop(void);
int ui_clip_depth(void);

/* Per-current-context ASCII sentence case for all-caps widget labels. Default
 * false: preserve caller text. Mixed-case labels are preserved even when true.
 * Reset restores false; text input buffers are never normalized. */
void ui_set_sentence_case(bool enabled);
/* Shared label formatting; bounded UTF-8 copy when normalization is disabled. */
void ui_humanize_label(const char *in, char *out, size_t out_size);

// Come ui_button_styled, ma 'id' è stabile (usato per lo stato interno)
// mentre 'label' è il testo mostrato (può cambiare ogni frame).
bool ui_button_labeled(const char *id, const char *label, Font *font,
                       TextAnchor anchor, float margin, float scale,
                       float offset_x, float offset_y, const UIButtonStyle *style);

// Altezza VERA che un bottone avrà con questo font/scale/style — stessa
// identica formula usata internamente da ui_button_labeled/ui_button_styled.
// Usala per dimensionare pannelli di sfondo attorno a gruppi di bottoni,
// invece di indovinare un'altezza fissa: quella indovinata può disallinearsi
// dal bottone vero se l'ascender del font è diverso da quanto stimato.
float ui_button_height(Font *font, float scale, const UIButtonStyle *style);

/* Absolute-position checkbox used where a compact property row is needed. */
bool ui_checkbox(const char *id, const char *label, Font *font,
                 float x, float y, float scale, bool *value);
bool ui_toggle(const char *id, const char *label, Font *font,
               float x, float y, float scale, float width, bool *value);

// Campo di testo cliccabile: click dentro per attivare la digitazione
// (testo Unicode da eventi della piattaforma), click fuori per
// disattivarla senza confermare. ENTER conferma — ritorna true SOLO nel
// frame in cui viene premuto — e disattiva il focus; ESC disattiva senza
// confermare. 'buffer' è una stringa C mutabile di 'buffer_size' byte,
// già inizializzata (anche vuota) da chi chiama: viene editata in place
// carattere per carattere, non solo alla conferma.
// Il cursore viene riallineato se il chiamante cambia il testo tra frame.
// Un buffer senza terminatore entro buffer_size viene ignorato.
/* buffer_size is bytes including NUL. Invalid UTF-8 or unterminated input is
 * rejected unchanged. Insertion never writes a partial encoded character.
 * Shift+arrows/Home/End selects; Ctrl+A selects all; C/X/V use UIClipboard.
 * Ctrl+Z/Y (or Ctrl+Shift+Z) undo/redo: up to 16 steps and 64 KiB per field,
 * snapshots up to 32 KiB including NUL. External text changes invalidate history. */
bool ui_text_input(const char *id, Font *font, TextAnchor anchor,
                   float margin, float scale, float offset_x, float offset_y,
                   float width, char *buffer, int buffer_size,
                   const UIButtonStyle *style);

/* Integer property row for values where a continuous slider is misleading.
 * The value can be nudged with -/+ or entered directly; ENTER commits and
 * ESC abandons the in-progress text.  The implementation deliberately
 * reuses ui_text_input(), so focus and editor-shortcut blocking remain
 * centralized in the existing UI system. */
// true se il campo con questo id sta catturando la tastiera in questo
// momento. Utile a chi chiama per sospendere altri keybind (es. T, N,
// frecce) finché l'utente sta scrivendo — ui_text_input consuma i tasti
// solo per sé, ma platform_key_state è polling globale: qualunque altro codice che
// controlla la stessa tastiera va zittito esplicitamente da fuori.
bool ui_text_input_is_focused(const char *id);

// Attiva il focus da codice, come se l'utente avesse cliccato sul campo
// (es. da un bottone "RINOMINA" separato dal campo stesso).
void ui_text_input_focus(const char *id);

// Dropdown: bottone che mostra l'opzione corrente (o, se header_label non è
// NULL, un'etichetta fissa — utile per i menu-azione tipo FILE/EDIT, dove
// ogni voce cliccata esegue un'azione e il bottone deve continuare a dire
// "FILE", non "diventare" l'ultima azione eseguita). Click apre/chiude,
// click su una voce la seleziona (e chiude la lista), click fuori chiude
// senza cambiare selezione. Ritorna true SOLO nel frame in cui la
// selezione cambia. *selected_index è letto e scritto in place (clampato
// al range valido). Il controllo chiuso viene disegnato subito; la lista
// aperta viene registrata per ui_draw_dropdown_popups(), che la porta sopra
// tutti i controlli normali indipendentemente dall'ordine delle chiamate.
// Le etichette del popup sono copiate in memoria allocata alla prima apertura,
// fino a 128 righe visibili di 128 byte. La capacità viene riusata fino al
// reset/destroy del contesto. Se la crescita fallisce, la lista viene chiusa
// senza selezionare una voce; un'apertura successiva può riprovare.
bool ui_dropdown(const char *id, Font *font, TextAnchor anchor,
                 float margin, float scale, float offset_x, float offset_y,
                 float width, const char *const *options, int option_count,
                 int *selected_index, const char *header_label,
                 const UIButtonStyle *style);

// true se il dropdown con questo id ha la lista aperta in questo momento —
// stesso scopo di ui_text_input_is_focused, per sospendere altri keybind
// (es. i tasti freccia) finché il menu è aperto.
bool ui_dropdown_is_open(const char *id);

// Chiude/apre programmaticamente una lista gia' registrata. Utile quando due
// dropdown della stessa area non devono poter restare aperti e sovrapposti.
void ui_dropdown_set_open(const char *id, bool open);

// Pass overlay dei dropdown. Va chiamato una sola volta alla fine dell'HUD,
// dopo pannelli e controlli normali. Le modali chiudono le liste aperte e
// restano quindi sopra il popup layer.
void ui_draw_dropdown_popups(void);

// Modale: pannello centrato con barra del titolo e bottone di chiusura
// (X), stesso genere di finestra "Salva con nome" di un programma
// qualunque — invece di infilare campi/bottoni sparsi dentro un pannello
// già affollato, stanno per conto loro sopra a tutto.
//
// *is_open è posseduto dal chiamante (es. una static bool locale
// "showSaveDialog"): se è già false, non disegna nulla e ritorna false
// subito. Se l'utente clicca la X, lo mette a false e ritorna false —
// quindi il pattern d'uso è:
//
//   static bool showDialog = false;
//   if (ui_button_labeled(..., "APRI")) showDialog = true;
//   float cx, cy, cw, ch;
//   if (ui_modal_begin("id", "Titolo", font, 500, 300, &showDialog,
//                      &cx, &cy, &cw, &ch)) {
//     // disegna qui dentro il contenuto, usando cx/cy/cw/ch come area
//     // disponibile — es. ui_text_input(..., ANCHOR_TOP_LEFT, 0, scale,
//     // cx, cy, cw, buffer, ...)
//   }
//
// Aprire una modale chiude le liste dropdown aperte, mantenendo la modale al
// livello di priorità UI più alto.
bool ui_modal_begin(const char *id, const char *title, Font *font,
                    float width, float height, bool *is_open,
                    float *out_content_x, float *out_content_y,
                    float *out_content_w, float *out_content_h);
/* Completes modal ownership after the caller has processed its action row. */
void ui_modal_end(const char *id, bool is_open);

#if 1 /* Generic menu API, extracted from the editor guard. */
/* One bounded menu hierarchy serves menu bars and context menus.  Callers
 * submit roots/submenus during ordinary HUD drawing; ui_menu_render_overlay()
 * performs the single top-layer render and hit-test pass. Item strings are
 * copied into storage allocated on demand and retained until context reset or
 * destroy. Allocation failure closes the affected menu/branch and sets the
 * overflowed diagnostic; submenu submission also returns false. */
typedef enum {
  UI_MENU_ITEM_ACTION = 0,
  UI_MENU_ITEM_SUBMENU,
  UI_MENU_ITEM_SEPARATOR,
} UIMenuItemKind;

typedef struct {
  const char *id;
  const char *label;
  UIMenuItemKind kind;
  const char *submenu_id;
  const char *shortcut;
  bool disabled;
  bool checked;
} UIMenuItem;

#define UI_MENU_MAX_DEPTH 4

typedef struct {
  int active_depth;
  int hovered_depth;
  int owning_depth;
  bool escape_consumed;
  bool overflowed;
  const char *popup_id[UI_MENU_MAX_DEPTH];
  const char *parent_id[UI_MENU_MAX_DEPTH];
  const char *parent_item_id[UI_MENU_MAX_DEPTH];
  UIRect anchor[UI_MENU_MAX_DEPTH];
  UIRect popup[UI_MENU_MAX_DEPTH];
  bool opens_left[UI_MENU_MAX_DEPTH];
} UIMenuDiagnostics;

const char *ui_menu_bar(const char *id, const char *label,
                        Font *font, float x, float y, float width, float scale,
                        const UIMenuItem *items, int item_count,
                        const UIButtonStyle *style);
bool ui_menu_submit_submenu(const char *parent_popup_id,
                            const char *parent_item_id,
                            const char *submenu_id,
                            const UIMenuItem *items, int item_count);
void ui_menu_open_context(const char *id, float x, float y, float width,
                          const UIMenuItem *items, int item_count);
const char *ui_menu_take_action_id(const char *root_id);
bool ui_menu_hierarchy_is_open(void);
void ui_menu_close_hierarchy(void);
void ui_menu_render_overlay(Font *font);
bool ui_menu_get_diagnostics(UIMenuDiagnostics *out);

bool ui_collapsible_header(const char *id, const char *label,
                           Font *font, float x, float y, float width,
                           float scale);
/* Release text editing snapshots; called by app_shutdown. */
void ui_text_input_reset(void);
#endif

/* Basic controls: IDs are caller-scoped, values belong to the application. */
bool ui_radio_button(const char *id,Font *font,UIRect bounds,const char *label,
                     int option,int *selected,bool disabled);
/* fraction clamps to [0,1]. Indeterminate animation uses caller-owned phase
 * in seconds. No input capture; NULL label shows percent or no text. */
void ui_progress_bar(Font *font,UIRect bounds,float fraction,bool indeterminate,
                     float phase,const char *label);
/* Submit after the target using its navigation ID. Copies at most 511 UTF-8
 * bytes; renders above content at end_frame after delay seconds. One tooltip
 * per frame, no input capture. Font must live through end_frame. */
void ui_tooltip(const char *target_id,Font *font,UIRect target,const char *text,float delay);
/* One property row with Show/Hide button. Caller initializes revealed=false.
 * UTF-8 editing/undo/selection/paste match text input; copy/cut are suppressed.
 * Masking is visual only: caller buffer and editing history contain plaintext.
 * Returns text confirmation (Enter); revealed is updated independently. */
bool ui_property_password(UIPropertyLayout *layout,const char *id,Font *font,
                          const char *label,char *buffer,size_t capacity,bool *revealed);
