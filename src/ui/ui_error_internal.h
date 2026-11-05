#pragma once
#include "ui/ui.h"
/* First failure sticks until explicit clear or context reset. No allocation. */
void ui_report_error(UIContext *ctx,UIErrorCode code,UIErrorOperation operation,const char *id);
