/* UI a dichiarazione per frame con stato persistente per ID: layout e widget
 * sono ricreati dalle chiamate, mentre focus, popup e drag vivono nei registri
 * interni. Ogni UIContext possiede registri e snapshot; solo la selezione
 * del contesto corrente resta sul thread principale. */
#include "text/utf8.h"
#include "ui/ui.h"

#include "log/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include "ui_context_internal.h"
#include "ui_navigation_internal.h"
#include "ui_renderer_internal.h"
#include "ui_paint_internal.h"
#include "ui_widget_internal.h"
#include "ui_capture_internal.h"
#include "ui_overlay_internal.h"
#include "ui_geometry_internal.h"
#include "ui_error_internal.h"

/* Selezione esplicita sul thread principale, non una istanza UI globale. */
static UIContext *current_ui;
const char *ui_version_string(void) { return UI_VERSION_STRING; }
const char *ui_error_code_name(UIErrorCode code) {
  switch(code) {
  case UI_ERROR_NONE: return "none";
  case UI_ERROR_ARGUMENT: return "invalid argument";
  case UI_ERROR_STATE: return "invalid state";
  case UI_ERROR_LIMIT: return "capacity exceeded";
  case UI_ERROR_MEMORY: return "out of memory";
  }
  return "unknown error";
}
UIErrorInfo ui_context_error(const UIContext *ctx) {
  return ctx?ctx->errors.first:(UIErrorInfo){.code=UI_ERROR_ARGUMENT,.operation=UI_OPERATION_CONTEXT};
}
void ui_context_clear_error(UIContext *ctx) { if(ctx) ctx->errors.first=(UIErrorInfo){0}; }
void ui_report_error(UIContext *ctx,UIErrorCode code,UIErrorOperation operation,const char *id) {
  if(!ctx || code==UI_ERROR_NONE || ctx->errors.first.code!=UI_ERROR_NONE) return;
  ctx->errors.first=(UIErrorInfo){.code=code,.operation=operation};
  if(id) snprintf(ctx->errors.first.widget_id,sizeof(ctx->errors.first.widget_id),"%s",id);
}
static UIContext *ui_ctx(void) {
  assert(current_ui && "Select a UIContext before calling stateful UI APIs");
  return current_ui;
}
#include "ui_text_edit_internal.h"

UINavigationState *ui_state_navigation(UIContext *ctx) { return &ctx->navigation; }
UIRenderState *ui_state_render(UIContext *ctx) { return &ctx->render; }
UIClipboardState *ui_state_clipboard(UIContext *ctx) { return &ctx->clipboard; }
UITooltipState *ui_state_tooltip(UIContext *ctx) { return &ctx->tooltip; }
UIFrameState *ui_state_frame(UIContext *ctx) { return &ctx->frame; }
UIIdsState *ui_state_ids(UIContext *ctx) { return &ctx->ids; }
UIPanelsState *ui_state_panels(UIContext *ctx) { return &ctx->panels; }
UIButtonsState *ui_state_buttons(UIContext *ctx) { return &ctx->buttons; }
UIModalsState *ui_state_modals(UIContext *ctx) { return &ctx->modals; }
UIStyleState *ui_state_style(UIContext *ctx) { return &ctx->style; }
UIMenuState *ui_state_menu(UIContext *ctx) { return &ctx->menu; }
UINumericState *ui_state_numeric(UIContext *ctx) { return &ctx->numeric; }
UISlidersState *ui_state_sliders(UIContext *ctx) { return &ctx->sliders; }
UITextFieldsState *ui_state_text_fields(UIContext *ctx) { return &ctx->text_fields; }
UIDropdownsState *ui_state_dropdowns(UIContext *ctx) { return &ctx->dropdowns; }
UIPickerState *ui_state_picker(UIContext *ctx) { return &ctx->picker; }
UIDebugState *ui_state_debug(UIContext *ctx) { return &ctx->debug; }

