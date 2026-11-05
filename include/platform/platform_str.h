#pragma once
/* Compatibilita' per le funzioni POSIX di tokenizzazione e confronto stringhe.
 * Su Windows vengono usate le equivalenti MSVC tramite wrapper inline. */

#include <string.h>

#if defined(_WIN32)
#include <stdlib.h>

static inline char *strtok_r(char *str, const char *delim, char **saveptr) {
  return strtok_s(str, delim, saveptr);
}

static inline int strcasecmp(const char *a, const char *b) {
  return _stricmp(a, b);
}

static inline int strncasecmp(const char *a, const char *b, size_t n) {
  return _strnicmp(a, b, n);
}
#endif
