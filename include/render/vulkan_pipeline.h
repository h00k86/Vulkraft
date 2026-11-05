

#pragma once
#include <stdbool.h>

#include <stdlib.h>
#include <vulkan/vulkan.h>

typedef struct APP APP;
#include "render/vulkan_shader.h"
#include "log/log.h"


bool create_pipeline(APP *app);
void destroy_pipeline(APP *app);