const UIFrameState *ui_state_frame_read(const UIContext *ctx) { return &ctx->frame; }
const UIMenuState *ui_state_menu_read(const UIContext *ctx) { return &ctx->menu; }
static void ui_context_defaults(UIContext *ctx) {
  memset(ctx,0,sizeof(*ctx));
  ui_state_frame(ctx)->canvas_width = 1280.0f;
  ui_state_frame(ctx)->canvas_height = 800.0f;
  ui_state_debug(ctx)->debug_enabled = -1;
  ui_state_panels(ctx)->g_section_default_open = true;
  ui_state_menu(ctx)->g_menu = (UIMenuHierarchy){.hovered_depth = -1, .owning_depth = -1};
  ui_state_style(ctx)->g_toast_top = -1.0f;
  ui_state_frame(ctx)->g_scroll_delta = 0.0f;
  ui_state_frame(ctx)->g_ui_frame_serial = 1;
}
UIContext *ui_context_create(void) {
  UIContext *ctx=malloc(sizeof(*ctx));
  if(ctx) ui_context_defaults(ctx);
  return ctx;
}
UIContext *ui_context_get_current(void) { return current_ui; }
void ui_context_clear_preview(UIContext *ctx) {
  if(ui_state_render(ctx)->preview_owned && ui_state_render(ctx)->renderer.texture_manager_preview_clear)
    ui_state_render(ctx)->renderer.texture_manager_preview_clear(ui_state_render(ctx)->renderer_user);
  ui_state_render(ctx)->preview_owned=false;
}
bool ui_context_set_current(UIContext *ctx) {
  if(current_ui==ctx) return true;
  if((current_ui && ui_state_frame(current_ui)->frame_active) || (ctx && ui_state_frame(ctx)->frame_active)) {
    ui_report_error(current_ui?current_ui:ctx,UI_ERROR_STATE,UI_OPERATION_CONTEXT,NULL); return false;
  }
  if(current_ui) ui_context_clear_preview(current_ui);
  current_ui=ctx;
  return true;
}
bool ui_context_reset(UIContext *ctx) {
  if(!ctx || ui_state_frame(ctx)->frame_active) {
    ui_report_error(ctx,UI_ERROR_STATE,UI_OPERATION_CONTEXT,NULL); return false;
  }
  ui_context_clear_preview(ctx);
  free(ui_state_navigation(ctx)->nav_event_owner);
  for(int i=0;i<UI_MENU_MAX_DEPTH;++i) free(ui_state_menu(ctx)->g_menu.levels[i].items);
  for(int i=0;i<MAX_UI_SUBMENU_SUBMISSIONS;++i) free(ui_state_menu(ctx)->g_menu.submissions[i].items);
  free(ui_state_picker(ctx)->file_picker_state);
  for(int i=0;i<ui_state_dropdowns(ctx)->dropdown_state_count;++i)
    free(ui_state_dropdowns(ctx)->dropdown_states[i].popupOptions);
  for(int i=0;i<ui_state_text_fields(ctx)->text_input_state_count;++i) {
    free(ui_state_text_fields(ctx)->text_input_states[i].original_long);
    ui_text_history_clear(&ui_state_text_fields(ctx)->text_input_states[i]);
  }
  UIClipboard clipboard=ui_state_clipboard(ctx)->clipboard;
  void *clipboard_user=ui_state_clipboard(ctx)->clipboard_user;
  UIRenderer renderer=ui_state_render(ctx)->renderer;
  void *user=ui_state_render(ctx)->renderer_user;
  ui_context_defaults(ctx);
  ui_state_clipboard(ctx)->clipboard=clipboard; ui_state_clipboard(ctx)->clipboard_user=clipboard_user;
  ui_state_render(ctx)->renderer=renderer;
  ui_state_render(ctx)->renderer_user=user;
  return true;
}
bool ui_context_destroy(UIContext *ctx) {
  if(!ctx) return true;
  if(!ui_context_reset(ctx)) return false;
  if(current_ui==ctx) current_ui=NULL;
  free(ctx);
  return true;
}
bool ui_context_set_clipboard(UIContext *ctx,const UIClipboard *clipboard,void *user) {
  if(!ctx || ui_state_frame(ctx)->frame_active) {
    ui_report_error(ctx,UI_ERROR_STATE,UI_OPERATION_CONTEXT,NULL); return false;
  }
  ui_state_clipboard(ctx)->clipboard=clipboard?*clipboard:(UIClipboard){0};
  ui_state_clipboard(ctx)->clipboard_user=clipboard?user:NULL;
  return true;
}
bool ui_context_set_renderer(UIContext *ctx, const UIRenderer *renderer, void *user) {
  if (!ctx || ui_state_frame(ctx)->frame_active) {
    ui_report_error(ctx,UI_ERROR_STATE,UI_OPERATION_CONTEXT,NULL); return false;
  }
  if (renderer && !ui_renderer_valid(renderer)) {
    ui_report_error(ctx,UI_ERROR_ARGUMENT,UI_OPERATION_CONTEXT,NULL); return false;
  }
  ui_context_clear_preview(ctx);
  ui_state_render(ctx)->renderer=renderer ? *renderer : (UIRenderer){0};
  ui_state_render(ctx)->renderer_user=renderer ? user : NULL;
  return true;
}

