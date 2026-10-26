#pragma once
#include <stdio.h>

#define COLOR_RESET "\x1b[0m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_RED "\x1b[31m"

// LOG_ERROR resta attivo SEMPRE, anche in build senza DEBUG — in giro
// per il progetto ci sono parecchi "LOG_ERROR(...); exit(1);" (init
// Vulkan, caricamento shader, ecc.): senza questo, in una build di
// release un errore fatale farebbe uscire il programma senza stampare
// nulla, zero indizio del perché.
#define LOG_ERROR(fmt, ...)                                                  \
  fprintf(stderr, COLOR_RED "[ERROR] " fmt COLOR_RESET "\n", ##__VA_ARGS__)

#if defined(DEBUG) && !defined(PERFORMANCE_LAB_BUILD)

  #define LOG_INFO(fmt, ...)                                                 \
    printf(COLOR_GREEN "[INFO] " fmt COLOR_RESET "\n", ##__VA_ARGS__)

  #define LOG_WARN(fmt, ...)                                                 \
    printf(COLOR_YELLOW "[WARN] " fmt COLOR_RESET "\n", ##__VA_ARGS__)

#else

  /* MACROTASK AUDIT-R1 fase E: release usava una macro VUOTA qui, che
   * scarta silenziosamente ogni argomento -- una variabile calcolata
   * SOLO per un LOG_INFO/LOG_WARN diventa "unused"/"set but not used"
   * in release (mai in debug, dove printf la referenzia davvero), un
   * falso positivo del compilatore da eliminare col codice giusto, non
   * con -Wno-unused-variable. Stesso ramo morto gia' usato da
   * PERFORMANCE_LAB_BUILD (formato controllato dal compilatore,
   * argomenti referenziati, zero I/O reale in nessuno dei due casi):
   * release e perf condividono ora la STESSA macro, nessuna
   * distinzione necessaria fra i due per questo specifico contratto. */
  #define LOG_INFO(fmt, ...)                                                 \
    do {                                                                     \
      if (0)                                                                \
        printf(fmt, ##__VA_ARGS__);                                          \
    } while (0)
  #define LOG_WARN(fmt, ...)                                                 \
    do {                                                                     \
      if (0)                                                                \
        printf(fmt, ##__VA_ARGS__);                                          \
    } while (0)

#endif
