#pragma once
#include <SDL.h>
#include "platform/window.h"
/* Private handle bridge. Never expose SDL types through platform/window.h. */
SDL_Window *platform_sdl2_native(PlatformWindow *window);
SDL_Window *platform_sdl2_active_native(void);
/* Storage belongs to the active window and is released with it. */
const char **platform_sdl2_extension_storage(unsigned count);
