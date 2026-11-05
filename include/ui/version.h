#pragma once

/* Single source of the SDK version. CMake and packaging read these definitions.
 * Change decimal components here; see docs/UI_COMPATIBILITY.md. */
#define UI_VERSION_MAJOR 0
#define UI_VERSION_MINOR 2
#define UI_VERSION_PATCH 0

#define UI_VERSION_DETAIL_STRINGIFY_(value) #value
#define UI_VERSION_DETAIL_STRINGIFY(value) UI_VERSION_DETAIL_STRINGIFY_(value)
#define UI_VERSION_STRING UI_VERSION_DETAIL_STRINGIFY(UI_VERSION_MAJOR) "." \
                          UI_VERSION_DETAIL_STRINGIFY(UI_VERSION_MINOR) "." \
                          UI_VERSION_DETAIL_STRINGIFY(UI_VERSION_PATCH)

/* Version of the linked core, independent of the headers used by the caller.
 * Static storage; no context, backend or allocation required. */
const char *ui_version_string(void);
