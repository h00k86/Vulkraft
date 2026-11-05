#pragma once
#include "platform/key_codes.h"
/* Backend-neutral events. Window size uses logical coordinates; framebuffer
 * size uses pixels. Scancodes are opaque native values, never UI key IDs. */
typedef enum { PLATFORM_EVENT_KEY, PLATFORM_EVENT_CHARACTER, PLATFORM_EVENT_FOCUS,
  PLATFORM_EVENT_CURSOR, PLATFORM_EVENT_BUTTON, PLATFORM_EVENT_SCROLL,
  PLATFORM_EVENT_SIZE, PLATFORM_EVENT_FRAMEBUFFER, PLATFORM_EVENT_ICONIFY,
  PLATFORM_EVENT_CLOSE } PlatformEventKind;
typedef struct {
  PlatformEventKind kind;
  int key, scancode, action, mods, button, value, width, height;
  unsigned int codepoint;
  double x,y;
} PlatformEvent;
