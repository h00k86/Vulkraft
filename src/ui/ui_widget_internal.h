#pragma once
#include "ui/ui.h"

/* Private widget services, evaluated against the current context. Hit testing
 * includes modal and input clips; drag uses the same press/cancel policy as
 * core widgets. hover_click reports an unclaimed press edge for field-like
 * property widgets and records hover feedback. No state pointers escape. */
bool ui_valid_id(const char *id);
bool ui_pointer_hit(float x,float y,float w,float h);
bool ui_pointer_drag(bool *dragging,bool hover,bool enabled);
bool ui_input_hover_click(UIRect rect,bool *clicked);
bool ui_scroll_wheel_blocked(void);
/* Shared text/password field; copy/cut filtering stays in the field core. */
bool ui_text_input_mode(const char *id,Font *font,TextAnchor anchor,
                        float margin,float scale,float x,float y,float width,
                        char *buffer,int capacity,const UIButtonStyle *style,
                        bool password,bool reveal);

/* Numeric field core preserves exact integer parsing for property rows. */
bool ui_number_field(const char *id,Font *font,float x,float y,
                      float width,float height,float scale,float *value,
                      float min,float max,int *integer,int int_min,int int_max);

unsigned ui_hash_id(const char *id);
void ui_nav_ring(const char *id,UIRect rect);

void ui_release_stale_slider_drags(void);
void ui_tooltip_draw_pending(void);
InputKeyEvent *ui_frame_events(size_t *count);
static inline bool ui_layout_pointer_hit(UIRect r) {
  return ui_pointer_hit(r.x,r.y,r.w,r.h);
}

/* Presentation shares slider interaction and exact float editing.
 * idle_decimals < 0 uses compact round-trip; 0..9 sets idle-only decimals. */
bool ui_slider_format(const char *id,Font *font,TextAnchor anchor,
                      float margin,float scale,float x,float y,float width,
                      float *value,float min,float max,const char *label,
                      bool *out_drag_started,int idle_decimals);
