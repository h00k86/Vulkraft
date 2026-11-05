#include "ui/ui.h"
#include "ui_paint_internal.h"
#include "ui_widget_internal.h"
#include <stdio.h>
#include <math.h>
#include <limits.h>

/* Compose public widgets; retained state and input routing stay in the core. */
void ui_property_label(const UIPropertyLayout *layout, Font *font,
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
  ui_paint_draw_text_colored(font, display, layout->x,
                    y + (layout->row_height + ui_paint_font_ascent(font, text_scale)) * 0.5f,
                    text_scale,
                    theme.text_muted.r, theme.text_muted.g, theme.text_muted.b);
}

void ui_property_value(UIPropertyLayout *layout, Font *font,
                       const char *label, const char *value) {
  if (!layout || !font) return;
  UITheme theme = ui_get_theme();
  float y = ui_property_layout_next_row(layout);
  ui_property_label(layout, font, label, y);
  float x = layout->x + layout->label_width;
  float w = layout->width - layout->label_width;
  float text_scale = 14.0f * layout->scale * theme.text_scale;
  char display[192];
  ui_ellipsize_right(font, value ? value : "", text_scale,
                     w - theme.label_value_gap * layout->scale, display,
                     sizeof(display));
  ui_paint_draw_text_colored(font, display, x + theme.label_value_gap * layout->scale,
                    y + (layout->row_height + ui_paint_font_ascent(font, text_scale)) * 0.5f,
                    text_scale, theme.text.r, theme.text.g, theme.text.b);
}

void ui_property_note(UIPropertyLayout *layout, Font *font,
                      const char *text) {
  if (!layout || !font) return;
  UITheme theme = ui_get_theme();
  float y = ui_property_layout_next_row(layout);
  float text_scale = 14.0f * layout->scale * theme.secondary_text_scale;
  char display[256];
  ui_ellipsize_right(font, text ? text : "", text_scale, layout->width, display,
                     sizeof(display));
  ui_paint_draw_text_colored(font, display, layout->x,
                    y + (layout->row_height + ui_paint_font_ascent(font, text_scale)) * 0.5f,
                    text_scale, theme.text_muted.r, theme.text_muted.g,
                    theme.text_muted.b);
}

bool ui_property_float(UIPropertyLayout *layout, const char *id,
                       Font *font, const char *label, float *value,
                       float min, float max) {
  if (!layout) return false;
  float y = ui_property_layout_next_row(layout);
  ui_property_label(layout, font, label, y);
  float valueX = layout->x + layout->label_width;
  return ui_float_field(ui_scoped_id(id), font, valueX, y,
                        layout->width - layout->label_width,
                        layout->row_height, 14.0f * layout->scale,
                        value, min, max);
}

bool ui_property_int(UIPropertyLayout *layout, const char *id,
                     Font *font, const char *label, int *value, int min, int max) {
  if (!layout || !value) return false;
  float y = ui_property_layout_next_row(layout);
  ui_property_label(layout, font, label, y);
  return ui_number_field(ui_scoped_id(id), font,
                         layout->x + layout->label_width, y,
                         layout->width - layout->label_width, layout->row_height,
                         14.0f * layout->scale, NULL, 0, 0, value, min, max);
}

bool ui_property_vec2(UIPropertyLayout *layout, const char *id,
                      Font *font, const char *label, Vec2 *value,
                      float min, float max) {
  if (!ui_valid_id(id)) return false;
  if (!layout || !value) return false;
  float y = ui_property_layout_next_row(layout);
  ui_property_label(layout, font, label, y);
  float x = layout->x + layout->label_width;
  float gap = ui_get_theme().row_spacing * layout->scale;
  float w = (layout->width - layout->label_width - gap) * 0.5f;
  bool changed = ui_float_field(ui_scoped_id(id), font, x, y, w,
                                layout->row_height, 14.0f * layout->scale,
                                &value->x, min, max);
  char yid[UI_ID_CAPACITY + 32]; snprintf(yid, sizeof(yid), "%s/y", id);
  changed |= ui_float_field(ui_scoped_id(yid), font, x + w + gap, y, w,
                            layout->row_height, 14.0f * layout->scale,
                            &value->y, min, max);
  return changed;
}

