#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "platform/key_codes.h"
/* Dati portabili del batch, senza finestra o collector. */
#define INPUT_KEYBOARD_CAPACITY 1024
typedef enum { INPUT_KEY, INPUT_CHARACTER } InputEventKind;
typedef struct {
  InputEventKind kind;
  int key, action, mods;
  uint32_t codepoint;
  bool consumed;
} InputKeyEvent;
