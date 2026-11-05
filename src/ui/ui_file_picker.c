#include "ui_widget_internal.h"
#include "ui_overlay_internal.h"
#include "ui_state_render_internal.h"
#include "ui_state_picker_internal.h"
/* Optional filesystem widget. The core has no link-time dependency on this module. */
#include "ui/ui.h"
#include "text/utf8.h"
#include "platform/platform_fs.h"
#include "platform/paths.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ui_state_types_internal.h"
#include "ui_paint_internal.h"
#include "ui_file_picker_internal.h"
static UIContext *picker_context(void) {
  UIContext *ctx=ui_context_get_current();
  assert(ctx && "Select a UIContext before using the file picker");
  return ctx;
}
static UIFilePickerState *picker_state(void) {
  return ui_state_picker(picker_context())->file_picker_state;
}
/* ------------------------------------------------------------------------- */
/* Bounded optional file picker; callers own the semantic operation, while
 * this module owns navigation, filtering and persistence. */

/* --- palette preferiti/recenti -------------------------------------------- */
static bool ui_picker_palette_file(char *out, size_t size) {
  char dir[APP_PATH_MAX];
  if (!picker_state()->favorites_key[0] || !paths_user_config_dir(dir, sizeof(dir), true))
    return false;
  int n = snprintf(out, size, "%s/picker_%s.txt", dir, picker_state()->favorites_key);
  return n > 0 && (size_t)n < size;
}

static void ui_picker_palette_load(void) {
  picker_state()->favorite_count = picker_state()->recent_count = 0;
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
        n - 2 >= sizeof(picker_state()->favorites[0]))
      continue; /* riga malformata o percorso troppo lungo: ignorata */
    if (line[0] == 'F' && picker_state()->favorite_count < UI_PICKER_FAVORITES_MAX)
      memcpy(picker_state()->favorites[picker_state()->favorite_count++], line + 2, n - 1);
    else if (line[0] == 'R' && picker_state()->recent_count < UI_PICKER_RECENTS_MAX)
      memcpy(picker_state()->recents[picker_state()->recent_count++], line + 2, n - 1);
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
  for (int i = 0; i < picker_state()->favorite_count; ++i)
    fprintf(f, "F|%s\n", picker_state()->favorites[i]);
  for (int i = 0; i < picker_state()->recent_count; ++i)
    fprintf(f, "R|%s\n", picker_state()->recents[i]);
  fclose(f);
}

/* percorso assoluto -> relativo alla root del picker ("" se fuori root) */
static const char *ui_picker_relative(const char *absolute) {
  size_t rl = strlen(picker_state()->root);
  if (strncmp(absolute, picker_state()->root, rl) != 0 || absolute[rl] != '/')
    return "";
  return absolute + rl + 1;
}

static int ui_picker_favorite_index(const char *relative) {
  for (int i = 0; i < picker_state()->favorite_count; ++i)
    if (strcmp(picker_state()->favorites[i], relative) == 0)
      return i;
  return -1;
}

static void ui_picker_favorite_toggle(const char *relative) {
  if (!relative || !relative[0])
    return;
  int idx = ui_picker_favorite_index(relative);
  if (idx >= 0) {
    for (int i = idx; i + 1 < picker_state()->favorite_count; ++i)
      memcpy(picker_state()->favorites[i], picker_state()->favorites[i + 1],
             sizeof(picker_state()->favorites[0]));
    picker_state()->favorite_count--;
  } else if (picker_state()->favorite_count < UI_PICKER_FAVORITES_MAX) {
    snprintf(picker_state()->favorites[picker_state()->favorite_count++],
             sizeof(picker_state()->favorites[0]), "%s", relative);
  }
  ui_picker_palette_save();
}

