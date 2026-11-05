#include "ui/ui.h"

/* Layout arithmetic and theme metrics; no widget state or renderer access. */
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

float ui_property_layout_next_row(UIPropertyLayout *layout) {
  if (!layout) return 0.0f;
  float y = layout->cursor_y;
  layout->cursor_y += layout->row_height + ui_get_theme().row_spacing * layout->scale;
  return y;
}

void ui_property_layout_gap(UIPropertyLayout *layout, float amount) {
  if (layout && amount > 0.0f) layout->cursor_y += amount * layout->scale;
}
