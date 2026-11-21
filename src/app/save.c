#include "save.h"

bool save_resolve_path(const char *explicit_path, const char *xdg_data_home, const char *home,
                       const char *exe_dir, char *out, size_t size)
{
    (void)explicit_path;
    (void)xdg_data_home;
    (void)home;
    (void)exe_dir;
    if (out != NULL && size > 0) {
        out[0] = '\0';
    }
    return false;
}

SaveLoadResult save_load(const char *path, MetaState *meta)
{
    (void)path;
    if (meta != NULL) {
        meta_init_default(meta);
    }
    return SAVE_LOAD_MISSING;
}

bool save_write(const char *path, const MetaState *meta, const SaveIoOps *ops)
{
    (void)path;
    (void)meta;
    (void)ops;
    return false;
}
