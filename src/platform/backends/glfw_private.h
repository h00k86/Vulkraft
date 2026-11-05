#pragma once
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "platform/window.h"
/* Private bridge for the Vulkan backend and native integration tests only. */
GLFWwindow *platform_glfw_native(PlatformWindow *window);
