#define _POSIX_C_SOURCE 200809L
#include "platform/platform_fs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void) {
  char root[] = "/tmp/graphics-path-XXXXXX"; assert(mkdtemp(root));
  char out[PLATFORM_PATH_MAX], path[PLATFORM_PATH_MAX], child[PLATFORM_PATH_MAX];
  snprintf(child, sizeof(child), "%s/child", root); assert(platform_make_dir(child));
  assert(platform_resolve_within(root, child, false, out, sizeof(out)));
  snprintf(path, sizeof(path), "%s/child/..", root);
  assert(platform_resolve_within(root, path, false, out, sizeof(out)) && !strcmp(out, root));
  snprintf(path, sizeof(path), "%s/../", root);
  assert(!platform_resolve_within(root, path, false, out, sizeof(out)));
  assert(platform_resolve_within("/", root, false, out, sizeof(out)));
  snprintf(path, sizeof(path), "%s/outside", root); assert(!symlink("/tmp", path));
  assert(!platform_resolve_within(root, path, false, out, sizeof(out))); unlink(path);
  snprintf(path, sizeof(path), "%s/dangling", root); assert(!symlink("/does-not-exist", path));
  assert(!platform_resolve_within(root, path, true, out, sizeof(out))); unlink(path);
  snprintf(path, sizeof(path), "%s/new.txt", root);
  assert(platform_resolve_within(root, path, true, out, sizeof(out)) && !strcmp(out, path));
  assert(!platform_resolve_within(root, path, false, out, sizeof(out)));
  strcpy(out, "unchanged");
  assert(!platform_resolve_within(root, path, true, out, 2) && !strcmp(out, "unchanged"));
  snprintf(path, sizeof(path), "%s/missing/new.txt", root);
  assert(!platform_resolve_within(root, path, true, out, sizeof(out)));
  assert(!rmdir(child)); assert(!rmdir(root));
  puts("Canonical picker paths, symlinks, missing leaves and root slash: PASS");
}
