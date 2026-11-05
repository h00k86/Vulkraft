#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "platform/window.h"
/* Single-window, main-thread input. Install before polling events; callbacks
 * installed later must chain to the callback returned by the platform. Unregister
 * restores the original callbacks, removing later wrappers as well. */
#include "input/events.h"
void input_keyboard_register(PlatformWindow *window);
void input_keyboard_unregister(PlatformWindow *window);
/* Publish the queued batch once per UI frame. Unconsumed events expire on the
 * next frame, preventing keys typed without a focused editor from replaying. */
void input_keyboard_begin_frame(PlatformWindow *window);
InputKeyEvent *input_keyboard_events(size_t *count);
/* focus_lost remains true for each published frame while the registered window
 * is unfocused. Loss/gain in one batch cancels that whole batch. Registration
 * assumes initial focus until the platform reports otherwise. Prior callbacks
 * still receive all events, including those suppressed for UI input. */
bool input_keyboard_focus_lost(void);