static void ui_picker_recent_push(const char *relative) {
  if (!picker_state()->favorites_key[0] || !relative || !relative[0])
    return;
  /* rimuovi un duplicato, poi inserisci in testa */
  for (int i = 0; i < picker_state()->recent_count; ++i) {
    if (strcmp(picker_state()->recents[i], relative) == 0) {
      for (int j = i; j + 1 < picker_state()->recent_count; ++j)
        memcpy(picker_state()->recents[j], picker_state()->recents[j + 1],
               sizeof(picker_state()->recents[0]));
      picker_state()->recent_count--;
      break;
    }
  }
  if (picker_state()->recent_count < UI_PICKER_RECENTS_MAX)
    picker_state()->recent_count++;
  for (int j = picker_state()->recent_count - 1; j > 0; --j)
    memcpy(picker_state()->recents[j], picker_state()->recents[j - 1],
           sizeof(picker_state()->recents[0]));
  snprintf(picker_state()->recents[0], sizeof(picker_state()->recents[0]), "%s", relative);
  ui_picker_palette_save();
}

static void ui_file_picker_refresh(UIFilePickerState *picker);

/* Seleziona un percorso relativo della palette: naviga nella sua cartella e
 * lo evidenzia (come un click sulla riga). */
static void ui_picker_select_relative(const char *relative) {
  char absolute[APP_PATH_MAX];
  int n = snprintf(absolute, sizeof(absolute), "%s/%s", picker_state()->root, relative);
  if (n <= 0 || (size_t)n >= sizeof(absolute))
    return;
  char canonical[APP_PATH_MAX];
  if (!platform_resolve_within(picker_state()->root, absolute, false, canonical, sizeof(canonical)) ||
      !platform_file_exists(canonical)) {
    snprintf(picker_state()->error, sizeof(picker_state()->error), "File non trovato: %.100s", relative);
    return;
  }
  /* Copia locale: sorgente e destinazione di snprintf sono oggetti distinti. */
  char root[APP_PATH_MAX];
  memcpy(root, picker_state()->root, sizeof(root));
  root[sizeof(root)-1] = '\0';
  const char *slash = strrchr(relative, '/');
  if (slash) {
    char dir[160];
    size_t dl = (size_t)(slash - relative);
    if (dl >= sizeof(dir)) dl = sizeof(dir) - 1;
    memcpy(dir, relative, dl);
    dir[dl] = '\0';
    int w = snprintf(picker_state()->current, sizeof(picker_state()->current), "%s/%s",
                     root, dir);
    if (w < 0 || (size_t)w >= sizeof(picker_state()->current))
      snprintf(picker_state()->current, sizeof(picker_state()->current), "%s", root);
  } else {
    snprintf(picker_state()->current, sizeof(picker_state()->current), "%s", root);
  }
  utf8_copy(picker_state()->filename, sizeof(picker_state()->filename), slash ? slash + 1 : relative);
  snprintf(picker_state()->selected, sizeof(picker_state()->selected), "%s", canonical);
  ui_file_picker_refresh(picker_state());
}

static bool ui_file_picker_capture_active(void) { return ui_state_picker(picker_context())->file_picker_open; }

void ui_file_picker_cancel(void) {
  UIPickerState *picker_view=ui_state_picker(picker_context());

  if (!picker_view->file_picker_state) return;
  char picker_id[sizeof(picker_state()->id)];
  snprintf(picker_id, sizeof(picker_id), "%s", picker_state()->id);
  if (picker_state()->image_preview)
    ui_context_clear_preview(picker_context());
  picker_state()->open = false;
  picker_view->file_picker_open=false;
  picker_view->file_picker_id[0]=0;
  picker_state()->overwrite_pending = false;
  ui_modal_end("file_picker_overwrite", false);
  if (picker_id[0])
    ui_modal_end(picker_id, false);
  /* ui_modal_end owns capture cleanup and restoration of the opener's focus.
   * Clearing it again here would erase the restored keyboard target. */
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
  char canonical[APP_PATH_MAX];
  if (!platform_resolve_within(picker->root, picker->current, false, canonical, sizeof(canonical)) ||
      !platform_dir_exists(canonical)) {
    snprintf(picker->error, sizeof(picker->error), "Cartella fuori dalla root o non accessibile");
    return;
  }
  snprintf(picker->current, sizeof(picker->current), "%s", canonical);
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
  char canonical[APP_PATH_MAX];
  return platform_resolve_within(picker->root, path, false, canonical, sizeof(canonical));
}

