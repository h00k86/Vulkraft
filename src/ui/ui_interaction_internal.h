#pragma once
#include "ui_widget_internal.h"
/* Shared gesture policy implemented once in ui_capture.c. */
bool ui_pointer_press(bool hover,bool enabled);
bool ui_pointer_activate(bool *pressed,bool hover,bool enabled);
