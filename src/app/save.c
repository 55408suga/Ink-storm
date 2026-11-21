#include "save.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

enum { SAVE_MAX_BYTES = 8192 };

static bool copy_checked(char *out, size_t size, const char *fmt, const char *arg)
{
    const int written = snprintf(out, size, fmt, arg);
    if (written < 0 || (size_t)written >= size) {
        out[0] = '\0';
        return false;
    }
    return true;
}

bool save_resolve_path(const char *explicit_path, const char *xdg_data_home, const char *home,
                       const char *exe_dir, char *out, size_t size)
{
    if (out == NULL || size == 0) {
        return false;
    }
    out[0] = '\0';

    if (explicit_path != NULL && explicit_path[0] != '\0') {
        return copy_checked(out, size, "%s", explicit_path);
    }
    if (xdg_data_home != NULL && xdg_data_home[0] == '/') {
        return copy_checked(out, size, "%s/inkstorm/save.txt", xdg_data_home);
    }
    if (home != NULL && home[0] != '\0') {
        return copy_checked(out, size, "%s/.local/share/inkstorm/save.txt", home);
    }
    if (exe_dir != NULL && exe_dir[0] != '\0') {
        return copy_checked(out, size, "%s/inkstorm_save.txt", exe_dir);
    }
    return false;
}

SaveLoadResult save_load(const char *path, MetaState *meta)
{
    if (meta != NULL) {
        meta_init_default(meta);
    }
    if (path == NULL || path[0] == '\0' || meta == NULL) {
        return SAVE_LOAD_MISSING;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return SAVE_LOAD_MISSING;
        }
        return SAVE_LOAD_IO_ERROR;
    }
    if (S_ISDIR(st.st_mode)) {
        return SAVE_LOAD_IO_ERROR;
    }
    if (st.st_size > (off_t)SAVE_MAX_BYTES) {
        return SAVE_LOAD_CORRUPT;
    }

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return SAVE_LOAD_MISSING;
        }
        return SAVE_LOAD_IO_ERROR;
    }

    char buf[SAVE_MAX_BYTES + 2];
    const size_t n = fread(buf, 1, SAVE_MAX_BYTES + 1, fp);
    const bool read_err = ferror(fp) != 0;
    const int close_rc = fclose(fp);
    if (read_err || close_rc != 0) {
        return SAVE_LOAD_IO_ERROR;
    }
    if (n > (size_t)SAVE_MAX_BYTES) {
        return SAVE_LOAD_CORRUPT;
    }
    if (memchr(buf, '\0', n) != NULL) {
        return SAVE_LOAD_CORRUPT;
    }
    buf[n] = '\0';

    if (!meta_deserialize(meta, buf)) {
        meta_init_default(meta);
        return SAVE_LOAD_CORRUPT;
    }
    return SAVE_LOAD_OK;
}

static bool mkdir_parents(const char *file_path, int (*mkdir_fn)(const char *, mode_t))
{
    char dir[PATH_MAX];
    const size_t path_len = strlen(file_path);
    if (path_len >= sizeof dir) {
        return false;
    }
    memcpy(dir, file_path, path_len + 1);

    char *last_slash = strrchr(dir, '/');
    if (last_slash == NULL || last_slash == dir) {
        return true;
    }
    *last_slash = '\0';

    for (char *p = dir + 1; *p != '\0'; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir_fn(dir, 0700) != 0 && errno != EEXIST) {
                return false;
            }
            *p = '/';
        }
    }
    if (mkdir_fn(dir, 0700) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

bool save_write(const char *path, const MetaState *meta, const SaveIoOps *ops)
{
    if (path == NULL || path[0] == '\0' || meta == NULL) {
        return false;
    }

    SaveIoOps io = {
        .mkdir = mkdir,
        .fopen = fopen,
        .fwrite = fwrite,
        .fflush = fflush,
        .fclose = fclose,
        .rename = rename,
        .remove = remove,
    };
    if (ops != NULL) {
        if (ops->mkdir != NULL) {
            io.mkdir = ops->mkdir;
        }
        if (ops->fopen != NULL) {
            io.fopen = ops->fopen;
        }
        if (ops->fwrite != NULL) {
            io.fwrite = ops->fwrite;
        }
        if (ops->fflush != NULL) {
            io.fflush = ops->fflush;
        }
        if (ops->fclose != NULL) {
            io.fclose = ops->fclose;
        }
        if (ops->rename != NULL) {
            io.rename = ops->rename;
        }
        if (ops->remove != NULL) {
            io.remove = ops->remove;
        }
    }

    if (!mkdir_parents(path, io.mkdir)) {
        return false;
    }

    char tmp_path[PATH_MAX];
    const int tmp_len = snprintf(tmp_path, sizeof tmp_path, "%s.tmp", path);
    if (tmp_len < 0 || (size_t)tmp_len >= sizeof tmp_path) {
        return false;
    }

    char payload[META_SERIALIZE_CAP + 64];
    const size_t len = meta_serialize(meta, payload, sizeof payload);
    if (len == 0 || len >= sizeof payload) {
        return false;
    }

    FILE *fp = io.fopen(tmp_path, "wb");
    if (fp == NULL) {
        return false;
    }

    const size_t written = io.fwrite(payload, 1, len, fp);
    const bool write_ok = (written == len);
    const bool flush_ok = write_ok && (io.fflush(fp) == 0);
    const bool close_ok = (io.fclose(fp) == 0);
    if (!write_ok || !flush_ok || !close_ok) {
        (void)io.remove(tmp_path);
        return false;
    }

    if (io.rename(tmp_path, path) != 0) {
        (void)io.remove(tmp_path);
        return false;
    }
    return true;
}