void ui_paint_draw_quad(float x, float y, float w, float h, float r, float g, float b) {
  UIContext *ctx=ui_ctx();
  assert(ui_state_render(ctx)->renderer.draw_quad && "Bind a UI renderer before drawing");
  if (!ui_state_render(ctx)->renderer.draw_quad) { return; }
  ui_state_render(ctx)->renderer.draw_quad(ui_state_render(ctx)->renderer_user, x, y, w, h, r, g, b);
}
void ui_paint_draw_quad_alpha(float x, float y, float w, float h, float r, float g, float b, float a) {
  UIContext *ctx=ui_ctx();
  assert(ui_state_render(ctx)->renderer.draw_quad_alpha && "Bind a UI renderer before drawing");
  if (!ui_state_render(ctx)->renderer.draw_quad_alpha) { return; }
  ui_state_render(ctx)->renderer.draw_quad_alpha(ui_state_render(ctx)->renderer_user, x, y, w, h, r, g, b, a);
}
void ui_paint_draw_text_colored(Font *font, const char *text, float x, float y, float scale, float r, float g, float b) {
  UIContext *ctx=ui_ctx();
  assert(ui_state_render(ctx)->renderer.draw_text_colored && "Bind a UI renderer before drawing");
  if (!ui_state_render(ctx)->renderer.draw_text_colored) { return; }
  ui_state_render(ctx)->renderer.draw_text_colored(ui_state_render(ctx)->renderer_user, font, text, x, y, scale, r, g, b);
}
void ui_paint_draw_textured_quad(float x, float y, float w, float h, int textureSlot, float u0, float v0, float u1, float v1) {
  UIContext *ctx=ui_ctx();
  assert(ui_state_render(ctx)->renderer.draw_textured_quad && "Bind a UI renderer before drawing");
  if (!ui_state_render(ctx)->renderer.draw_textured_quad) { return; }
  ui_state_render(ctx)->renderer.draw_textured_quad(ui_state_render(ctx)->renderer_user, x, y, w, h, textureSlot, u0, v0, u1, v1);
}
void ui_paint_draw_hud_paper_panel(float x, float y, float w, float h, const float fill[4], const float accent[3], bool focused, uint32_t seed) {
  UIContext *ctx=ui_ctx();
  assert(ui_renderer_valid(&ui_state_render(ctx)->renderer) && "Bind a UI renderer before drawing");
  if (!ui_renderer_valid(&ui_state_render(ctx)->renderer)) return;
  ui_renderer_paper(&ui_state_render(ctx)->renderer, ui_state_render(ctx)->renderer_user, x, y, w, h, fill, accent, focused, seed);
}
void ui_paint_draw_hud_trash_swipe(float x, float y, float w, float h, bool focused, uint32_t seed, int textureSlot) {
  UIContext *ctx=ui_ctx();
  assert(ui_renderer_valid(&ui_state_render(ctx)->renderer) && "Bind a UI renderer before drawing");
  if (!ui_renderer_valid(&ui_state_render(ctx)->renderer)) return;
  ui_renderer_swipe(&ui_state_render(ctx)->renderer, ui_state_render(ctx)->renderer_user, x, y, w, h, focused, seed, textureSlot);
}
float ui_paint_font_ascent(Font *font, float scale) {
  UIContext *ctx=ui_ctx();
  assert(ui_state_render(ctx)->renderer.font_ascent && "Bind a UI renderer before layout");
  if (!ui_state_render(ctx)->renderer.font_ascent) return 0.0f;
  return ui_state_render(ctx)->renderer.font_ascent(ui_state_render(ctx)->renderer_user, font, scale);
}
float ui_paint_measure_text_width(Font *font, const char *text, float scale) {
  UIContext *ctx=ui_ctx();
  assert(ui_state_render(ctx)->renderer.measure_text_width && "Bind a UI renderer before drawing");
  if (!ui_state_render(ctx)->renderer.measure_text_width) { return 0.0f; }
  return ui_state_render(ctx)->renderer.measure_text_width(ui_state_render(ctx)->renderer_user, font, text, scale);
}
bool ui_paint_text_clip_push(float x, float y, float w, float h) {
  UIContext *ctx=ui_ctx();
  assert(ui_state_render(ctx)->renderer.text_clip_push && "Bind a UI renderer before drawing");
  if (!ui_state_render(ctx)->renderer.text_clip_push) { return false; }
  return ui_state_render(ctx)->renderer.text_clip_push(ui_state_render(ctx)->renderer_user, x, y, w, h);
}
bool ui_paint_text_clip_pop(void) {
  UIContext *ctx=ui_ctx();
  assert(ui_state_render(ctx)->renderer.text_clip_pop && "Bind a UI renderer before drawing");
  if (!ui_state_render(ctx)->renderer.text_clip_pop) { return false; }
  return ui_state_render(ctx)->renderer.text_clip_pop(ui_state_render(ctx)->renderer_user);
}
static int ui_paint_text_clip_depth(void) {
  UIContext *ctx=ui_ctx();
  assert(ui_state_render(ctx)->renderer.text_clip_depth && "Bind a UI renderer before drawing");
  if (!ui_state_render(ctx)->renderer.text_clip_depth) { return -1; }
  return ui_state_render(ctx)->renderer.text_clip_depth(ui_state_render(ctx)->renderer_user);
}
bool ui_paint_texture_manager_get_size(int slot, int *outW, int *outH) {
  UIContext *ctx=ui_ctx();
  if (!ui_state_render(ctx)->renderer.texture_manager_get_size) { return false; }
  return ui_state_render(ctx)->renderer.texture_manager_get_size(ui_state_render(ctx)->renderer_user, slot, outW, outH);
}
int ui_paint_texture_manager_preview_load(const char *path) {
  UIContext *ctx=ui_ctx();
  if (!ui_state_render(ctx)->renderer.texture_manager_preview_load) { return -1; }
  return ui_state_render(ctx)->renderer.texture_manager_preview_load(ui_state_render(ctx)->renderer_user, path);
}
InputKeyEvent *ui_frame_events(size_t *count) {
  *count=ui_state_frame(ui_ctx())->key_event_count;
  return ui_state_frame(ui_ctx())->key_events;
}

/* Riorganizzazione UI 2026-09-17: 128 bottoni erano meno delle righe di una
 * gerarchia media (ogni riga e' un bottone): oltre il limite i bottoni
 * dell'inspector SPARIVANO in silenzio (find_or_create_state -> NULL).
 * Il pool e' una cache limitata ma capiente; la ricerca resta lineare nel
 * numero di elementi, con hash per evitare confronti di stringhe inutili. */