bool ui_property_slider(UIPropertyLayout *layout, const char *id,
                        Font *font, const char *label, float *value,
                        float min, float max, bool *out_drag_started) {
  if (!layout) return false;
  float y = ui_property_layout_next_row(layout);
  return ui_slider(ui_scoped_id(id), font, ANCHOR_TOP_LEFT, 0, layout->scale,
                   layout->x, y, layout->width, value, min, max, label,
                   out_drag_started);
}

bool ui_property_toggle(UIPropertyLayout *layout, const char *id,
                        Font *font, const char *label, bool *value) {
  if (!layout) return false;
  float y = ui_property_layout_next_row(layout);
  ui_property_label(layout, font, label, y);
  return ui_toggle(ui_scoped_id(id), "", font, layout->x + layout->label_width, y,
                   layout->scale, layout->width - layout->label_width, value);
}

bool ui_property_checkbox(UIPropertyLayout *layout, const char *id,
                          Font *font, const char *label, bool *value) {
  if (!layout) return false;
  float y = ui_property_layout_next_row(layout);
  return ui_checkbox(ui_scoped_id(id), label, font,
                     layout->x, y, 14.0f * layout->scale, value);
}

bool ui_property_dropdown(UIPropertyLayout *layout, const char *id,
                          Font *font, const char *label,
                          const char *const *options, int option_count,
                          int *selected) {
  if (!layout) return false;
  float y = ui_property_layout_next_row(layout);
  ui_property_label(layout, font, label, y);
  UIButtonStyle style = {.min_width = layout->width - layout->label_width,
                         .min_height = layout->row_height};
  return ui_dropdown(ui_scoped_id(id), font, ANCHOR_TOP_LEFT, 0, 14.0f * layout->scale,
                     layout->x + layout->label_width, y,
                     layout->width - layout->label_width, options, option_count,
                     selected, NULL, &style);
}

bool ui_property_text(UIPropertyLayout *layout, const char *id,
                      Font *font, const char *label, char *buffer,
                      size_t buffer_size) {
  if (!layout) return false;
  float y = ui_property_layout_next_row(layout);
  ui_property_label(layout, font, label, y);
  UIButtonStyle style = {.min_width = layout->width - layout->label_width,
                         .min_height = layout->row_height};
  return ui_text_input(ui_scoped_id(id), font, ANCHOR_TOP_LEFT, 0,
                       14.0f * layout->scale,
                       layout->x + layout->label_width, y,
                       layout->width - layout->label_width, buffer, buffer_size,
                       &style);
}

bool ui_property_action(UIPropertyLayout *layout, const char *id,
                        Font *font, const char *label, bool danger) {
  if (!layout) return false;
  float y = ui_property_layout_next_row(layout);
  UIButtonStyle style = {.min_width = layout->width,
                         .max_width = layout->width,
                         .min_height = layout->row_height,
                         .danger = danger};
  return ui_button_labeled(ui_scoped_id(id), label, font, ANCHOR_TOP_LEFT, 0,
                           14.0f * layout->scale, layout->x, y, &style);
}

unsigned ui_property_action_row(UIPropertyLayout *layout,
                                const char *id, Font *font,
                                const char *const *labels, int count) {
  if (!ui_valid_id(id)) return 0;
  if (!layout || !labels || count < 1) return 0;
  float y = ui_property_layout_next_row(layout);
  float gap = ui_get_theme().row_spacing * layout->scale;
  float width = (layout->width - gap * (count - 1)) / count;
  unsigned result = 0;
  for (int i = 0; i < count && i < 31; ++i) {
    UIRect r = {layout->x + i * (width + gap), y, width, layout->row_height}; char action_id[UI_ID_CAPACITY + 32];
    snprintf(action_id, sizeof(action_id), "%s/%d", id, i);
    UIButtonStyle style = {.min_width = r.w, .max_width = r.w,
                           .min_height = r.h};
    if (ui_button_labeled(ui_scoped_id(action_id), labels[i], font,
                          ANCHOR_TOP_LEFT, 0, 14.0f * layout->scale, r.x, r.y, &style)) result |= 1u << i;
  }
  return result;
}

