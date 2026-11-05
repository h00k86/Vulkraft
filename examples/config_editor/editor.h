#pragma once
#include "document.h"
#include <stdint.h>
#define CONFIG_HISTORY_CAP 33 /* Initial state plus 32 document edits. */
typedef struct { ConfigDocument document; int selected; } ConfigSnapshot;
typedef enum { CONFIG_ACTION_NONE,CONFIG_ACTION_NEW,CONFIG_ACTION_OPEN,CONFIG_ACTION_CLOSE } ConfigAction;
typedef enum { CONFIG_PICKER_NONE,CONFIG_PICKER_OPEN,CONFIG_PICKER_SAVE } ConfigPicker;
typedef enum { CONFIG_CANCEL,CONFIG_DISCARD,CONFIG_SAVE } ConfigDecision;
typedef struct {
  ConfigDocument document,saved;
  char path[CONFIG_PATH_CAP],root[CONFIG_PATH_CAP],message[CONFIG_ERROR_CAP];
  int selected;
  unsigned generation; /* UI IDs must not reuse an old document's editor state. */
  ConfigAction pending;
  ConfigPicker picker;
  bool confirm,conflict,close_ready;
  ConfigSnapshot history[CONFIG_HISTORY_CAP];
  int history_count,history_cursor;
  uint64_t history_group;
} ConfigEditor;
/* Call after mutation. Nonzero group merges consecutive edits of one text
 * field; zero records a discrete operation. History also permits temporary
 * invalid text (e.g. an empty name), just like the live editor. */
void config_editor_checkpoint(ConfigEditor *editor,uint64_t group);
void config_editor_history_break(ConfigEditor *editor);
bool config_editor_can_undo(const ConfigEditor *editor);
bool config_editor_can_redo(const ConfigEditor *editor);
bool config_editor_undo(ConfigEditor *editor);
bool config_editor_redo(ConfigEditor *editor);
bool config_editor_init(ConfigEditor *editor,const char *root);
bool config_editor_dirty(const ConfigEditor *editor);
void config_editor_request(ConfigEditor *editor,ConfigAction action);
void config_editor_decide(ConfigEditor *editor,ConfigDecision decision);
void config_editor_save(ConfigEditor *editor,bool save_as);
bool config_editor_choose(ConfigEditor *editor,const char *path);
void config_editor_cancel_picker(ConfigEditor *editor);
/* Conflict never silently overwrites the changed file. Copy resumes a pending
 * action only after a successful save to another path; cancel abandons it. */
void config_editor_resolve_conflict(ConfigEditor *editor,bool save_copy);