static void ui_keyboard_set_frame(const InputKeyEvent *events, size_t count, bool focus_lost) {
  assert(count<=INPUT_KEYBOARD_CAPACITY);
  if (count) memcpy(ui_state_frame(ui_ctx())->key_events,events,count*sizeof(*events));
  ui_state_frame(ui_ctx())->key_event_count=count;
  if (focus_lost) ui_nav_clear_editor_focus(ui_ctx());
}
static bool ui_key_pending(int key) {
  size_t count; InputKeyEvent *events=ui_frame_events(&count);
  for (size_t i=0;i<count;++i)
    if (!events[i].consumed && events[i].kind==INPUT_KEY &&
        events[i].key==key && events[i].action==PLATFORM_PRESS) return true;
  return false;
}

bool ui_valid_id(const char *id) {
  return id && id[0] && strlen(id) < UI_ID_CAPACITY;
}

bool ui_push_id(const char *scope) {
  if (!ui_valid_id(scope) || ui_state_ids(ui_ctx())->g_id_stack_depth >= MAX_UI_ID_STACK) {
    ui_report_error(ui_ctx(),ui_valid_id(scope)?UI_ERROR_LIMIT:UI_ERROR_ARGUMENT,UI_OPERATION_ID_STACK,scope);
#ifdef DEBUG
    LOG_WARN("UI ID stack overflow or empty scope");
#endif
    return false;
  }
  snprintf(ui_state_ids(ui_ctx())->g_id_stack[ui_state_ids(ui_ctx())->g_id_stack_depth], sizeof(ui_state_ids(ui_ctx())->g_id_stack[0]), "%s", scope);
  ui_state_ids(ui_ctx())->g_id_stack_depth++;
  return true;
}

void ui_pop_id(void) {
  if (ui_state_ids(ui_ctx())->g_id_stack_depth <= 0) {
#ifdef DEBUG
    LOG_WARN("UI ID stack underflow");
#endif
    return;
  }
  ui_state_ids(ui_ctx())->g_id_stack_depth--;
}

const char *ui_scoped_id(const char *local_id) {
  char *out = ui_state_ids(ui_ctx())->g_scoped_id_buffers[ui_state_ids(ui_ctx())->g_scoped_id_buffer_index++ % UI_SCOPED_ID_BUFFER_COUNT];
  const size_t capacity = sizeof(ui_state_ids(ui_ctx())->g_scoped_id_buffers[0]);
  if (!ui_valid_id(local_id ? local_id : "widget")) return "";
  size_t used = 0;
  out[0] = '\0';
  for (int i = 0; i < ui_state_ids(ui_ctx())->g_id_stack_depth; ++i) {
    size_t remaining = capacity - used;
    int wrote = snprintf(out + used, remaining, "%s/", ui_state_ids(ui_ctx())->g_id_stack[i]);
    if (wrote < 0) {
      return "";
    }
    if ((size_t)wrote >= remaining)
      return ""; /* Reject instead of aliasing two distinct long IDs. */
    used += (size_t)wrote;
  }
  int written = snprintf(out + used, capacity - used, "%s", local_id ? local_id : "widget");
  return written >= 0 && (size_t)written < capacity - used ? out : "";
}

unsigned ui_hash_id(const char *id) {
  unsigned h = 2166136261u; /* FNV-1a */
  for (const unsigned char *p = (const unsigned char *)id; *p; ++p)
    h = (h ^ *p) * 16777619u;
  return h;
}

/* Input ownership lives in ui_capture.c; visual focus remains here. */
static bool point_in_rect(float px, float py, float x, float y, float w,
                          float h);
void ui_nav_ring(const char *id,UIRect r) {
  if(!ui_state_navigation(ui_ctx())->nav_keyboard || !ui_nav_focused(ui_ctx(), id) ||
     ui_nav_find(ui_ctx(), ui_state_navigation(ui_ctx())->nav_current,ui_state_navigation(ui_ctx())->nav_count,id)<0) return;
  UIColor a=ui_get_theme().accent;
  const float t=2;
  ui_paint_draw_quad(r.x,r.y,r.w,t,a.r,a.g,a.b);
  ui_paint_draw_quad(r.x,r.y+r.h-t,r.w,t,a.r,a.g,a.b);
  ui_paint_draw_quad(r.x,r.y,t,r.h,a.r,a.g,a.b);
  ui_paint_draw_quad(r.x+r.w-t,r.y,t,r.h,a.r,a.g,a.b);
}
const char *ui_keyboard_focus_id(void) { return ui_state_navigation(ui_ctx())->nav_focus; }

#include "ui_interaction_internal.h"

void ui_set_theme(UITheme theme) {
  ui_state_style(ui_ctx())->g_theme = theme;
  ui_state_style(ui_ctx())->g_theme_initialized = true;
}

UITheme ui_get_theme(void) {
  if (!ui_state_style(ui_ctx())->g_theme_initialized) {
    ui_state_style(ui_ctx())->g_theme = ui_theme_dark();
    ui_state_style(ui_ctx())->g_theme_initialized = true;
  }
  return ui_state_style(ui_ctx())->g_theme;
}

void ui_set_feedback(UIFeedbackFn fn, void *user) {
  ui_state_style(ui_ctx())->g_feedback = fn;
  ui_state_style(ui_ctx())->g_feedbackUser = user;
}
/* dal frame appena finito: su quale widget stava il puntatore (hash del suo
 * id, 0 = nessuno con stato) e se era su qualcosa di cliccabile */
