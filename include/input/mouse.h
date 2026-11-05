#pragma once
#include "app/application.h"

typedef struct {
    double x;
    double y;
} Point;

Point get_mouse_coordinate(APP * app);

/* Single-window APP collector; owns the event observer slot until unregister.
 * One queued transition per UI frame, preserving click order and position.
 * Overflow or focus loss cancels gestures rather than replaying a partial batch. */
#define INPUT_MOUSE_CAPACITY 128
void input_mouse_register(PlatformWindow *window);
void input_mouse_unregister(PlatformWindow *window);
bool input_mouse_frame(APP *app, Point *position, bool *cancelled);
