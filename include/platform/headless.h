#pragma once
#include "platform/window.h"
/* Deterministic test backend only. Events are delivered on poll/wait in FIFO
 * order. Wait never blocks; time advances only through headless_advance. */
bool platform_headless_post(PlatformWindow *window,PlatformEvent event);
void platform_headless_advance(double seconds);
