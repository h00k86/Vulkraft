#pragma once
#include "render/gfx.h"

typedef struct APP APP;

void frame_dump_record(APP *app, GfxCommandList cmd, uint32_t frame_index,
                       uint32_t image_index);
void frame_dump_collect(APP *app, uint32_t frame_index);
void frame_dump_shutdown(APP *app);
