#include "ui/ui.h"
#include <math.h>

// La swapchain usa VK_FORMAT_B8G8R8A8_SRGB: i colori del tema sono in
// spazio LINEARE e la GPU li ri-codifica in sRGB alla scrittura. La
// palette è definita in valori sRGB e
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
  // Tema scuro con accento viola #ab34eb. Hover, focus e selezione
  // derivano dal colore accent configurabile nel tema.
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
