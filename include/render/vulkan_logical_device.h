
#pragma once
#include "render/gfx_vulkan.h" /* PORTING E14: tipi del backend */
#include <stdlib.h>
typedef struct APP APP;



VkDevice create_logical_device(APP * app);


void destroy_logical_device(APP * app);
