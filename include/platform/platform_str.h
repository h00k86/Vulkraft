#pragma once
/* PORTING (VALUTAZIONI.md E13, 2026-09-25) -- le due funzioni di stringa
 * che il motore usa e che NON sono standard C.
 *
 * strtok_r (22 usi: parser di livelli, behavior, .obj, camera, danno) e
 * strcasecmp (3 usi) sono POSIX. MSVC ha gli stessi identici comportamenti
 * con altri nomi. Non c'e' nulla da riscrivere nei chiamanti: basta
 * includere questo header al posto di darlo per scontato.
 *
 * Volutamente NON si ridefiniscono i nomi POSIX con delle macro: una macro
 * su un nome di funzione della libreria standard rompe qualunque header di
 * terze parti che dichiari lo stesso simbolo. Si definiscono invece delle
 * inline sui nomi POSIX solo dove mancano. */

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
