#include "../src/platform/backends/glfw.c"
#include <assert.h>
#include <stdio.h>
int main(void) {
  for(int key=PLATFORM_KEY_UNKNOWN+1;key<PLATFORM_KEY_LAST;++key) {
    int native=native_key(key);
    assert(native!=GLFW_KEY_UNKNOWN && translate_key(native)==key);
  }
  assert(translate_key(-100)==PLATFORM_KEY_UNKNOWN);
  assert(native_key(-100)==GLFW_KEY_UNKNOWN);
  assert(translate_key(GLFW_KEY_APOSTROPHE)==PLATFORM_KEY_APOSTROPHE);
  assert(translate_key(GLFW_KEY_SEMICOLON)==PLATFORM_KEY_SEMICOLON);
  assert(translate_mods(GLFW_MOD_SHIFT|GLFW_MOD_CONTROL|GLFW_MOD_ALT|GLFW_MOD_SUPER|GLFW_MOD_CAPS_LOCK|GLFW_MOD_NUM_LOCK)==
    (PLATFORM_MOD_SHIFT|PLATFORM_MOD_CONTROL|PLATFORM_MOD_ALT|PLATFORM_MOD_SUPER|PLATFORM_MOD_CAPS_LOCK|PLATFORM_MOD_NUM_LOCK));
  puts("GLFW adapter: all key round-trips, unknown keys, modifiers: PASS");
}
