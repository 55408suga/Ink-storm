#include "paths.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static bool has_parent_segment(const char *relative_path)
{
    const char *p = relative_path;
    while (*p != '\0') {
        const char *sep = strchr(p, '/');
        const size_t len = sep != NULL ? (size_t)(sep - p) : strlen(p);
        if (len == 2 && p[0] == '.' && p[1] == '.') {
            return true;
        }
        if (sep == NULL) {
            break;
        }
        p = sep + 1;
    }
    return false;
}

bool app_resolve_asset_path(const char *relative_path, char *path, size_t size)
{
    if (relative_path == NULL || path == NULL || size == 0 || relative_path[0] == '/' ||
        has_parent_segment(relative_path)) {
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