static void ui_feedback_frame(bool mouse_down) {

  unsigned hover = 0;
  for (int i = 0; i < ui_state_buttons(ui_ctx())->ui_state_count; ++i)
    if (ui_state_buttons(ui_ctx())->ui_states[i].is_hover) {
      hover = ui_state_buttons(ui_ctx())->ui_states[i].id_hash;
      break;
    }
  const bool enter = (hover && hover != ui_state_style(ui_ctx())->feedback_last_hover) || (ui_state_frame(ui_ctx())->g_any_hover && !ui_state_style(ui_ctx())->feedback_last_any);
  if (ui_state_style(ui_ctx())->g_feedback && enter)
    ui_state_style(ui_ctx())->g_feedback("focus", ui_state_style(ui_ctx())->g_feedbackUser);
  /* la pressione si giudica col frame IN CUI e' avvenuta (lo hover di quel
   * frame e' gia' sulla posizione premuta): qui, al frame dopo */

  if (ui_state_style(ui_ctx())->g_feedback && ui_state_style(ui_ctx())->feedback_pressed_last && ui_state_frame(ui_ctx())->g_any_hover)
    ui_state_style(ui_ctx())->g_feedback("confirm", ui_state_style(ui_ctx())->g_feedbackUser);
  ui_state_style(ui_ctx())->feedback_pressed_last = mouse_down && !ui_state_style(ui_ctx())->feedback_last_down;
  ui_state_style(ui_ctx())->feedback_last_hover = hover;
  ui_state_style(ui_ctx())->feedback_last_any = ui_state_frame(ui_ctx())->g_any_hover;
  ui_state_style(ui_ctx())->feedback_last_down = mouse_down;
}

bool ui_begin_frame_input(const UIFrameInput *input) {
  return ui_begin_frame_input_status(input)==UI_ERROR_NONE;
}
UIErrorCode ui_begin_frame_input_status(const UIFrameInput *input) {
  if(!current_ui || ui_state_frame(current_ui)->frame_active) return UI_ERROR_STATE;
  if(!input) return UI_ERROR_ARGUMENT;
  if(input->event_count>INPUT_KEYBOARD_CAPACITY) return UI_ERROR_LIMIT;
  if (
      (input->event_count && !input->events) ||
      !isfinite(input->width) || input->width <= 0 ||
      !isfinite(input->height) || input->height <= 0 ||
      !isfinite(input->mouse_x) || !isfinite(input->mouse_y) ||
      !isfinite(input->delta_time) || input->delta_time < 0 ||
      !isfinite(input->scroll_delta)) return UI_ERROR_ARGUMENT;
  if(!ui_nav_reserve_events(current_ui,input->focus_lost?0:input->event_count)) return UI_ERROR_MEMORY;
  const float mouse_x=input->mouse_x, mouse_y=input->mouse_y;
  const float delta_time=input->delta_time;
  const bool mouse_down=input->mouse_down && !input->focus_lost;
  if (input->focus_lost) {
    /* Window deactivation cancels gestures; it is not a button release. */
    ui_release_input_capture();
    ui_state_frame(ui_ctx())->g_mouse_down_prev=false;
    ui_state_style(ui_ctx())->feedback_pressed_last=false;
    ui_state_style(ui_ctx())->feedback_last_down=false;
  }
  ui_state_frame(ui_ctx())->frame_active=true;
  ui_state_frame(ui_ctx())->canvas_width=input->width;
  ui_state_frame(ui_ctx())->canvas_height=input->height;
  ui_state_frame(ui_ctx())->debug_keys_down=input->debug_keys_down && !input->focus_lost;
  ui_set_scroll_delta(input->focus_lost?0:input->scroll_delta);
  ui_keyboard_set_frame(input->events,input->focus_lost?0:input->event_count,input->focus_lost);
  /* Pointer activity hides keyboard rings without discarding focus. Do this
   * before routing Tab so subsequent keyboard navigation can show them again. */
  if(mouse_x!=ui_state_frame(ui_ctx())->g_mouse_x || mouse_y!=ui_state_frame(ui_ctx())->g_mouse_y ||
     mouse_down!=ui_state_frame(ui_ctx())->g_mouse_down_prev || input->scroll_delta!=0)
    ui_state_navigation(ui_ctx())->nav_keyboard=false;
  ui_nav_begin(ui_ctx(), input->focus_lost, ui_menu_hierarchy_is_open());
  ui_feedback_frame(mouse_down);
  ++ui_state_frame(ui_ctx())->g_ui_frame_serial;
  if (ui_state_frame(ui_ctx())->g_ui_frame_serial == 0) {
    ui_state_frame(ui_ctx())->g_ui_frame_serial = 1;
    for (int i = 0; i < ui_state_buttons(ui_ctx())->ui_state_count; ++i)
      ui_state_buttons(ui_ctx())->ui_states[i].last_seen_frame = 0;
    for (int i = 0; i < ui_state_panels(ui_ctx())->g_section_state_count; ++i)
      ui_state_panels(ui_ctx())->g_section_states[i].last_seen_frame = 0;
  }
  ui_state_ids(ui_ctx())->g_scoped_id_buffer_index = 0;
  ui_state_frame(ui_ctx())->g_scroll_consumed = false;
  ui_state_modals(ui_ctx())->g_modal_rect_valid = ui_state_modals(ui_ctx())->g_modal_rect_seen_this_frame; /* modale del frame precedente */
  ui_state_modals(ui_ctx())->g_modal_rect_seen_this_frame = false;
  ui_state_modals(ui_ctx())->g_modal_open_count_prev = 0;
  for (int i = 0; i < ui_state_modals(ui_ctx())->g_modal_state_count; ++i)
    if (ui_state_modals(ui_ctx())->g_modal_states[i].wasOpen)
      ui_state_modals(ui_ctx())->g_modal_open_count_prev++;
  ui_state_modals(ui_ctx())->g_modal_scope_depth = 0;
  ui_state_frame(ui_ctx())->g_mouse_x = mouse_x;
  ui_state_frame(ui_ctx())->g_mouse_y = mouse_y;
  ui_state_frame(ui_ctx())->g_mouse_clicked = mouse_down && !ui_state_frame(ui_ctx())->g_mouse_down_prev;
  ui_state_frame(ui_ctx())->g_mouse_released = !mouse_down && ui_state_frame(ui_ctx())->g_mouse_down_prev;
  ui_state_frame(ui_ctx())->g_mouse_down = mouse_down;
  ui_state_frame(ui_ctx())->g_mouse_down_prev = mouse_down;
  ui_state_frame(ui_ctx())->g_escape_pressed = ui_key_pending(PLATFORM_KEY_ESCAPE);
  ui_state_frame(ui_ctx())->g_escape_consumed = false;
  if (!ui_state_frame(ui_ctx())->g_mouse_down) {
    for (int i = 0; i < ui_state_panels(ui_ctx())->g_scroll_state_count; ++i)
      ui_state_panels(ui_ctx())->g_scroll_states[i].dragging = false;
    /* Un widget SPARITO mentre era premuto/in drag (cambio pannello o
       modalita' durante la pressione) non viene piu' ridisegnato e quindi
       non azzererebbe mai il proprio stato: ui_has_pointer_capture()
       restava vero per sempre. Si azzerano SOLO i widget non visti nel
       frame precedente: quelli ancora vivi consumano il rilascio da soli
       (e' cosi' che nasce il click). */
    for (int i = 0; i < ui_state_buttons(ui_ctx())->ui_state_count; ++i)
      if (ui_state_buttons(ui_ctx())->ui_states[i].is_pressed && ui_state_buttons(ui_ctx())->ui_states[i].last_seen_frame + 1 < ui_state_frame(ui_ctx())->g_ui_frame_serial)
        ui_state_buttons(ui_ctx())->ui_states[i].is_pressed = false;
    ui_release_stale_slider_drags();
  }
  ui_state_frame(ui_ctx())->g_delta_time = delta_time;
  for (int i = 0; i < ui_state_style(ui_ctx())->g_toast_count;) {
    ui_state_style(ui_ctx())->g_toasts[i].remaining -= delta_time;
    if (ui_state_style(ui_ctx())->g_toasts[i].remaining <= 0.0f) {
      memmove(&ui_state_style(ui_ctx())->g_toasts[i], &ui_state_style(ui_ctx())->g_toasts[i + 1],
              sizeof(ui_state_style(ui_ctx())->g_toasts[0]) * (size_t)(ui_state_style(ui_ctx())->g_toast_count - i - 1));
      --ui_state_style(ui_ctx())->g_toast_count;
    } else {
      ++i;
    }
  }
  ui_state_frame(ui_ctx())->g_any_hover = false;
  for (int i = 0; i < ui_state_buttons(ui_ctx())->ui_state_count; ++i)
    ui_state_buttons(ui_ctx())->ui_states[i].is_hover = false;
  for (int i = 0; i < ui_state_panels(ui_ctx())->g_scroll_state_count; ++i) {
    ui_state_panels(ui_ctx())->g_scroll_states[i].wheel_consumed = false;
    ui_state_panels(ui_ctx())->g_scroll_states[i].thumb_hovered = false;
  }
  ui_reset_dropdown_popup_pass();
  ui_input_regions_begin();

  ui_menu_begin_frame();
  ui_capture_begin_frame(ui_ctx());
  ui_modal_input_begin_frame();

  return UI_ERROR_NONE;
}

