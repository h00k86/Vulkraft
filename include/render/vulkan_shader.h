

#pragma once
#include "render/gfx_vulkan.h"

#include <vulkan/vulkan.h>
#include <stdlib.h>
#include <stdio.h>
#include "log/log.h"

VkShaderModule load_shader(VkDevice device,const char *path);
