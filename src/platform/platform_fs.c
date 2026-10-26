/* PORTING (VALUTAZIONI.md E13, 2026-09-25) -- unica implementazione delle
 * chiamate al file system. Tutto il resto del motore parla solo con
 * platform_fs.h.
 *
 * Il file e' diviso in due blocchi condizionali con lo stesso elenco di
 * funzioni nello stesso ordine, cosi' una modifica da una parte si vede
 * subito se manca dall'altra. Le funzioni che non dipendono dal sistema
 * (platform_make_dirs, la normalizzazione dei separatori) stanno in fondo,
 * scritte una volta sola. */

/* Definite PRIMA di ogni include: con -std=c11 puro glibc nasconde
 * realpath, readlink e setenv, e questo e' l'unico file del motore che li
 * usa. Cosi' compila con qualunque -std lo raggiunga. */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "platform/platform_fs.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)

/* --------------------------------------------------------------- WINDOWS */
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>

bool platform_path_exists(const char *path) {
  return path && *path && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

bool platform_file_exists(const char *path) {
  if (!path || !*path)
    return false;
  DWORD a = GetFileAttributesA(path);
  return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool platform_dir_exists(const char *path) {
  if (!path || !*path)
    return false;
  DWORD a = GetFileAttributesA(path);
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool platform_file_size(const char *path, long long *outBytes) {
  if (!path || !outBytes)
    return false;
  WIN32_FILE_ATTRIBUTE_DATA d;
  if (!GetFileAttributesExA(path, GetFileExInfoStandard, &d))
    return false;
  *outBytes = ((long long)d.nFileSizeHigh << 32) | (long long)d.nFileSizeLow;
  return true;
}

bool platform_file_mtime(const char *path, long long *outSeconds) {
  if (!path || !outSeconds)
    return false;
  WIN32_FILE_ATTRIBUTE_DATA d;
  if (!GetFileAttributesExA(path, GetFileExInfoStandard, &d))
    return false;
  /* FILETIME conta in unita' di 100 ns dal 1601: si divide per avere
   * secondi. Il motore confronta solo due valori fra loro (asset piu'
   * recente del derivato?), quindi l'epoca diversa da POSIX non conta. */
  unsigned long long t =
      ((unsigned long long)d.ftLastWriteTime.dwHighDateTime << 32) |
      (unsigned long long)d.ftLastWriteTime.dwLowDateTime;
  *outSeconds = (long long)(t / 10000000ULL);
  return true;
}

bool platform_make_dir(const char *path) {
  if (!path || !*path)
    return false;
  if (_mkdir(path) == 0)
    return true;
  return platform_dir_exists(path);
}

bool platform_remove_file(const char *path) {
  return path && *path && DeleteFileA(path);
}

bool platform_remove_dir(const char *path) {
  return path && *path && RemoveDirectoryA(path);
}

bool platform_get_cwd(char *out, size_t cap) {
  if (!out || cap == 0)
    return false;
  if (!_getcwd(out, (int)cap))
    return false;
  platform_fs_normalize_separators(out);
  return true;
}

bool platform_set_cwd(const char *path) { return path && _chdir(path) == 0; }

bool platform_real_path(const char *path, char *out, size_t cap) {
  if (!path || !out || cap == 0)
    return false;
  DWORD n = GetFullPathNameA(path, (DWORD)cap, out, NULL);
  if (n == 0 || n >= cap)
    return false;
  platform_fs_normalize_separators(out);
  /* GetFullPathNameA non verifica l'esistenza, realpath(3) si': si allinea
   * al comportamento POSIX, altrimenti project.c accetterebbe cartelle di
   * progetto inesistenti solo su Windows. */
  return platform_dir_exists(out) || platform_file_exists(out);
}

bool platform_exe_dir(char *out, size_t cap) {
  if (!out || cap == 0)
    return false;
  DWORD n = GetModuleFileNameA(NULL, out, (DWORD)cap);
  if (n == 0 || n >= cap)
    return false;
  out[n] = '\0';
  platform_fs_normalize_separators(out);
  char *slash = strrchr(out, '/');
  if (!slash)
    return false;
  *slash = '\0';
  return true;
}

bool platform_dir_open(PlatformDirIter *it, const char *path) {
  if (!it || !path || !*path)
    return false;
  memset(it, 0, sizeof(*it));
  /* FindFirstFile vuole un pattern, non una cartella. */
  int n = snprintf(it->pattern, sizeof(it->pattern), "%s\\*", path);
  if (n <= 0 || (size_t)n >= sizeof(it->pattern))
    return false;
  WIN32_FIND_DATAA *fd = (WIN32_FIND_DATAA *)it->scratch;
  HANDLE h = FindFirstFileA(it->pattern, fd);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  it->handle = (void *)h;
  it->pending = true; /* la prima voce e' gia' stata letta da FindFirstFile */
  return true;
}

bool platform_dir_next(PlatformDirIter *it, PlatformDirEntry *out) {
  if (!it || !it->handle || !out)
    return false;
  WIN32_FIND_DATAA *fd = (WIN32_FIND_DATAA *)it->scratch;
  for (;;) {
    if (!it->pending) {
      if (!FindNextFileA((HANDLE)it->handle, fd))
        return false;
    }
    it->pending = false;
    const char *name = fd->cFileName;
    if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
      continue;
    snprintf(out->name, sizeof(out->name), "%s", name);
    out->isDir = (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return true;
  }
}

void platform_dir_close(PlatformDirIter *it) {
  if (it && it->handle) {
    FindClose((HANDLE)it->handle);
    it->handle = NULL;
  }
}

bool platform_setenv(const char *name, const char *value) {
  /* _putenv_s e' l'equivalente MSVC di setenv(3) con overwrite=1. */
  return name && value && _putenv_s(name, value) == 0;
}

#else

/* ----------------------------------------------------------------- POSIX */
#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

bool platform_path_exists(const char *path) {
  if (!path || !*path)
    return false;
  struct stat st;
  return stat(path, &st) == 0;
}

bool platform_file_exists(const char *path) {
  if (!path || !*path)
    return false;
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool platform_dir_exists(const char *path) {
  if (!path || !*path)
    return false;
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool platform_file_size(const char *path, long long *outBytes) {
  if (!path || !outBytes)
    return false;
  struct stat st;
  if (stat(path, &st) != 0)
    return false;
  *outBytes = (long long)st.st_size;
  return true;
}

bool platform_file_mtime(const char *path, long long *outSeconds) {
  if (!path || !outSeconds)
    return false;
  struct stat st;
  if (stat(path, &st) != 0)
    return false;
  *outSeconds = (long long)st.st_mtime;
  return true;
}

bool platform_make_dir(const char *path) {
  if (!path || !*path)
    return false;
  if (mkdir(path, 0755) == 0)
    return true;
  return platform_dir_exists(path);
}

bool platform_remove_file(const char *path) {
  return path && *path && unlink(path) == 0;
}

bool platform_remove_dir(const char *path) {
  return path && *path && rmdir(path) == 0;
}

bool platform_get_cwd(char *out, size_t cap) {
  return out && cap > 0 && getcwd(out, cap) != NULL;
}

bool platform_set_cwd(const char *path) { return path && chdir(path) == 0; }

bool platform_real_path(const char *path, char *out, size_t cap) {
  if (!path || !out || cap == 0)
    return false;
  /* realpath(3) senza secondo argomento allocherebbe: si usa un buffer
   * locale grande PATH_MAX come vuole lo standard, e si copia. */
  char buf[PATH_MAX];
  if (!realpath(path, buf))
    return false;
  size_t n = strlen(buf);
  if (n + 1 > cap)
    return false; /* meglio fallire che troncare un percorso */
  memcpy(out, buf, n + 1);
  return true;
}

bool platform_exe_dir(char *out, size_t cap) {
  if (!out || cap == 0)
    return false;
#if defined(__APPLE__)
  uint32_t n = (uint32_t)cap;
  if (_NSGetExecutablePath(out, &n) != 0)
    return false;
#else
  ssize_t n = readlink("/proc/self/exe", out, cap - 1);
  if (n <= 0)
    return false;
  out[n] = '\0';
#endif
  char *slash = strrchr(out, '/');
  if (!slash)
    return false;
  *slash = '\0';
  return true;
}

bool platform_dir_open(PlatformDirIter *it, const char *path) {
  if (!it || !path || !*path)
    return false;
  memset(it, 0, sizeof(*it));
  DIR *d = opendir(path);
  if (!d)
    return false;
  it->handle = (void *)d;
  return true;
}

bool platform_dir_next(PlatformDirIter *it, PlatformDirEntry *out) {
  if (!it || !it->handle || !out)
    return false;
  struct dirent *e;
  while ((e = readdir((DIR *)it->handle)) != NULL) {
    const char *name = e->d_name;
    if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
      continue;
    snprintf(out->name, sizeof(out->name), "%s", name);
#ifdef DT_DIR
    /* d_type e' un'estensione: su alcuni file system vale DT_UNKNOWN e
     * tocca comunque fare stat(). Si usa quando c'e' perche' risparmia una
     * syscall per voce, e in cartelle da centinaia di asset si sente. */
    if (e->d_type == DT_DIR) {
      out->isDir = true;
      return true;
    }
    if (e->d_type != DT_UNKNOWN) {
      out->isDir = false;
      return true;
    }
#endif
    out->isDir = false;
    return true;
  }
  return false;
}

void platform_dir_close(PlatformDirIter *it) {
  if (it && it->handle) {
    closedir((DIR *)it->handle);
    it->handle = NULL;
  }
}

bool platform_setenv(const char *name, const char *value) {
  return name && value && setenv(name, value, 1) == 0;
}

#endif

/* --------------------------------------------------- comune ai due mondi */

void platform_fs_normalize_separators(char *path) {
  if (!path)
    return;
  for (char *p = path; *p; ++p)
    if (*p == '\\')
      *p = '/';
}

bool platform_make_dirs(const char *path) {
  if (!path || !*path)
    return false;
  char buf[PLATFORM_PATH_MAX];
  size_t n = strlen(path);
  if (n + 1 > sizeof(buf))
    return false;
  memcpy(buf, path, n + 1);
  platform_fs_normalize_separators(buf);
  /* Si parte da 1 per non trattare la '/' iniziale di un percorso assoluto
   * come una cartella vuota da creare. */
  for (size_t i = 1; i < n; ++i) {
    if (buf[i] != '/')
      continue;
    buf[i] = '\0';
    if (buf[0] && !platform_make_dir(buf))
      return false;
    buf[i] = '/';
  }
  return platform_make_dir(buf);
}

bool platform_dir_entry_is_dir(const char *dirPath, const PlatformDirEntry *entry) {
  if (!dirPath || !entry)
    return false;
  if (entry->isDir)
    return true;
  /* d_type puo' dire "non lo so" (DT_UNKNOWN su certi file system): in quel
   * caso isDir esce false e serve la verifica vera. Chi deve distinguere
   * cartelle da file passa sempre da qui, non dal campo grezzo. */
  char full[PLATFORM_PATH_MAX];
  int k = snprintf(full, sizeof(full), "%s/%s", dirPath, entry->name);
  if (k <= 0 || (size_t)k >= sizeof(full))
    return false;
  return platform_dir_exists(full);
}