static bool ui_file_picker_build_path(const UIFilePickerState *picker,
                                      const char *name, char *out,
                                      size_t out_size) {
  if (!name || !name[0]) return false;
  char joined[APP_PATH_MAX];
  int written = snprintf(joined, sizeof(joined), "%s/%s", picker->current, name);
  return written >= 0 && (size_t)written < sizeof(joined) &&
         platform_resolve_within(picker->root, joined, picker->mode == UI_FILE_PICKER_SAVE,
                                  out, out_size);
}

/* Ogni contesto possiede un picker, allocato al primo utilizzo. La richiesta configura navigazione e filtro;
 * la conferma restituisce un percorso, non apre o salva il contenuto del file. */
void ui_file_picker_open(const char *id, const char *title,
                         const UIFilePickerRequest *request) {
  UIPickerState *picker_view=ui_state_picker(picker_context());

  if (!ui_valid_id(id)) return ;
  if (!id || !request || !request->root_directory || !request->root_directory[0])
    return;
  if (ui_file_picker_capture_active())
    ui_file_picker_cancel();
  if (!picker_view->file_picker_state) {
    picker_view->file_picker_state=calloc(1,sizeof(UIFilePickerState));
    if (!picker_view->file_picker_state) return;
  }
  memset(picker_state(), 0, sizeof(*picker_state()));
  snprintf(picker_state()->id, sizeof(picker_state()->id), "%s", id);
  snprintf(picker_state()->title, sizeof(picker_state()->title), "%s",
           title ? title : "FILE");
  if (!platform_real_path(request->root_directory, picker_state()->root, sizeof(picker_state()->root)) ||
      !platform_dir_exists(picker_state()->root)) return;
  const char *initial = request->initial_directory && request->initial_directory[0]
                            ? request->initial_directory : picker_state()->root;
  if (!platform_resolve_within(picker_state()->root, initial, false,
                               picker_state()->current, sizeof(picker_state()->current)) ||
      !platform_dir_exists(picker_state()->current))
    snprintf(picker_state()->current, sizeof(picker_state()->current), "%s", picker_state()->root);
  snprintf(picker_state()->extension, sizeof(picker_state()->extension), "%s",
           request->extension ? request->extension : "");
  utf8_copy(picker_state()->filename, sizeof(picker_state()->filename),
           request->initial_name ? request->initial_name : "");
  picker_state()->mode = request->mode;
  picker_state()->image_preview = request->image_preview;
  snprintf(picker_state()->favorites_key, sizeof(picker_state()->favorites_key), "%s",
           request->favorites_key ? request->favorites_key : "");
  ui_picker_palette_load();
  picker_state()->open = true;
  picker_view->file_picker_open=true;
  snprintf(picker_view->file_picker_id,sizeof(picker_view->file_picker_id),"%s",id);
  ui_file_picker_refresh(picker_state());
}

bool ui_file_picker_selected_path(const char *id, char *out_path, size_t out_path_size) {
  if (!ui_file_picker_is_open(id) || !out_path || out_path_size == 0 ||
      !picker_state()->selected[0])
    return false;
  return platform_resolve_within(picker_state()->root, picker_state()->selected,
                                  picker_state()->mode == UI_FILE_PICKER_SAVE, out_path, out_path_size);
}

bool ui_file_picker_is_open(const char *id) {
  return id && ui_state_picker(picker_context())->file_picker_state && picker_state()->open && strcmp(picker_state()->id, id) == 0;
}

