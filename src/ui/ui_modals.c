#include "ui_state_navigation_internal.h"
#include "ui_state_modals_internal.h"
#include "ui_state_frame_internal.h"
#include "ui/ui.h"
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
void ui_modal_input_begin_frame(void) {
  UIModalsState *modals_view=ui_state_modals(ui_overlay_context());

  for (int i = 0; i < modals_view->g_modal_state_count; ++i)
    modals_view->g_modal_states[i].submittedThisFrame = false;
}

void ui_modal_input_end_frame(void) {
  UIModalsState *modals_view=ui_state_modals(ui_overlay_context());

  bool closed_stale_modal = false;
  for (int i = 0; i < modals_view->g_modal_state_count; ++i) {
    if (modals_view->g_modal_states[i].wasOpen &&
        !modals_view->g_modal_states[i].submittedThisFrame) {
      modals_view->g_modal_states[i].wasOpen = false;
      closed_stale_modal = true;
    }
  }
  if (closed_stale_modal && !ui_any_modal_open() &&
      !ui_file_picker_capture_active())
    ui_release_input_capture();
}

bool ui_any_modal_open(void) {
  UIModalsState *modals_view=ui_state_modals(ui_overlay_context());

  for (int i = 0; i < modals_view->g_modal_state_count; ++i) {
    if (modals_view->g_modal_states[i].wasOpen)
      return true;
  }
  return false;
}

void ui_modal_end(const char *id, bool is_open) {
  UIModalsState *modals_view=ui_state_modals(ui_overlay_context());
  UINavigationState *navigation_view=ui_state_navigation(ui_overlay_context());

  if (!ui_valid_id(id)) return ;
  if (!id || !id[0])
    return;
  for (int i = 0; i < modals_view->g_modal_state_count; ++i) {
    if (strcmp(modals_view->g_modal_states[i].id, id) != 0)
      continue;
    bool closed_now = modals_view->g_modal_states[i].wasOpen && !is_open;
    modals_view->g_modal_states[i].wasOpen = is_open;
    modals_view->g_modal_states[i].submittedThisFrame = is_open;
    if (modals_view->g_modal_states[i].scopeOpen) {
      modals_view->g_modal_states[i].scopeOpen = false;
      if(navigation_view->nav_scope_depth>0) --navigation_view->nav_scope_depth;
      if (modals_view->g_modal_scope_depth > 0)
        modals_view->g_modal_scope_depth--;
    }
    if (closed_now && !ui_any_modal_open() &&
        !ui_file_picker_capture_active())
      ui_release_input_capture();
    if(!is_open) ui_nav_restore(ui_overlay_context(), &modals_view->g_modal_states[i]);
    return;
  }
}

/* Il chiamante possiede is_open e compone il contenuto nel rettangolo
 * restituito. Terminare la gestione con ui_modal_end: possesso dell'input
 * e visibilita della modale devono rimanere coerenti nello stesso frame. */
