#pragma once
/* Allocation-free diagnostics shared by frame admission and stateful widgets.
 * Backend/draw-list errors retain their own, more specific contracts. */
#define UI_ERROR_ID_CAPACITY 256
typedef enum {
  UI_ERROR_NONE, UI_ERROR_ARGUMENT, UI_ERROR_STATE, UI_ERROR_LIMIT, UI_ERROR_MEMORY
} UIErrorCode;
typedef enum {
  UI_OPERATION_NONE, UI_OPERATION_CONTEXT, UI_OPERATION_BEGIN_FRAME,
  UI_OPERATION_BUTTON, UI_OPERATION_TEXT_FIELD, UI_OPERATION_DROPDOWN,
  UI_OPERATION_MENU_ROOT, UI_OPERATION_MENU_SUBMIT, UI_OPERATION_MENU_CHILD,
  UI_OPERATION_ID_STACK
} UIErrorOperation;
typedef struct {
  UIErrorCode code;
  UIErrorOperation operation;
  char widget_id[UI_ERROR_ID_CAPACITY]; /* owned, NUL-terminated, bounded copy */
} UIErrorInfo;
const char *ui_error_code_name(UIErrorCode code);
