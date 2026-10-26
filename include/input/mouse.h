#pragma once
#include "app/application.h"

typedef struct {
    double x;
    double y;
} Point;

Point get_mouse_coordinate(APP * app);
