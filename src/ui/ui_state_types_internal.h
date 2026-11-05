#pragma once
#include "ui_text_edit_internal.h"
/* Stato privato della UI. Nessun handle grafico e nessun singleton di widget:
 * ogni istanza viene inizializzata e distrutta dalle API UIContext. */
#define MAX_UI_ELEMENTS 512
#define MAX_UI_INPUT_REGIONS 32
#define MAX_UI_ID_STACK 16
#define UI_SCOPED_ID_BUFFER_COUNT 16
#define MAX_UI_SECTIONS 64
#define MAX_UI_SCROLL_SCOPES 32
#define MAX_UI_MODALS 16
#define MAX_UI_TOASTS 4
#define MAX_UI_PANELS 64
#define MAX_UI_MENU_ITEMS 32 /* editor.md A5: menu contestuale unico + SELEZIONE nei menu */
#define MAX_UI_SUBMENU_SUBMISSIONS 16
#define MAX_UI_FLOAT_FIELDS 96
#define MAX_UI_SLIDERS 64
#define MAX_UI_TEXT_INPUTS 32
#define MAX_UI_DROPDOWNS 32
#define UI_DROPDOWN_SCROLL_ARROW_H 34.0f
#define UI_DROPDOWN_SCROLL_GAP 6.0f

typedef struct {
  char id[UI_ID_CAPACITY];
  bool open;
  unsigned long long last_seen_frame;
} UISectionState;
typedef struct {
  char id[UI_ID_CAPACITY];
  float offset, content_height, viewport_height, drag_grab;
  bool dragging, wheel_consumed, thumb_hovered;
} UIScrollState;
typedef struct {
  char id[UI_ID_CAPACITY];
  unsigned id_hash;
  bool is_hover;
  bool is_pressed;
  float hover_t;
  unsigned long long last_seen_frame;
} UIState;
typedef struct {
  char id[UI_ID_CAPACITY];
  UIRect rect;
} UIInputRegion;
typedef struct {
  char id[UI_ID_CAPACITY];
  bool wasOpen;
  char return_focus[UI_ID_CAPACITY], return_modal[UI_ID_CAPACITY];
  bool submittedThisFrame;
  bool scopeOpen; /* ui_modal_begin riuscito in questo frame, ui_modal_end non ancora chiamato */
} UIModalOpenState;
typedef struct {
  char message[96];
  UIToastKind kind;
  float remaining;
} UIToast;
typedef struct {
  char id[UI_ID_CAPACITY];
  bool collapsed;
} UIPanelState;
typedef struct {
  char id[UI_ID_CAPACITY], label[64], submenu_id[UI_ID_CAPACITY], shortcut[32];
  UIMenuItemKind kind;
  bool disabled, checked;
} UIStoredMenuItem;
typedef struct {
  bool active, context_root, opens_left;
  char id[UI_ID_CAPACITY], parent_id[UI_ID_CAPACITY], parent_item_id[UI_ID_CAPACITY];
  int depth, item_count;
  float width;
  UIRect anchor, popup;
  UIStoredMenuItem *items;
  int item_capacity;
} UIMenuLevel;
typedef struct {
  char parent_id[UI_ID_CAPACITY], parent_item_id[UI_ID_CAPACITY], submenu_id[UI_ID_CAPACITY];
  float width;
  int item_count;
  UIStoredMenuItem *items;
  int item_capacity;
} UIMenuSubmission;
typedef struct {
  UIMenuLevel levels[UI_MENU_MAX_DEPTH];
  UIMenuSubmission submissions[MAX_UI_SUBMENU_SUBMISSIONS];
  int depth, submission_count, hovered_depth, owning_depth;
  bool overflowed, escape_consumed, root_opened_this_frame;
  bool root_submitted_this_frame;
  bool result_pending;
  char result_root_id[UI_ID_CAPACITY], result_item_id[UI_ID_CAPACITY];
} UIMenuHierarchy;
typedef struct {
  char id[UI_ID_CAPACITY];
  bool dragging;
  float grab_offset; /* Splitter: preserve grab location instead of jumping. */
  unsigned long long last_seen_frame;
} UISliderState;
typedef struct {
  char id[UI_ID_CAPACITY];
  char buffer[32];
  float original;
  int original_int;
  bool focused;
  bool invalid;
  unsigned long long last_seen_frame;
} UIFloatFieldState;
typedef struct {
  char id[UI_ID_CAPACITY];
  bool open;
  unsigned long long last_seen_frame;
  int scrollOffset; // indice della prima opzione visibile, quando la
                    // lista ha più righe di quante ne stiano a schermo
  int keyboard_index;

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
  /* Owned visible-row copies, allocated only when a popup is submitted.
   * Capacity survives closing and cache-entry reuse; reset/destroy frees it. */
  char (*popupOptions)[128];
  int popupOptionCapacity;
} UIDropdownState;

typedef struct {
  char id[UI_ID_CAPACITY], modal[UI_ID_CAPACITY];
  int kind, scroll_index;
  float content_y, height;
} UINavigationEntry;