void ui_notify(UIToastKind kind, const char *message) {
  if (!message || !message[0])
    return;
  for (int i = 0; i < ui_state_style(ui_ctx())->g_toast_count; ++i) {
    if (!strcmp(ui_state_style(ui_ctx())->g_toasts[i].message, message)) {
      ui_state_style(ui_ctx())->g_toasts[i].kind = kind;
      ui_state_style(ui_ctx())->g_toasts[i].remaining = 2.8f;
      return;
    }
  }
  if (ui_state_style(ui_ctx())->g_toast_count == MAX_UI_TOASTS) {
    memmove(&ui_state_style(ui_ctx())->g_toasts[0], &ui_state_style(ui_ctx())->g_toasts[1],
            sizeof(ui_state_style(ui_ctx())->g_toasts[0]) * (MAX_UI_TOASTS - 1));
    --ui_state_style(ui_ctx())->g_toast_count;
  }
  UIToast *toast = &ui_state_style(ui_ctx())->g_toasts[ui_state_style(ui_ctx())->g_toast_count++];
  snprintf(toast->message, sizeof(toast->message), "%s", message);
  toast->kind = kind;
  toast->remaining = 2.8f;
}

void ui_set_notification_top(float y) { ui_state_style(ui_ctx())->g_toast_top = y; }

