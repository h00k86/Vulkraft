

#pragma once
#include <stdbool.h>
#include <vulkan/vulkan.h>
#include <stdlib.h>

#include "log/log.h"

typedef struct {

    VkPhysicalDevice device ;
    VkPhysicalDeviceProperties property;
    int deviceScore;
    // 1 = questa GPU ha almeno un'uscita video COLLEGATA, 0 = nessuna,
    // -1 = non determinabile. Vedi gpu_has_connected_display().
    int ownsDisplay;
}PHYSICAL_DEVICE;

// Trova una queue family singola utilizzabile sia dal rendering sia dalla
// presentazione. Il renderer usa intenzionalmente una sola queue.
bool find_graphics_present_queue_family(VkPhysicalDevice device,
                                        VkSurfaceKHR surface,
                                        uint32_t *out_family_index);

VkPhysicalDevice select_device(VkInstance instance, VkSurfaceKHR surface);

/* Selected device name, or an empty string before selection. */
const char *gpu_selected_name(void);
