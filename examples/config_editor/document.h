#pragma once
#include <stdbool.h>
#include <stddef.h>
#define CONFIG_MAX_ITEMS 32
#define CONFIG_NAME_CAP 96
#define CONFIG_NOTE_CAP 256
#define CONFIG_PATH_CAP 1024
#define CONFIG_ERROR_CAP 192
#define CONFIG_FILE_CAP 65536
/* Pure document: no widget focus, renderer, window or native library types. */
typedef struct { unsigned id; char name[CONFIG_NAME_CAP],note[CONFIG_NOTE_CAP]; bool enabled; int level; } ConfigItem;
typedef struct { ConfigItem items[CONFIG_MAX_ITEMS]; int count; unsigned next_id; } ConfigDocument;
void config_document_init(ConfigDocument *document);
bool config_document_equal(const ConfigDocument *a,const ConfigDocument *b);
bool config_document_valid(const ConfigDocument *document);
bool config_document_add(ConfigDocument *document);
bool config_document_remove(ConfigDocument *document,int index);
/* Transactional: destination document/file stays unchanged on failure. */
bool config_document_load(ConfigDocument *document,const char *path,char error[CONFIG_ERROR_CAP]);
bool config_document_save(const ConfigDocument *document,const char *path,char error[CONFIG_ERROR_CAP]);