bool ui_modal_begin(const char *id, const char *title, Font *font,
                    float width, float height, bool *is_open,
                    float *out_content_x, float *out_content_y,
                    float *out_content_w, float *out_content_h) {
  UIModalsState *modals_view=ui_state_modals(ui_overlay_context());
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

  if (!ui_valid_id(id)) return false;
  // Modale sopra popup: una lista aperta nel normale HUD viene chiusa QUANDO
  // il dialogo si apre. Non farlo a ogni frame: alcuni dialoghi contengono a
  // loro volta dropdown, che devono poter restare aperti dal frame seguente.
  UIModalOpenState *modalState = NULL;
  for (int i = 0; i < modals_view->g_modal_state_count; ++i) {
    if (strcmp(modals_view->g_modal_states[i].id, id) == 0) {
      modalState = &modals_view->g_modal_states[i];
      break;
    }
  }
  if (!modalState && modals_view->g_modal_state_count < MAX_UI_MODALS) {
    modalState = &modals_view->g_modal_states[modals_view->g_modal_state_count++];
    memset(modalState, 0, sizeof(*modalState));
    strncpy(modalState->id, id, sizeof(modalState->id) - 1);
  }
  if (!*is_open) {
    if (modalState) {
      modalState->wasOpen = false;
      modalState->submittedThisFrame = false;
      ui_nav_restore(ui_overlay_context(), modalState);
    }
    return false;
  }
  if (ui_capture_modal_escape_allowed(ui_overlay_context(),id) && ui_capture_consume_escape(ui_overlay_context())) {
    *is_open = false;
    if (modalState) {
      modalState->wasOpen = false;
      modalState->submittedThisFrame = false;
    }
    if (!ui_any_modal_open() && !ui_file_picker_capture_active())
      ui_release_input_capture();
    if(modalState) ui_nav_restore(ui_overlay_context(), modalState);
    return false;
  }
  if (!modalState || !modalState->wasOpen) {
    ui_close_all_dropdowns();
    ui_menu_close_hierarchy();
  }
  if (modalState) {
    ui_nav_modal_enter(ui_overlay_context(), modalState);
    modalState->wasOpen = true;
    modalState->submittedThisFrame = true;
    modalState->scopeOpen = true;
  }
  modals_view->g_modal_scope_depth++; /* da qui fino a ui_modal_end: widget del modale */

  UITheme theme = ui_get_theme();
  float screen_margin = theme.modal_padding;
  width = fminf(width, frame_view->canvas_width - screen_margin * 2.0f);
  height = fminf(height, frame_view->canvas_height - screen_margin * 2.0f);
  float x = (frame_view->canvas_width - width) * 0.5f;
  float y = (frame_view->canvas_height - height) * 0.5f;
  float titleBarH = theme.panel_header_height + theme.row_spacing;
  float outer_border = theme.border_thickness * 2.0f;

  /* MACROTASK UI residui: sfondo SEMITRASPARENTE (draw_quad_alpha, la
   * pipeline HUD ha il blending): l'editor sotto resta visibile ma
   * scurito; i suoi widget sono inerti (ui_pointer_hit). */
  ui_paint_draw_quad_alpha(0.0f, 0.0f, frame_view->canvas_width, frame_view->canvas_height,
                  theme.editor_background.r * 0.25f,
                  theme.editor_background.g * 0.25f,
                  theme.editor_background.b * 0.25f, 0.62f);

  // Rettangolo del modale visibile a ui_scroll_begin (la rotellina scorre
  // solo i pannelli dentro il modale finche' questo e' aperto).
  modals_view->g_modal_rect = (UIRect){x, y, width, height};
  modals_view->g_modal_rect_seen_this_frame = true;

  // Bordo esterno + corpo del pannello
  ui_paint_draw_quad(x - outer_border, y - outer_border,
            width + outer_border * 2.0f, height + outer_border * 2.0f,
            theme.border.r, theme.border.g, theme.border.b);
  ui_paint_draw_quad(x, y, width, height, theme.panel.r, theme.panel.g,
            theme.panel.b);

  // Barra del titolo, leggermente più chiara del corpo per distinguerla
  ui_paint_draw_quad(x, y, width, titleBarH, theme.panel_header.r,
            theme.panel_header.g, theme.panel_header.b);
  ui_paint_draw_quad(x, y, 3.0f, titleBarH, theme.accent.r, theme.accent.g,
            theme.accent.b);
  float titleScale = 16.0f * theme.section_text_scale;
  float titleAscent = ui_paint_font_ascent(font, titleScale);
  ui_paint_draw_text_colored(font, title, x + theme.modal_padding,
                    y + (titleBarH + titleAscent) * 0.5f, titleScale, theme.text.r,
                    theme.text.g, theme.text.b);

  // Bottone di chiusura (X), in cima a destra della barra del titolo
  char closeId[UI_ID_CAPACITY + 32];
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
  if (ui_button_labeled(closeId, "X", font, ANCHOR_TOP_LEFT, 0, 14.0f,
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
    if(modalState) ui_nav_restore(ui_overlay_context(), modalState);
    return false;
  }

  *out_content_x = x + theme.modal_padding;
  *out_content_y = y + titleBarH + theme.modal_padding;
  *out_content_w = width - theme.modal_padding * 2.0f;
  *out_content_h = height - titleBarH - theme.modal_padding * 2.0f;
  return true;
}
