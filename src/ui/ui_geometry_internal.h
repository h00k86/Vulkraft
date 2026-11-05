#pragma once
#include "ui/ui.h"
#include <math.h>

/* Shared geometry checks; no context or renderer dependency. */
static inline bool ui_layout_rect_valid(UIRect r) {
  return isfinite(r.x)&&isfinite(r.y)&&isfinite(r.w)&&isfinite(r.h)&&
    r.w>0&&r.h>0&&isfinite(r.x+r.w)&&isfinite(r.y+r.h);
}
