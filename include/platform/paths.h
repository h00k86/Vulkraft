#pragma once
#include <stdbool.h>
#include <stddef.h>
#define APP_PATH_MAX 512
#define APP_ID "graphics-app-base"
/* Legacy names for the default application resource root. APP.resources
 * overrides resolution for independent application bundles. */
void paths_engine_dir_init(void);
bool paths_engine_dir(char *out, size_t out_size);
bool paths_engine_file(char *out, size_t out_size, const char *relative);
bool paths_user_config_dir(char *out, size_t out_size, bool create_dir);
bool paths_settings_file(char *out, size_t out_size, bool create_dir);
bool paths_user_data_dir(char *out, size_t out_size, bool create_dir);