static bool ui_file_picker_confirm_overwrite(Font *font,
                                             char *out_path,
                                             size_t out_path_size) {
  if (!picker_state()->overwrite_pending)
    return false;
  bool confirm_open = true;
  float x, y, w, h;
  if (!ui_modal_begin("file_picker_overwrite", "SOVRASCRIVERE FILE?",
                      font, 460.0f, 150.0f, &confirm_open, &x, &y, &w, &h)) {
    picker_state()->overwrite_pending = false;
    ui_modal_end("file_picker_overwrite", false);
    return false;
  }
  UILayout layout = ui_layout_begin((UIRect){x, y, w, h}, 1.0f);
  UITheme theme = ui_get_theme();
  ui_paint_draw_text_colored(font, "Il file esiste gia'. Questa azione non e' annullabile.",
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
  if (ui_button_labeled("overwrite_cancel", "ANNULLA", font,
                        ANCHOR_TOP_LEFT, 0, 14.0f, cancel.x, cancel.y,
                        &cancel_style))
    action |= 1u;
  if (ui_button_labeled("overwrite_confirm", "SOVRASCRIVI", font,
                        ANCHOR_TOP_LEFT, 0, 14.0f, overwrite.x, overwrite.y,
                        &overwrite_style))
    action |= 2u;
  if (action & 1u) {
    picker_state()->overwrite_pending = false;
    ui_modal_end("file_picker_overwrite", false);
  } else if (action & 2u) {
    if (!ui_file_picker_selected_path(picker_state()->id, out_path, out_path_size)) return false;
    ui_file_picker_cancel();
    return true;
  }
  ui_modal_end("file_picker_overwrite", picker_state()->overwrite_pending);
  return false;
}

bool ui_file_picker_draw(Font *font, char *out_path,
                         size_t out_path_size) {
  if (!ui_state_picker(picker_context())->file_picker_state || !picker_state()->open || !out_path || out_path_size == 0)
    return false;
  if (picker_state()->overwrite_pending)
    return ui_file_picker_confirm_overwrite(font, out_path,
                                            out_path_size);

  bool open = true;
  float x, y, width, height;
  if (!ui_modal_begin(picker_state()->id, picker_state()->title, font,
                      760.0f, 620.0f, &open, &x, &y, &width, &height)) {
    ui_file_picker_cancel();
    return false;
  }
  UIPanel panel = ui_panel_begin("file_picker_panel", "", font,
                                 (UIRect){x, y, width, height}, 1.0f);
  if (!panel.clip_pushed) {
    ui_panel_end(&panel);
    ui_modal_end(picker_state()->id, picker_state()->open);
    return false;
  }
  UILayout top = panel.layout;
  UITheme theme = ui_get_theme();
  UIPropertyLayout properties = ui_property_layout_begin(
      top.bounds.x, top.cursor_y, top.bounds.w, 1.0f);
  const char *navigation[] = {"ROOT", "SU", "AGGIORNA"};
  unsigned navigation_action = ui_property_action_row(&properties, "navigation", font, navigation, 3);
  if (navigation_action & 1u) {
    snprintf(picker_state()->current, sizeof(picker_state()->current), "%s",
             picker_state()->root);
    ui_file_picker_refresh(picker_state());
  }
  if (navigation_action & 2u) {
    char *slash = strrchr(picker_state()->current, '/');
    if (slash) {
      if (slash == picker_state()->current) slash[1] = '\0'; else *slash = '\0';
      if (!ui_file_picker_path_is_inside_root(picker_state(),
                                              picker_state()->current))
        snprintf(picker_state()->current, sizeof(picker_state()->current), "%s",
                 picker_state()->root);
      ui_file_picker_refresh(picker_state());
    }
  }
  if (navigation_action & 4u)
    ui_file_picker_refresh(picker_state());
  bool path_changed = ui_property_text(&properties, "directory", font,
                                       "PERCORSO", picker_state()->current,
                                       sizeof(picker_state()->current));
  if (path_changed) {
    if (!ui_file_picker_path_is_inside_root(picker_state(),
                                            picker_state()->current))
      snprintf(picker_state()->current, sizeof(picker_state()->current), "%s",
               picker_state()->root);
    ui_file_picker_refresh(picker_state());
  }
  if (ui_property_text(&properties, "search", font, "CERCA",
                       picker_state()->search, sizeof(picker_state()->search)))
    ui_file_picker_refresh(picker_state());
  ui_property_layout_commit(&top, &properties);

  char filter_label[96];
  snprintf(filter_label, sizeof(filter_label), "%d elementi  |  filtro: %s",
           picker_state()->entry_count,
           picker_state()->extension[0] ? picker_state()->extension : "tutti i file");
  float filter_scale = 12.0f;
  float filter_width = ui_paint_measure_text_width(font, filter_label, filter_scale);
  ui_paint_draw_text_colored(font, filter_label,
                    top.bounds.x + top.bounds.w - filter_width,
                    top.cursor_y + ui_paint_font_ascent(font, filter_scale),
                    filter_scale, theme.text_muted.r,
                    theme.text_muted.g, theme.text_muted.b);
  top.cursor_y += theme.compact_button_height;

  int footer_rows = picker_state()->mode == UI_FILE_PICKER_SAVE ? 2 : 1;
  float action_height = footer_rows * (theme.row_height + theme.row_spacing) +
                        (picker_state()->error[0] ? theme.compact_button_height
                                                : 0.0f);
  bool palette = picker_state()->favorites_key[0] != '\0';
  float preview_gap = (picker_state()->image_preview || palette) ? theme.row_spacing : 0.0f;
  float preview_width = picker_state()->image_preview
                            ? fminf(260.0f, top.bounds.w * 0.38f)
                            : (palette ? fminf(280.0f, top.bounds.w * 0.40f) : 0.0f);
  UIRect list_bounds = {top.bounds.x, top.cursor_y,
                        top.bounds.w - preview_width - preview_gap,
                        fmaxf(0.0f, top.bounds.y + top.bounds.h - top.cursor_y -
                                           action_height)};
  UIScrollScope list = ui_scroll_begin("file_entries", list_bounds, 1.0f);
  if (!list.clip_pushed) {
    ui_scroll_end(&list);
    ui_panel_end(&panel);
    ui_modal_end(picker_state()->id, picker_state()->open);
    return false;
  }
  UIPropertyLayout rows = ui_property_layout_begin(
      list.layout.bounds.x, list.layout.cursor_y, list.layout.bounds.w, 1.0f);
  for (int i = 0; i < picker_state()->entry_count; ++i) {
    UIFilePickerEntry *entry = &picker_state()->entries[i];
    if (!ui_push_id(entry->name)) continue;
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
        picker_state(), entry->name, entry_path, sizeof(entry_path));
    bool selected = !entry->directory && has_entry_path &&
                    !strcmp(entry_path, picker_state()->selected);
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
    bool chosen = ui_button_labeled(ui_scoped_id("entry"), label, font,
                                    ANCHOR_TOP_LEFT, 0, 14.0f,
                                    rows.x, row_y, &row_style);
    ui_pop_id();
    if (!chosen)
      continue;
    if (entry->directory) {
      char next[APP_PATH_MAX];
      if (ui_file_picker_build_path(picker_state(), entry->name, next,
                                    sizeof(next))) {
        snprintf(picker_state()->current, sizeof(picker_state()->current), "%s", next);
        picker_state()->selected[0] = '\0';
        ui_file_picker_refresh(picker_state());
      }
    } else {
      utf8_copy(picker_state()->filename, sizeof(picker_state()->filename), entry->name);
      ui_file_picker_build_path(picker_state(), entry->name,
                                picker_state()->selected,
                                sizeof(picker_state()->selected));
    }
  }
  ui_property_layout_commit(&list.layout, &rows);
  ui_scroll_end(&list);

  if (picker_state()->image_preview) {
    UIRect preview = {list_bounds.x + list_bounds.w + preview_gap,
                      list_bounds.y, preview_width, list_bounds.h};
    ui_paint_draw_quad(preview.x - theme.border_thickness,
              preview.y - theme.border_thickness,
              preview.w + theme.border_thickness * 2.0f,
              preview.h + theme.border_thickness * 2.0f,
              theme.border.r, theme.border.g, theme.border.b);
    ui_paint_draw_quad(preview.x, preview.y, preview.w, preview.h,
              theme.field.r, theme.field.g, theme.field.b);
    float title_scale = 13.0f * theme.secondary_text_scale;
    ui_paint_draw_text_colored(font, "ANTEPRIMA", preview.x + theme.row_spacing,
                      preview.y + theme.row_spacing + ui_paint_font_ascent(font, title_scale),
                      title_scale, theme.text_muted.r, theme.text_muted.g,
                      theme.text_muted.b);

    if (picker_state()->selected[0] && !ui_file_picker_path_is_inside_root(picker_state(), picker_state()->selected))
      picker_state()->selected[0] = '\0';
    if (picker_state()->selected[0]) {
      ui_state_render(picker_context())->preview_owned=true;
      int preview_slot = ui_paint_texture_manager_preview_load(picker_state()->selected);
      int texture_width = 0, texture_height = 0;
      if (preview_slot >= 0 &&
          ui_paint_texture_manager_get_size(preview_slot, &texture_width,
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
        ui_paint_draw_textured_quad(area_x + (area_w - draw_w) * 0.5f,
                           area_y + (area_h - draw_h) * 0.5f, draw_w, draw_h,
                           preview_slot, 0.0f, 0.0f, 1.0f, 1.0f);
      } else {
        ui_paint_draw_text_colored(font, "Immagine non caricabile",
                          preview.x + theme.row_spacing,
                          preview.y + preview.h * 0.5f, title_scale,
                          theme.warning.r, theme.warning.g, theme.warning.b);
      }
    } else {
      ui_paint_draw_text_colored(font, "Seleziona un PNG",
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
    ui_paint_draw_quad(pal.x - theme.border_thickness, pal.y - theme.border_thickness,
              pal.w + theme.border_thickness * 2.0f, pal.h + theme.border_thickness * 2.0f,
              theme.border.r, theme.border.g, theme.border.b);
    ui_paint_draw_quad(pal.x, pal.y, pal.w, pal.h, theme.field.r, theme.field.g, theme.field.b);
    UIScrollScope palScroll = ui_scroll_begin("picker_palette", pal, 1.0f);
    if (!palScroll.clip_pushed) {
      ui_scroll_end(&palScroll);
      ui_panel_end(&panel);
      ui_modal_end(picker_state()->id, picker_state()->open);
      return false;
    }
    UIPropertyLayout prow = ui_property_layout_begin(palScroll.layout.bounds.x + theme.row_spacing,
                                                     palScroll.layout.cursor_y,
                                                     palScroll.layout.bounds.w - theme.row_spacing * 2.0f,
                                                     1.0f);
    const char *selRel = picker_state()->selected[0] ? ui_picker_relative(picker_state()->selected) : "";
    bool isFav = selRel[0] && ui_picker_favorite_index(selRel) >= 0;
    {
      float ry = ui_property_layout_next_row(&prow);
      UIButtonStyle st = {.min_width = prow.width, .max_width = prow.width,
                          .min_height = prow.row_height, .selected = isFav};
      const char *lbl = !selRel[0] ? "* (evidenzia un file)"
                        : isFav ? "* TOGLI DAI PREFERITI" : "* AGGIUNGI AI PREFERITI";
      if (ui_button_labeled(ui_scoped_id("palette/fav_toggle"), lbl, font, ANCHOR_TOP_LEFT, 0,
                            13.0f, prow.x, ry, &st) && selRel[0])
        ui_picker_favorite_toggle(selRel);
    }
    const char *clicked = NULL;

    for (int section = 0; section < 2; ++section) {
      int count = section == 0 ? picker_state()->favorite_count : picker_state()->recent_count;
      float hy = ui_property_layout_next_row(&prow);
      char head[48];
      snprintf(head, sizeof(head), "%s (%d)", section == 0 ? "PREFERITI" : "RECENTI", count);
      ui_paint_draw_text_colored(font, head, prow.x, hy + ui_paint_font_ascent(font, 13.0f) + 4.0f, 13.0f,
                        theme.text_muted.r, theme.text_muted.g, theme.text_muted.b);
      for (int i = 0; i < count; ++i) {
        const char *rel = section == 0 ? picker_state()->favorites[i] : picker_state()->recents[i];
        char id[32];
        snprintf(id, sizeof(id), "p%d_%d", section, i);
        if (!ui_push_id(id)) continue;
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
          if (ui_button_labeled(ui_scoped_id("entry"), label, font, ANCHOR_TOP_LEFT, 0, 13.0f,
                                prow.x, ry, &st)) {
            snprintf(picker_state()->clicked_path, sizeof(picker_state()->clicked_path), "%s", rel);
            clicked = picker_state()->clicked_path;
          }
        }
        ui_pop_id();
      }
    }
    ui_property_layout_commit(&palScroll.layout, &prow);
    ui_scroll_end(&palScroll);
    if (clicked)
      ui_picker_select_relative(clicked);
  }

  UIPropertyLayout actions = ui_property_layout_begin(
      top.bounds.x, list_bounds.y + list_bounds.h, top.bounds.w, 1.0f);
  if (picker_state()->mode == UI_FILE_PICKER_SAVE)
    ui_property_text(&actions, "filename", font, "NOME FILE",
                     picker_state()->filename, sizeof(picker_state()->filename));
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
  if (ui_button_labeled(ui_scoped_id("actions/cancel"), "ANNULLA", font,
                        ANCHOR_TOP_LEFT, 0, 14.0f, actions.x, action_y,
                        &cancel_style))
    action |= 1u;
  const char *confirm_label = picker_state()->mode == UI_FILE_PICKER_SAVE
                                  ? "SALVA"
                                  : (picker_state()->mode == UI_FILE_PICKER_DIRECTORY
                                         ? "SELEZIONA CARTELLA"
                                         : "APRI");
  if (ui_button_labeled(ui_scoped_id("actions/confirm"), confirm_label,
                        font, ANCHOR_TOP_LEFT, 0, 14.0f,
                        actions.x + action_w + action_gap, action_y,
                        &confirm_style))
    action |= 2u;
  if (action & 1u) {
    ui_file_picker_cancel();
  } else if (action & 2u) {
    char selected[APP_PATH_MAX] = "";
    if (picker_state()->mode == UI_FILE_PICKER_DIRECTORY) {
      if (platform_resolve_within(picker_state()->root, picker_state()->current, false, out_path, out_path_size) &&
          platform_dir_exists(out_path)) {
        ui_file_picker_cancel(); ui_panel_end(&panel); return true;
      }
      snprintf(picker_state()->error, sizeof(picker_state()->error), "Cartella non valida o percorso troppo lungo");
    } else if (picker_state()->mode == UI_FILE_PICKER_SAVE) {
      snprintf(selected, sizeof(selected), "%s", picker_state()->filename);
      size_t name_length = strlen(selected), extension_length = strlen(picker_state()->extension);
      if (picker_state()->extension[0] &&
          (name_length < extension_length ||
           strcmp(selected + name_length - extension_length,
                  picker_state()->extension) != 0))
        strncat(selected, picker_state()->extension,
                sizeof(selected) - strlen(selected) - 1);
      if (!ui_file_picker_build_path(picker_state(), selected,
                                     picker_state()->selected,
                                     sizeof(picker_state()->selected)))
        snprintf(picker_state()->error, sizeof(picker_state()->error),
                 "Nome file non valido");
      else {
        if (platform_file_exists(picker_state()->selected))
          picker_state()->overwrite_pending = true;
        else {
          if (!ui_file_picker_selected_path(picker_state()->id, out_path, out_path_size)) {
            ui_panel_end(&panel); return false;
          }
          ui_file_picker_cancel();
          ui_panel_end(&panel);
          return true;
        }
      }
    } else if (picker_state()->selected[0]) {
      if (!ui_file_picker_selected_path(picker_state()->id, out_path, out_path_size)) {
        ui_panel_end(&panel); return false;
      }
      ui_picker_recent_push(ui_picker_relative(picker_state()->selected));
      ui_file_picker_cancel();
      ui_panel_end(&panel);
      return true;
    }
  }
  if (picker_state()->error[0]) {
    float row = ui_property_layout_next_row(&actions);
    ui_paint_draw_text_colored(font, picker_state()->error, actions.x,
                      row + (actions.row_height + ui_paint_font_ascent(font, 13.0f)) * 0.5f,
                      13.0f, theme.error.r, theme.error.g, theme.error.b);
  }
  ui_panel_end(&panel);
  ui_modal_end(picker_state()->id, picker_state()->open);
  return false;
}
