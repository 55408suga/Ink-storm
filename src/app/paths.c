#include "paths.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

bool app_resolve_asset_path(const char *relative_path, char *path, size_t size)
{
    if (relative_path[0] == '/') {
        return false;
    }

    char executable[PATH_MAX];
    const ssize_t length = readlink("/proc/self/exe", executable, sizeof executable - 1);
    if (length < 0 || (size_t)length == sizeof executable - 1) {
        return false;
    }
    executable[length] = '\0';

    char *const slash = strrchr(executable, '/');
    if (slash == NULL) {
        return false;
    }
    *slash = '\0';

    const int written = snprintf(path, size, "%s/assets/%s", executable, relative_path);
    return written >= 0 && (size_t)written < size && access(path, F_OK) == 0;
}
