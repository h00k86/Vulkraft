#include "app/application.h"
#include "ui/ui.h"
#include "render/text_renderer.h"
#include <stdio.h>
#include <string.h>
// Composition of existing canonical widgets, from game_draw_hud.
void app_draw(APP *app) {
  static char text[256] = "La tua applicazione parte da qui";
  static int clicks = 0;
  static bool light = false, checked = true;
  static float value = 0.5f;
  UITheme theme = ui_get_theme();
  Font *font = &app->renderer.font;
  draw_quad(app, 0, 0, VIRTUAL_W, VIRTUAL_H, theme.editor_background.r, theme.editor_background.g, theme.editor_background.b);
  float pad = 24.0f;
  float width = VIRTUAL_W - pad * 2;
  if (width < 20) return;
  draw_text_colored(app, font, "Graphics App Base", pad, 45, 28, theme.text.r, theme.text.g, theme.text.b);
  draw_text_colored(app, font, "C  /  Platform  /  Vulkan", pad, 73, 15, theme.text_muted.r, theme.text_muted.g, theme.text_muted.b);
  UIRect bounds = {pad, 96, width, VIRTUAL_H - 120};
  UIScrollScope scroll = ui_scroll_begin("starter/content", bounds, 1);
  if (!scroll.clip_pushed) { ui_scroll_end(&scroll); return; }
  UILayout layout = scroll.layout;
  float y = ui_layout_next_row(&layout);
  draw_text_colored(app, font, "Testo, input, pulsanti e layout riutilizzabili", pad + 8, y + 18, 17, theme.text.r, theme.text.g, theme.text.b);
  y = ui_layout_next_row(&layout);
  UIButtonStyle field = {.min_height = 34};
  ui_text_input("starter/text", font, ANCHOR_TOP_LEFT, 0, 18, pad + 8, y, width - 32, text, sizeof(text), &field);
  ui_layout_space(&layout, 16);
  y = ui_layout_next_row(&layout);
  UIButtonStyle button = {.min_width = 190, .max_width = 190, .min_height = 34};
  if (ui_button_labeled("starter/button", "Premi qui", font, ANCHOR_TOP_LEFT, 0, 18, pad + 8, y, &button)) clicks++;
  y = ui_layout_next_row(&layout);
  char message[96]; snprintf(message, sizeof(message), "Click: %d", clicks);
  draw_text_colored(app, font, message, pad + 8, y + 18, 17, theme.text.r, theme.text.g, theme.text.b);
  y = ui_layout_next_row(&layout);
  if (ui_button_labeled("starter/theme", light ? "Tema scuro" : "Tema chiaro", font, ANCHOR_TOP_LEFT, 0, 18, pad + 8, y, &button)) {
    light = !light; ui_set_theme(light ? ui_theme_light() : ui_theme_dark());
  }
  ui_layout_space(&layout, 16);
  UIPropertyLayout props = ui_property_layout_begin(pad + 8, layout.cursor_y, width - 32, 1);
  ui_property_checkbox(&props, "starter/enabled", font, "Opzione", &checked);
  ui_property_float(&props, "starter/value", font, "Valore", &value, 0, 1);
  ui_property_layout_commit(&layout, &props);
  y = ui_layout_next_row(&layout);
  draw_text_colored(app, font, text, pad + 8, y + 18, 18, theme.accent.r, theme.accent.g, theme.accent.b);
  scroll.layout = layout;
  ui_scroll_end(&scroll);
  draw_text_colored(app, font, "Ridimensiona la finestra: canvas e puntatore restano allineati.", pad, VIRTUAL_H - 16, 14, theme.text_muted.r, theme.text_muted.g, theme.text_muted.b);
}
