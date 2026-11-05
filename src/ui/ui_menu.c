#include "ui_state_menu_internal.h"
#include "ui_state_frame_internal.h"
#include "ui/ui.h"
#include "ui_error_internal.h"
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
static bool ui_menu_reserve_items(UIStoredMenuItem **items,int *capacity,int count) {
  if(count<=*capacity) return true;
  int next=*capacity?*capacity:4;
  while(next<count) next*=2;
  UIStoredMenuItem *storage=realloc(*items,(size_t)next*sizeof(*storage));
  if(!storage) return false;
  *items=storage; *capacity=next; return true;
}
static bool ui_menu_copy_items(UIStoredMenuItem **storage,int *capacity,int *out_count,
                               const UIMenuItem *items, int item_count, UIErrorOperation operation, const char *id) {
  UIMenuState *menu_view=ui_state_menu(ui_overlay_context());

  int count = item_count;
  if (count < 0 || (count && !items)) {
    ui_report_error(ui_overlay_context(),UI_ERROR_ARGUMENT,operation,id);
    *out_count=0; return true;
  }
  if (count > MAX_UI_MENU_ITEMS) {
    ui_report_error(ui_overlay_context(),UI_ERROR_LIMIT,operation,id);
    count = MAX_UI_MENU_ITEMS;
    menu_view->g_menu.overflowed = true;
#ifdef DEBUG
    LOG_WARN("Menu item capacity exceeded");
#endif
  }
  if(!ui_menu_reserve_items(storage,capacity,count)) return false;
  UIStoredMenuItem *out=*storage;
  for (int i = 0; i < count; ++i) {
    if (!ui_valid_id(items[i].id ? items[i].id : "item") ||
        (items[i].submenu_id && items[i].submenu_id[0] && !ui_valid_id(items[i].submenu_id))) {
      ui_report_error(ui_overlay_context(),UI_ERROR_ARGUMENT,operation,id);
      *out_count = 0; menu_view->g_menu.overflowed = true; return true;
    }
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
  return true;
}

static void ui_menu_close_from(int depth) {
  UIMenuState *menu_view=ui_state_menu(ui_overlay_context());

  if (depth < 0) depth = 0;
  if (depth > menu_view->g_menu.depth) {
#ifdef DEBUG
    LOG_WARN("Menu hierarchy close underflow: %d > %d", depth, menu_view->g_menu.depth);
#endif
    return;
  }
  for (int i = depth; i < UI_MENU_MAX_DEPTH; ++i) {
    UIMenuLevel *level=&menu_view->g_menu.levels[i];
    UIStoredMenuItem *items=level->items; int capacity=level->item_capacity;
    memset(level,0,sizeof(*level));
    level->items=items; level->item_capacity=capacity;
  }
  menu_view->g_menu.depth = depth;
}

void ui_menu_close_hierarchy(void) { ui_menu_close_from(0); }
bool ui_menu_hierarchy_is_open(void) { return ui_state_menu(ui_overlay_context())->g_menu.depth > 0; }

static void ui_menu_allocation_failed(int close_depth, UIErrorOperation operation, const char *id) {
  ui_report_error(ui_overlay_context(),UI_ERROR_MEMORY,operation,id);
  ui_menu_close_from(close_depth);
  ui_state_menu(ui_overlay_context())->g_menu.overflowed=true;
  if(ui_state_frame(ui_overlay_context())->g_mouse_clicked) ui_capture_claim_menu(ui_overlay_context());
}

static void ui_menu_open_root(const char *id, UIRect anchor, float width,
                              bool context_root, const UIMenuItem *items,
                              int item_count) {
  UIMenuState *menu_view=ui_state_menu(ui_overlay_context());

  if (!ui_valid_id(id)) {
    ui_report_error(ui_overlay_context(),UI_ERROR_ARGUMENT,UI_OPERATION_MENU_ROOT,id); return;
  }
  ui_close_all_dropdowns();
  ui_menu_close_hierarchy();
  UIMenuLevel *root = &menu_view->g_menu.levels[0];
  if(!ui_menu_copy_items(&root->items,&root->item_capacity,&root->item_count,items,item_count,UI_OPERATION_MENU_ROOT,id)) {
    ui_menu_allocation_failed(0,UI_OPERATION_MENU_ROOT,id); return;
  }
  root->active = true;
  root->context_root = context_root;
  root->depth = 0;
  root->anchor = anchor;
  root->width = width > 0.0f ? width : 220.0f;
  snprintf(root->id, sizeof(root->id), "%s", id ? id : "menu");
  menu_view->g_menu.depth = 1;
  menu_view->g_menu.root_opened_this_frame = true;
  menu_view->g_menu.root_submitted_this_frame = true;
}

void ui_menu_begin_frame(void) {
  UIMenuState *menu_view=ui_state_menu(ui_overlay_context());

  menu_view->g_menu.hovered_depth = -1;
  menu_view->g_menu.owning_depth = -1;
  menu_view->g_menu.escape_consumed = false;
  menu_view->g_menu.root_opened_this_frame = false;
  menu_view->g_menu.root_submitted_this_frame = false;
}

bool ui_menu_submit_submenu(const char *parent_popup_id,
                            const char *parent_item_id,
                            const char *submenu_id,
                            const UIMenuItem *items, int item_count) {
  UIMenuState *menu_view=ui_state_menu(ui_overlay_context());

  if (!ui_valid_id(parent_popup_id) || !ui_valid_id(parent_item_id) || !ui_valid_id(submenu_id) || !items) {
    ui_report_error(ui_overlay_context(),UI_ERROR_ARGUMENT,UI_OPERATION_MENU_SUBMIT,submenu_id);
    return false;
  }
  UIMenuSubmission *submission = NULL;
  for (int i = 0; i < menu_view->g_menu.submission_count; ++i) {
    if (!strcmp(menu_view->g_menu.submissions[i].parent_id, parent_popup_id) &&
        !strcmp(menu_view->g_menu.submissions[i].parent_item_id, parent_item_id) &&
        !strcmp(menu_view->g_menu.submissions[i].submenu_id, submenu_id)) {
      submission = &menu_view->g_menu.submissions[i];
      break;
    }
  }
  if (!submission && menu_view->g_menu.submission_count >= MAX_UI_SUBMENU_SUBMISSIONS) {
    ui_report_error(ui_overlay_context(),UI_ERROR_LIMIT,UI_OPERATION_MENU_SUBMIT,submenu_id);
    menu_view->g_menu.overflowed = true;
#ifdef DEBUG
    LOG_WARN("Menu submenu submission capacity exceeded");
#endif
    return false;
  }
  bool added=!submission;
  if (!submission) submission=&menu_view->g_menu.submissions[menu_view->g_menu.submission_count];
  int copied_count=0;
  if(!ui_menu_copy_items(&submission->items,&submission->item_capacity,&copied_count,items,item_count,UI_OPERATION_MENU_SUBMIT,submenu_id)) {
    ui_menu_allocation_failed(0,UI_OPERATION_MENU_SUBMIT,submenu_id); return false;
  }
  UIStoredMenuItem *storage=submission->items; int capacity=submission->item_capacity;
  memset(submission, 0, sizeof(*submission));
  submission->items=storage; submission->item_capacity=capacity; submission->item_count=copied_count;
  if(added) ++menu_view->g_menu.submission_count;
  snprintf(submission->parent_id, sizeof(submission->parent_id), "%s",
           parent_popup_id);
  snprintf(submission->parent_item_id, sizeof(submission->parent_item_id),
           "%s", parent_item_id);
  snprintf(submission->submenu_id, sizeof(submission->submenu_id), "%s",
           submenu_id);
  submission->width = 220.0f;
  return true;
}

static UIMenuSubmission *ui_menu_find_submission(const char *parent_id,
                                                 const char *item_id,
                                                 const char *submenu_id) {
  UIMenuState *menu_view=ui_state_menu(ui_overlay_context());

  for (int i = 0; i < menu_view->g_menu.submission_count; ++i) {
    UIMenuSubmission *submission = &menu_view->g_menu.submissions[i];
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
  UIMenuState *menu_view=ui_state_menu(ui_overlay_context());

  if (!ui_valid_id(root_id)) return NULL;
  if (!root_id || !menu_view->g_menu.result_pending ||
      strcmp(menu_view->g_menu.result_root_id, root_id) != 0)
    return NULL;
  menu_view->g_menu.result_pending = false;
  return menu_view->g_menu.result_item_id;
}

const char *ui_menu_bar(const char *id, const char *label,
                        Font *font, float x, float y, float width, float scale,
                        const UIMenuItem *items, int item_count,
                        const UIButtonStyle *style) {
  UIMenuState *menu_view=ui_state_menu(ui_overlay_context());

  if (!ui_valid_id(id)) return NULL;
  if (!id || !label || !items || item_count <= 0)
    return NULL;
  const char *result = ui_menu_take_action_id(id);
  UIButtonStyle trigger = style ? *style : (UIButtonStyle){0};
  UITheme theme = ui_get_theme();
  float natural_width = ui_paint_measure_text_width(font, label, scale) +
                        theme.popup_padding * 2.0f;
  float trigger_width = natural_width;
  if (trigger.min_width > trigger_width)
    trigger_width = trigger.min_width;
  trigger.min_width = trigger_width;
  trigger.max_width = trigger_width;
  trigger.min_height = theme.menu_height;
  bool root_active = menu_view->g_menu.depth > 0 && !strcmp(menu_view->g_menu.levels[0].id, id);
  if (root_active)
    menu_view->g_menu.root_submitted_this_frame = true;
  if (root_active) {
    trigger.has_custom_color = true;
    trigger.color_normal = theme.selected;
    trigger.color_hover = theme.selected;
    trigger.color_pressed = theme.field;
    trigger.color_border = theme.accent;
  }
  float trigger_height = ui_button_height(font, scale, &trigger);
  bool clicked_open_trigger =
      ui_state_frame(ui_overlay_context())->g_mouse_clicked && menu_view->g_menu.depth > 0 &&
      ui_pointer_hit(x, y, trigger_width, trigger_height);
  if (ui_button_labeled(id, label, font, ANCHOR_TOP_LEFT, 0, scale,
                        x, y, &trigger) || clicked_open_trigger) {
    if (menu_view->g_menu.depth > 0 && !strcmp(menu_view->g_menu.levels[0].id, id))
      ui_menu_close_hierarchy();
    else
      ui_menu_open_root(id, (UIRect){x, y, trigger_width, trigger_height}, width,
                        false, items, item_count);
  } else if (menu_view->g_menu.depth > 0 && !strcmp(menu_view->g_menu.levels[0].id, id)) {
    UIMenuLevel *root = &menu_view->g_menu.levels[0];
    root->anchor = (UIRect){x, y, trigger_width, trigger_height};
    root->width = width;
    if(!ui_menu_copy_items(&root->items,&root->item_capacity,&root->item_count,items,item_count,UI_OPERATION_MENU_ROOT,id))
      ui_menu_allocation_failed(0,UI_OPERATION_MENU_ROOT,id);
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
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

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
                    ui_paint_measure_text_width(font, item->label, text_scale) + 24.0f;
    if (item->shortcut[0])
      natural += ui_paint_measure_text_width(font, item->shortcut,
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
    bool flip_left = right + level->width > frame_view->canvas_width - 10.0f;
    x = flip_left ? level->anchor.x - level->width : right;
    y = level->anchor.y;
    level->opens_left = flip_left;
  }
  if (x + level->width > frame_view->canvas_width - 10.0f)
    x = frame_view->canvas_width - 10.0f - level->width;
  if (x < 10.0f) x = 10.0f;
  if (y + height > frame_view->canvas_height - 10.0f) y = frame_view->canvas_height - 10.0f - height;
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
  UIMenuState *menu_view=ui_state_menu(ui_overlay_context());

  UIMenuLevel *parent = &menu_view->g_menu.levels[parent_depth];
  UIStoredMenuItem *item = &parent->items[item_index];
  UIMenuSubmission *submission = ui_menu_find_submission(
      parent->id, item->id, item->submenu_id);
  if (!submission || item->disabled)
    return;
  int child_depth = parent_depth + 1;
  if (child_depth >= UI_MENU_MAX_DEPTH) {
    ui_report_error(ui_overlay_context(),UI_ERROR_LIMIT,UI_OPERATION_MENU_CHILD,submission->submenu_id);
    menu_view->g_menu.overflowed = true;
#ifdef DEBUG
    LOG_WARN("Menu hierarchy depth exceeded: %d", UI_MENU_MAX_DEPTH);
#endif
    return;
  }
  UIMenuLevel *destination=&menu_view->g_menu.levels[child_depth];
  if(!ui_menu_reserve_items(&destination->items,&destination->item_capacity,submission->item_count)) {
    ui_menu_allocation_failed(child_depth,UI_OPERATION_MENU_CHILD,submission->submenu_id); return;
  }
  if (menu_view->g_menu.depth > child_depth &&
      !strcmp(menu_view->g_menu.levels[child_depth].id, submission->submenu_id) &&
      !strcmp(menu_view->g_menu.levels[child_depth].parent_id, parent->id) &&
      !strcmp(menu_view->g_menu.levels[child_depth].parent_item_id, item->id)) {
    UIMenuLevel *child = &menu_view->g_menu.levels[child_depth];
    child->anchor = ui_menu_item_rect(parent, font, item_index);
    child->width = submission->width;
    if(submission->item_count) memcpy(child->items,submission->items,(size_t)submission->item_count*sizeof(*child->items));
    child->item_count = submission->item_count;
    return;
  }
  char parent_id[UI_ID_CAPACITY], parent_item_id[UI_ID_CAPACITY];
  snprintf(parent_id, sizeof(parent_id), "%s", parent->id);
  snprintf(parent_item_id, sizeof(parent_item_id), "%s", item->id);
  ui_menu_close_from(child_depth);
  UIMenuLevel *child = &menu_view->g_menu.levels[child_depth];
  child->active = true;
  child->depth = child_depth;
  child->width = submission->width;
  child->anchor = ui_menu_item_rect(parent, font, item_index);
  snprintf(child->id, sizeof(child->id), "%s", submission->submenu_id);
  snprintf(child->parent_id, sizeof(child->parent_id), "%s", parent_id);
  snprintf(child->parent_item_id, sizeof(child->parent_item_id), "%s",
           parent_item_id);
  if(submission->item_count) memcpy(child->items,submission->items,(size_t)submission->item_count*sizeof(*child->items));
  child->item_count = submission->item_count;
  menu_view->g_menu.depth = child_depth + 1;
}

void ui_menu_render_overlay(Font *font) {
  UIMenuState *menu_view=ui_state_menu(ui_overlay_context());
  UIFrameState *frame_view=ui_state_frame(ui_overlay_context());

  if (menu_view->g_menu.depth <= 0)
    return;
  if (!menu_view->g_menu.levels[0].context_root &&
      !menu_view->g_menu.root_submitted_this_frame) {
    ui_menu_close_hierarchy();
    return;
  }
  if (ui_any_modal_open()) {
    ui_menu_close_hierarchy();
    return;
  }
  if (ui_capture_consume_escape(ui_overlay_context())) {
    ui_menu_close_from(menu_view->g_menu.depth - 1);
    menu_view->g_menu.escape_consumed = true;
    return;
  }

  UITheme theme = ui_get_theme();
  menu_view->g_menu.hovered_depth = -1;
  menu_view->g_menu.owning_depth = -1;
  for (int depth = 0; depth < menu_view->g_menu.depth; ++depth) {
    UIMenuLevel *level = &menu_view->g_menu.levels[depth];
    ui_menu_place_level(level, font);
    bool inside = ui_pointer_hit(level->popup.x,
                                level->popup.y, level->popup.w,
                                level->popup.h);
    if (inside) {
      frame_view->g_any_hover = true;
      menu_view->g_menu.hovered_depth = depth;
    }
    float popup_border = theme.border_thickness;
    ui_paint_draw_quad(level->popup.x - popup_border,
              level->popup.y - popup_border,
              level->popup.w + popup_border * 2.0f,
              level->popup.h + popup_border * 2.0f,
              theme.border.r, theme.border.g, theme.border.b);
    ui_paint_draw_quad(level->popup.x, level->popup.y, level->popup.w,
              level->popup.h, theme.panel.r, theme.panel.g, theme.panel.b);
    for (int i = 0; i < level->item_count; ++i) {
      UIStoredMenuItem *item = &level->items[i];
      UIRect row = ui_menu_item_rect(level, font, i);
      if (item->kind == UI_MENU_ITEM_SEPARATOR) {
        ui_paint_draw_quad(row.x + theme.popup_padding,
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
          ui_paint_draw_quad(row.x, row.y, row.w, row.h, theme.button_hover.r,
                    theme.button_hover.g, theme.button_hover.b);
        if (!item->disabled && item->kind == UI_MENU_ITEM_SUBMENU)
          ui_menu_open_child(depth, i, font);
        else if (menu_view->g_menu.depth > depth + 1)
          ui_menu_close_from(depth + 1);
      }
      UIColor color = item->disabled ? theme.text_muted : theme.text;
      float text_scale = 14.0f * theme.text_scale;
      float baseline = row.y + (row.h + ui_paint_font_ascent(font, text_scale)) * 0.5f;
      if (item->checked)
        ui_paint_draw_text_colored(font, "x", row.x + theme.popup_padding,
                          baseline, text_scale, theme.accent.r,
                          theme.accent.g, theme.accent.b);
      ui_paint_draw_text_colored(font, item->label,
                        row.x + theme.popup_padding + 18.0f, baseline,
                        text_scale, color.r, color.g, color.b);
      if (item->shortcut[0]) {
        float shortcut_w = ui_paint_measure_text_width(font, item->shortcut,
                                              text_scale * theme.secondary_text_scale);
        ui_paint_draw_text_colored(font, item->shortcut,
                          row.x + row.w - theme.popup_padding - shortcut_w -
                              (item->kind == UI_MENU_ITEM_SUBMENU ? 18.0f : 0.0f),
                          baseline, text_scale * theme.secondary_text_scale,
                          theme.text_muted.r, theme.text_muted.g,
                          theme.text_muted.b);
      }
      if (item->kind == UI_MENU_ITEM_SUBMENU)
        ui_paint_draw_text_colored(font, level->opens_left ? "<" : ">",
                          row.x + row.w - theme.popup_padding - 8.0f,
                          baseline, text_scale, color.r, color.g, color.b);
    }
  }

  int hit_depth = -1, hit_item = -1;
  for (int depth = menu_view->g_menu.depth - 1; depth >= 0; --depth) {
    UIMenuLevel *level = &menu_view->g_menu.levels[depth];
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
  menu_view->g_menu.owning_depth = hit_depth;
  if (!frame_view->g_mouse_clicked)
    return;
  if (hit_depth < 0) {
    if (menu_view->g_menu.root_opened_this_frame) {
      ui_capture_claim_menu(ui_overlay_context());
      return;
    }
    ui_menu_close_hierarchy();
    ui_capture_claim_menu(ui_overlay_context());
    return;
  }
  if (hit_item < 0) {
    ui_capture_claim_menu(ui_overlay_context());
    return;
  }
  UIMenuLevel *level = &menu_view->g_menu.levels[hit_depth];
  UIStoredMenuItem *item = &level->items[hit_item];
  if (item->disabled || item->kind != UI_MENU_ITEM_ACTION) {
    ui_capture_claim_menu(ui_overlay_context());
    return;
  }
  menu_view->g_menu.result_pending = true;
  snprintf(menu_view->g_menu.result_root_id, sizeof(menu_view->g_menu.result_root_id), "%s",
           menu_view->g_menu.levels[0].id);
  snprintf(menu_view->g_menu.result_item_id, sizeof(menu_view->g_menu.result_item_id), "%s",
           item->id);
  ui_menu_close_hierarchy();
  ui_capture_claim_menu(ui_overlay_context());
}

bool ui_menu_get_diagnostics(UIMenuDiagnostics *out) {
  UIMenuState *menu_view=ui_state_menu(ui_overlay_context());

  if (!out) return false;
  memset(out, 0, sizeof(*out));
  out->active_depth = menu_view->g_menu.depth;
  out->hovered_depth = menu_view->g_menu.hovered_depth;
  out->owning_depth = menu_view->g_menu.owning_depth;
  out->escape_consumed = menu_view->g_menu.escape_consumed;
  out->overflowed = menu_view->g_menu.overflowed;
  for (int i = 0; i < menu_view->g_menu.depth && i < UI_MENU_MAX_DEPTH; ++i) {
    out->popup_id[i] = menu_view->g_menu.levels[i].id;
    out->parent_id[i] = menu_view->g_menu.levels[i].parent_id;
    out->parent_item_id[i] = menu_view->g_menu.levels[i].parent_item_id;
    out->anchor[i] = menu_view->g_menu.levels[i].anchor;
    out->popup[i] = menu_view->g_menu.levels[i].popup;
    out->opens_left[i] = menu_view->g_menu.levels[i].opens_left;
  }
  return true;
}
