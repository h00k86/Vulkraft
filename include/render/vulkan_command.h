
#pragma once
/* Private command recording and upload orchestration. */
#include <stdint.h>
#include <stdbool.h>
#include "log/log.h"

typedef struct APP APP;

bool create_command_buffers(APP *app);

bool submit_pending_uploads(APP *app, uint32_t frame_index);

bool record_command_buffer(APP *app,
                            uint32_t frame_index,
                            uint32_t image_index);

void destroy_command_buffers(APP *app);
