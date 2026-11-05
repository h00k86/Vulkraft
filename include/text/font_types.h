#pragma once
/* Backend-owned font. UI only borrows pointers and obtains metrics via UIRenderer.
 * A backend defines struct Font; do not mix fonts from incompatible backends.
 * Keep fonts alive while referenced by widgets, popups or recorded draw commands. */
typedef struct Font Font;