void ui_draw_notifications(Font *font) {
  if (!font || ui_state_style(ui_ctx())->g_toast_count == 0)
    return;
  UITheme theme = ui_get_theme();
  float width = 300.0f;
  float height = theme.menu_height;
  float x = ui_state_frame(ui_ctx())->canvas_width - width - theme.panel_padding;
  /* Sotto il chrome che il chiamante dichiara (editor: topbar + subbar),
     mai sopra i tab del workspace. */
  float y = ui_state_style(ui_ctx())->g_toast_top >= 0.0f ? ui_state_style(ui_ctx())->g_toast_top : theme.menu_height + theme.panel_padding;
  for (int i = ui_state_style(ui_ctx())->g_toast_count - 1; i >= 0; --i) {
    UIToast *toast = &ui_state_style(ui_ctx())->g_toasts[i];
    UIColor semantic = theme.accent;
    if (toast->kind == UI_TOAST_SUCCESS) semantic = theme.success;
    else if (toast->kind == UI_TOAST_WARNING) semantic = theme.warning;
    else if (toast->kind == UI_TOAST_ERROR) semantic = theme.error;
    ui_paint_draw_quad(x - theme.border_thickness, y - theme.border_thickness,
              width + theme.border_thickness * 2.0f,
              height + theme.border_thickness * 2.0f, theme.border.r,
              theme.border.g, theme.border.b);
    ui_paint_draw_quad(x, y, width, height, theme.panel_header.r,
              theme.panel_header.g, theme.panel_header.b);
    ui_paint_draw_quad(x, y, 4.0f, height, semantic.r, semantic.g, semantic.b);
    char display[96];
    ui_ellipsize_right(font, toast->message, 13.0f,
                       width - theme.panel_padding * 2.0f, display,
                       sizeof(display));
    ui_paint_draw_text_colored(font, display, x + theme.panel_padding,
                      y + (height + ui_paint_font_ascent(font, 13.0f)) * 0.5f, 13.0f,
                      theme.text.r, theme.text.g, theme.text.b);
    y += height + theme.row_spacing;
  }
}

Vec2 ui_mouse_position(void) { return (Vec2){ui_state_frame(ui_ctx())->g_mouse_x, ui_state_frame(ui_ctx())->g_mouse_y}; }
unsigned long long ui_frame_serial_hint(void) { return ui_state_frame(ui_ctx())->g_ui_frame_serial; }
bool ui_mouse_down(void) { return ui_state_frame(ui_ctx())->g_mouse_down; }

void ui_end_frame(void) {
  ui_tooltip_draw_pending();
  ui_modal_input_end_frame();
  ui_nav_end(ui_ctx());
#ifdef DEBUG
  if (ui_state_ids(ui_ctx())->g_id_stack_depth != 0)
    LOG_WARN("UI ID stack imbalance: %d scope(s) left open", ui_state_ids(ui_ctx())->g_id_stack_depth);
#endif
  ui_state_ids(ui_ctx())->g_id_stack_depth = 0;
  ui_state_frame(ui_ctx())->frame_active=false;
}

void ui_input_regions_begin(void) { ui_state_frame(ui_ctx())->g_input_region_count = 0; }

bool ui_input_region_register(const char *id, UIRect rect) {
  if (!ui_valid_id(id)) return false;
  if (!id || !id[0] || !isfinite(rect.x) || !isfinite(rect.y) ||
      !isfinite(rect.w) || !isfinite(rect.h) || rect.w <= 0.0f ||
      rect.h <= 0.0f ||
      ui_state_frame(ui_ctx())->g_input_region_count >= MAX_UI_INPUT_REGIONS)
    return false;

  UIInputRegion *region = &ui_state_frame(ui_ctx())->g_input_regions[ui_state_frame(ui_ctx())->g_input_region_count++];
  strncpy(region->id, id, sizeof(region->id) - 1);
  region->id[sizeof(region->id) - 1] = '\0';
  region->rect = rect;
  return true;
}

bool ui_input_region_contains(float x, float y) {
  /* Last registration wins. This keeps an eventual overlay region above its
   * parent without giving callers another priority mechanism. */
  for (int i = ui_state_frame(ui_ctx())->g_input_region_count - 1; i >= 0; --i) {
    UIRect r = ui_state_frame(ui_ctx())->g_input_regions[i].rect;
    if (point_in_rect(x, y, r.x, r.y, r.w, r.h))
      return true;
  }
  return false;
}

bool ui_clip_push(UIRect rect) {
  if (!isfinite(rect.x) || !isfinite(rect.y) || !isfinite(rect.w) ||
      !isfinite(rect.h) || rect.w <= 0.0f || rect.h <= 0.0f)
    return false;
  if(ui_state_frame(ui_ctx())->layout_input_clip_count>=128) return false;
  if(!ui_paint_text_clip_push(rect.x, rect.y, rect.w, rect.h)) return false;
  ui_state_frame(ui_ctx())->layout_input_clips[ui_state_frame(ui_ctx())->layout_input_clip_count++]=rect;
  return true;
}

bool ui_clip_pop(void) {
  if(!ui_paint_text_clip_pop()) return false;
  if(ui_state_frame(ui_ctx())->layout_input_clip_count) --ui_state_frame(ui_ctx())->layout_input_clip_count;
  return true;
}

int ui_clip_depth(void) { return ui_paint_text_clip_depth(); }

int ui_input_region_count(void) { return ui_state_frame(ui_ctx())->g_input_region_count; }

