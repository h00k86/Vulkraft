

#pragma once

#include <vulkan/vulkan_core.h>

#include <stdint.h>
#include <stdlib.h>

VkInstance create_instance();

void destroy_instance(VkInstance instance);
