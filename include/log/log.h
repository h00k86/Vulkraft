#pragma once
#include <stdio.h>

#define COLOR_RESET "\x1b[0m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_RED "\x1b[31m"

/* Errors remain visible in all builds. */
#define LOG_ERROR(fmt, ...)                                                  \
  fprintf(stderr, COLOR_RED "[ERROR] " fmt COLOR_RESET "\n", ##__VA_ARGS__)

#if defined(DEBUG)

  #define LOG_INFO(fmt, ...)                                                 \
    printf(COLOR_GREEN "[INFO] " fmt COLOR_RESET "\n", ##__VA_ARGS__)

  #define LOG_WARN(fmt, ...)                                                 \
    printf(COLOR_YELLOW "[WARN] " fmt COLOR_RESET "\n", ##__VA_ARGS__)

#else

  /* Check format/arguments without runtime I/O in Release. */

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