const UIRect *ui_input_region_at(int index, const char **out_id) {
  if (index < 0 || index >= ui_state_frame(ui_ctx())->g_input_region_count)
    return NULL;
  if (out_id)
    *out_id = ui_state_frame(ui_ctx())->g_input_regions[index].id;
  return &ui_state_frame(ui_ctx())->g_input_regions[index].rect;
}

bool ui_any_hover(void) { return ui_state_frame(ui_ctx())->g_any_hover; }

/* Implemented after the overlay-state registries below. */
bool ui_has_input_capture(void);

static bool point_in_rect(float px, float py, float x, float y, float w,
                          float h) {
  return px >= x && px <= x + w && py >= y && py <= y + h;
}

void ui_set_sentence_case(bool enabled) {
  ui_state_style(ui_ctx())->sentence_case=enabled;
}

static bool ui_label_word_byte(unsigned char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '_';
}

bool ui_set_sentence_case_exceptions(const char *const *words, size_t count) {
  if (count && !words) return false;
  for (size_t i = 0; i < count; ++i) {
    if (!words[i] || !words[i][0]) return false;
    for (const unsigned char *p = (const unsigned char *)words[i]; *p; ++p)
      if (!ui_label_word_byte(*p)) return false;
  }
  UIStyleState *style = ui_state_style(ui_ctx());
  style->sentence_case_exceptions = count ? words : NULL;
  style->sentence_case_exception_count = count;
  return true;
}

static bool ui_label_exception(const UIStyleState *style, const char *word,
                               size_t bytes) {
  for (size_t i = 0; i < style->sentence_case_exception_count; ++i) {
    const char *entry = style->sentence_case_exceptions[i];
    if (strlen(entry) == bytes && !memcmp(entry, word, bytes)) return true;
  }
  return false;
}

void ui_humanize_label(const char *in, char *out, size_t outSize) {
  if (!out || !outSize) return;
  if (!in) { out[0]=0; return; }
  UIContext *ctx=ui_context_get_current();
  if (!ctx || !ui_state_style(ctx)->sentence_case) {
    utf8_copy(out,outSize,in);
    return;
  }
  /* Explicit ASCII sentence case for all-caps labels. Mixed-case labels and
   * parenthesized units are preserved. This is not Unicode case folding. */
  bool hasLower = false;
  int depth = 0;
  for (const char *p = in; *p; p++) {
    if (*p == '(') depth++;
    else if (*p == ')' && depth > 0) depth--;
    else if (depth == 0 && *p >= 'a' && *p <= 'z') {
      hasLower = true;
      break;
    }
  }
  if (hasLower) {
    utf8_copy(out,outSize,in);
    return;
  }
  size_t bytes=strlen(in);
  size_t limit=bytes<outSize?bytes:utf8_floor(in,bytes,outSize-1);
  size_t i = 0;
  bool firstLetter = true;
  depth = 0;
  const char *preserve_end = in;
  const UIStyleState *style = ui_state_style(ctx);
  for (const char *p = in; *p && i < limit; p++, i++) {
    char c = *p;
    if (c == '(') depth++;
    if (depth > 0) {
      out[i] = c; // parentesi: intatto
      if (c == ')') depth--;
      continue;
    }
    /* Scan full input tokens, even when the output ends inside a word.
     * Non-ASCII neighbors prevent matches embedded
     * in UTF-8 words such as ÉFX. */
    if (ui_label_word_byte((unsigned char)c) &&
        (p == in || (!ui_label_word_byte((unsigned char)p[-1]) &&
                     (unsigned char)p[-1] < 128))) {
      const char *end = p;
      while (ui_label_word_byte((unsigned char)*end)) ++end;
      if ((unsigned char)*end < 128 &&
          ui_label_exception(style, p, (size_t)(end - p))) preserve_end = end;
    }
    if (c >= 'A' && c <= 'Z') {
      out[i] = firstLetter || p < preserve_end ? c : (char)(c - 'A' + 'a');
      firstLetter = false;
    } else {
      out[i] = c;
    }
  }
  out[i] = '\0';
}

void ui_ellipsize_left(Font *font, const char *text, float scale,
                              float max_width, char *out, size_t out_size) {
  if (!out || !out_size) return;
  utf8_copy(out,out_size,text ? text : "");
  if (!font || max_width<=0 || ui_paint_measure_text_width(font,out,scale)<=max_width) return;
  float dots=ui_paint_measure_text_width(font,"...",scale);
  if (out_size<4 || dots>max_width) { out[0]=0; return; }
  size_t n=strlen(out), start=0;
  while (start<n && (n-start+3>=out_size ||
         ui_paint_measure_text_width(font,out+start,scale)+dots>max_width))
    start=utf8_next(out,n,start);
  memmove(out+3,out+start,n-start+1);
  memcpy(out,"...",3);
}

void ui_ellipsize_right(Font *font, const char *text, float scale,
                               float max_width, char *out, size_t out_size) {
  if (!out || !out_size) return;
  utf8_copy(out,out_size,text ? text : "");
  if (!font || max_width<=0 || ui_paint_measure_text_width(font,out,scale)<=max_width) return;
  float dots=ui_paint_measure_text_width(font,"...",scale);
  if (out_size<4 || dots>max_width) { out[0]=0; return; }
  size_t n=strlen(out);
  while (n && (n+3>=out_size || ui_paint_measure_text_width(font,out,scale)+dots>max_width)) {
    n=utf8_prev(out,n); out[n]=0;
  }
  memcpy(out+n,"...",4);
}
