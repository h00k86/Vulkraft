#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "text/font_types.h"
/* Optional module: link ui_file_picker.c and its filesystem adapters. */
/* One bounded, cached project-file workflow for ordinary editor assets.
 * The caller owns the semantic root/filter and consumes the confirmed full
 * path; the picker owns navigation, selection, search and overlay capture. */
typedef enum {
  UI_FILE_PICKER_OPEN,
  UI_FILE_PICKER_SAVE,
  UI_FILE_PICKER_DIRECTORY,
} UIFilePickerMode;

typedef struct {
    UIFilePickerMode mode;
    const char *root_directory;
    const char *initial_directory;
    const char *extension; /* ".dat", ".pfx", ...; NULL accepts all files. */
    const char *initial_name;
    bool image_preview; /* mostra una sola preview GPU per il file selezionato */
    /* 2026-09-18 (kit modulari): chiave della PALETTE preferiti/recenti
     * (es. "models"): NULL = nessuna palette. I percorsi sono salvati
     * relativi a root_directory in <config utente>/picker_<chiave>.txt. */
    const char *favorites_key;
} UIFilePickerRequest;

void ui_file_picker_open(const char *id, const char *title,
                         const UIFilePickerRequest *request);
bool ui_file_picker_is_open(const char *id);
/* Programmatic cancellation uses the same cleanup path as the Cancel button.
 * Primarily useful for bounded development tooling and tests. */
void ui_file_picker_cancel(void);
/* Returns true once when OPEN/SAVE/DIRECTORY is confirmed and writes the
 * absolute selected path.  Directory contents are refreshed only on open or
 * navigation, never while the picker is drawn. */
bool ui_file_picker_draw(Font *font, char *out_path,
                         size_t out_path_size);
/* Selezione CORRENTE (non ancora confermata) del picker aperto con quell'id:
 * true e il percorso assoluto se un file e' evidenziato. Serve alle
 * anteprime "dal vivo" (oggetto 3D che cambia mesh mentre scorri l'elenco). */
bool ui_file_picker_selected_path(const char *id, char *out_path, size_t out_path_size);
