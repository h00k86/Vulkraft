#pragma once
#include "platform/window.h"
void input_scroll_register(PlatformWindow *window);
void input_scroll_unregister(PlatformWindow *window);
float input_scroll_take(void);
