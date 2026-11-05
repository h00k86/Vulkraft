#pragma once
#include <vulkan/vulkan.h>
#include "platform/window.h"
/* Renderer/platform bridge. Implemented by the selected platform backend. */
/* L'array restituito e preso in prestito dal backend: non liberarlo.
 * NULL/count=0 indica che il backend non offre presentazione Vulkan. */
const char **platform_vulkan_extensions(uint32_t *count);
/* In caso di successo la superficie passa al renderer, che deve distruggerla
 * prima dell'istanza. La finestra deve restare viva durante il suo utilizzo. */
VkResult platform_vulkan_surface(VkInstance instance,PlatformWindow *window,VkSurfaceKHR *surface);
