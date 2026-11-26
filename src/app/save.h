#ifndef INK_APP_SAVE_H
#define INK_APP_SAVE_H

#include "game.h"
#include "meta.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

typedef enum {
    SAVE_LOAD_OK = 0,
    SAVE_LOAD_MISSING,
    SAVE_LOAD_CORRUPT,
    SAVE_LOAD_IO_ERROR
} SaveLoadResult;

typedef struct {
    int (*mkdir)(const char *path, mode_t mode);
    FILE *(*fopen)(const char *path, const char *mode);
    size_t (*fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *stream);
    int (*fflush)(FILE *stream);
    int (*fclose)(FILE *stream);
    int (*rename)(const char *oldpath, const char *newpath);
    int (*remove)(const char *path);
} SaveIoOps;

bool save_resolve_path(const char *explicit_path, const char *xdg_data_home, const char *home,
                       const char *exe_dir, char *out, size_t size);
SaveLoadResult save_load(const char *path, MetaState *meta);
bool save_write(const char *path, const MetaState *meta, const SaveIoOps *ops);
bool save_persist_meta(const char *path, bool enabled, const MetaState *meta, bool *dirty,
                       const SaveIoOps *ops);
bool save_retry_if_dirty(const char *path, bool enabled, const MetaState *meta, bool *dirty,
                         const SaveIoOps *ops);
bool save_finish_run_on_exit(const char *path, bool enabled, MetaState *meta, bool *dirty,
                             GameState *game, bool active_run, MetaSettleBreakdown *out_breakdown,
                             const SaveIoOps *ops);
#define SAVE_BACKUP_SLOTS 10

typedef enum {
    SAVE_BACKUP_CREATED = 0,
    SAVE_BACKUP_ALREADY_KEPT,
    SAVE_BACKUP_FAILED
} SaveBackupResult;

typedef struct {
    int (*link)(const char *oldpath, const char *newpath);
    int (*unlink)(const char *path);
} SaveBackupOps;

SaveBackupResult save_backup_unreadable(const char *path, char *out_path, size_t out_cap);
SaveBackupResult save_backup_unreadable_with(const char *path, char *out_path, size_t out_cap,
                                             const SaveBackupOps *ops);

#endif
