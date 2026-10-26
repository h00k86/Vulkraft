#include "platform/paths.h"
#include "platform/platform_fs.h"
#include "log/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static char g_engineDir[APP_PATH_MAX];

static bool path_write(char *out, size_t out_size, const char *format,
                       const char *a, const char *b, const char *c) {
  if (!out || out_size == 0 || !format)
    return false;
  int written = snprintf(out, out_size, format, a ? a : "", b ? b : "",
                         c ? c : "");
  return written >= 0 && (size_t)written < out_size;
}

static bool mkdir_recursive(const char *path) {
  if (!path || !path[0])
    return false;
  if (platform_make_dirs(path))
    return true;
  LOG_WARN("Impossibile creare directory utente %s: %s", path,
           strerror(errno));
  return false;
}

void paths_engine_dir_init(void) {
  if (g_engineDir[0]) return;
  if (platform_exe_dir(g_engineDir, sizeof(g_engineDir))) {
    char *slash = strrchr(g_engineDir, '/');
    if (slash && strcmp(slash + 1, "build") == 0) *slash = '\0';
    return;
  }
  if (!platform_get_cwd(g_engineDir, sizeof(g_engineDir)))
    snprintf(g_engineDir, sizeof(g_engineDir), ".");
}

bool paths_engine_dir(char *out, size_t out_size) {
  paths_engine_dir_init();
  return path_write(out, out_size, "%s", g_engineDir, NULL, NULL);
}

bool paths_engine_file(char *out, size_t out_size, const char *relative) {
  paths_engine_dir_init();
  return relative && path_write(out, out_size, "%s/%s", g_engineDir, relative, NULL);
}

static bool paths_xdg_dir(char *out, size_t out_size, const char *xdg_name,
                          const char *home_suffix, bool create_dir) {
  const char *base = getenv(xdg_name);
  char fallback[APP_PATH_MAX];
  if (!base || !base[0]) {
    const char *home = getenv("HOME");
    if (!home || !home[0]) {
      LOG_WARN("%s e HOME non disponibili: preferenze non persistenti", xdg_name);
      return false;
    }
    if (!path_write(fallback, sizeof(fallback), "%s%s", home, home_suffix,
                    NULL))
      return false;
    base = fallback;
  }
  if (!path_write(out, out_size, "%s/%s", base, APP_ID, NULL))
    return false;
  return !create_dir || mkdir_recursive(out);
}

bool paths_user_config_dir(char *out, size_t out_size, bool create_dir) {
  return paths_xdg_dir(out, out_size, "XDG_CONFIG_HOME", "/.config",
                       create_dir);
}

bool paths_settings_file(char *out, size_t out_size, bool create_dir) {
  char config_dir[APP_PATH_MAX];
  if (!paths_user_config_dir(config_dir, sizeof(config_dir), create_dir))
    return false;
  return path_write(out, out_size, "%s/settings.cfg", config_dir, NULL,
                    NULL);
}

bool paths_user_data_dir(char *out, size_t out_size, bool create_dir) {
  return paths_xdg_dir(out, out_size, "XDG_DATA_HOME", "/.local/share",
                       create_dir);
}