bool ui_property_asset(UIPropertyLayout *layout, const char *id,
                       Font *font, const char *label, char *path,
                       size_t path_size, bool *out_browse) {
  if (!ui_valid_id(id)) return false;
  if (!layout || !path || path_size == 0) return false;
  if (out_browse) *out_browse = false;
  float y = ui_property_layout_next_row(layout);
  ui_property_label(layout, font, label, y);
  UITheme theme = ui_get_theme();
  float x = layout->x + layout->label_width;
  float browse_w = theme.button_height * layout->scale;
  float field_w = layout->width - layout->label_width - browse_w - theme.row_spacing * layout->scale;
  bool browse=false;
  bool hover=ui_input_hover_click((UIRect){x,y,field_w,layout->row_height},&browse);
  UIColor fill = hover ? theme.field_hover : theme.field;
  UIColor border = hover ? theme.accent : theme.border;
  float thickness = theme.border_thickness * layout->scale;
  ui_paint_draw_quad(x - thickness, y - thickness, field_w + thickness * 2.0f,
            layout->row_height + thickness * 2.0f, border.r, border.g,
            border.b);
  ui_paint_draw_quad(x, y, field_w, layout->row_height, fill.r, fill.g, fill.b);
  float text_scale = 14.0f * layout->scale * theme.text_scale;
  char display[192];
  ui_ellipsize_left(font, path[0] ? path : "Nessun asset", text_scale,
                    field_w - theme.label_value_gap * layout->scale * 2.0f,
                    display, sizeof(display));
  UIColor text_color = path[0] ? theme.text : theme.text_muted;
  ui_paint_draw_text_colored(font, display,
                    x + theme.label_value_gap * layout->scale,
                    y + (layout->row_height + ui_paint_font_ascent(font, text_scale)) * 0.5f,
                    text_scale, text_color.r, text_color.g, text_color.b);
  char browse_id[UI_ID_CAPACITY + 32]; snprintf(browse_id, sizeof(browse_id), "%s/browse", id);
  UIButtonStyle browse_style = {.min_width = browse_w, .max_width = browse_w,
                                .min_height = layout->row_height,
                                .selected = false};
  if (ui_button_labeled(ui_scoped_id(browse_id), "...", font, ANCHOR_TOP_LEFT, 0,
                        14.0f * layout->scale, x + field_w + theme.row_spacing * layout->scale, y, &browse_style)) {
    browse = true;
  }
  if (out_browse) *out_browse = browse;
  return browse;
}

bool ui_property_password(UIPropertyLayout *p,const char *id,Font *font,const char *label,char *buffer,size_t capacity,bool *revealed) {
  if(!p||!font||!label||!buffer||!revealed||capacity==0||capacity>INT_MAX||!ui_push_id(id)) return false;
  float y=ui_property_layout_next_row(p),x=p->x+p->label_width;
  float available=p->width-p->label_width,button=fminf(80*p->scale,available*.35f),gap=4*p->scale;
  ui_property_label(p,font,label,y);
  UIButtonStyle field={.min_height=p->row_height};
  bool confirmed=false;
  if(available>button+gap) confirmed=ui_text_input_mode(ui_scoped_id("text"),font,ANCHOR_TOP_LEFT,0,14*p->scale,
    x,y,available-button-gap,buffer,(int)capacity,&field,true,*revealed);
  UIButtonStyle style={.min_width=button,.max_width=button,.min_height=p->row_height};
  if(button>0&&ui_button_labeled(ui_scoped_id("reveal"),*revealed?"Nascondi":"Mostra",font,ANCHOR_TOP_LEFT,0,12*p->scale,
                               x+available-button,y,&style)) *revealed=!*revealed;
  ui_pop_id(); return confirmed;
}
