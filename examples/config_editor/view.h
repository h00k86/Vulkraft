#pragma once
#include "editor.h"
#include "ui/ui.h"
/* Only bridge between the document/controller and the portable UI. */
void config_editor_draw(ConfigEditor *editor,Font *font,float width,float height);
/* Deterministic UI smoke, no desktop input/clipboard operations. */
size_t config_editor_smoke_event(int frame,InputKeyEvent *event);
bool config_editor_smoke_result(const ConfigEditor *editor);
