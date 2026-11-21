#ifndef INK_APP_PATHS_H
#define INK_APP_PATHS_H

#include <stdbool.h>
#include <stddef.h>

// An empty relative path checks the assets directory itself.
// The output path is usable only on success.
bool app_resolve_asset_path(const char *relative_path, char *path, size_t size);

#endif
