#include "save.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { SAVE_MAX_BYTES = 8192, SAVE_TMP_CREATE_ATTEMPTS = 128 };

static atomic_uint_fast32_t g_save_tmp_seq = 0;

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

    const int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return SAVE_LOAD_MISSING;
        }
        return SAVE_LOAD_IO_ERROR;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        (void)close(fd);
        return SAVE_LOAD_IO_ERROR;
    }
    if (!S_ISREG(st.st_mode) || st.st_size < 0) {
        (void)close(fd);
        return SAVE_LOAD_IO_ERROR;
    }
    if (st.st_size > (off_t)SAVE_MAX_BYTES) {
        (void)close(fd);
        return SAVE_LOAD_CORRUPT;
    }

    FILE *fp = fdopen(fd, "rb");
    if (fp == NULL) {
        (void)close(fd);
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

static bool format_unique_tmp_path(const char *path, char *tmp_path, size_t cap)
{
    const uint32_t seq = (uint32_t)atomic_fetch_add(&g_save_tmp_seq, 1u);
    const unsigned long pid = (unsigned long)getpid();
    const char *slash = strrchr(path, '/');
    int n = -1;
    if (slash == NULL) {
        n = snprintf(tmp_path, cap, ".inktmp.%lu.%u", pid, (unsigned)seq);
    } else if (slash == path) {
        n = snprintf(tmp_path, cap, "/.inktmp.%lu.%u", pid, (unsigned)seq);
    } else {
        const size_t dir_len = (size_t)(slash - path);
        if (dir_len > (size_t)INT_MAX) {
            return false;
        }
        n = snprintf(tmp_path, cap, "%.*s/.inktmp.%lu.%u", (int)dir_len, path, pid, (unsigned)seq);
    }
    return n > 0 && (size_t)n < cap;
}

static FILE *open_unique_temp_stream(const char *path, const SaveIoOps *io, bool use_custom_fopen,
                                     char *tmp_path, size_t cap)
{
    if (use_custom_fopen) {
        if (!format_unique_tmp_path(path, tmp_path, cap)) {
            return NULL;
        }
        return io->fopen(tmp_path, "wb");
    }

    for (int attempt = 0; attempt < SAVE_TMP_CREATE_ATTEMPTS; ++attempt) {
        if (!format_unique_tmp_path(path, tmp_path, cap)) {
            return NULL;
        }
        const int fd = open(tmp_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd < 0) {
            if (errno == EEXIST) {
                continue;
            }
            return NULL;
        }
        FILE *fp = fdopen(fd, "wb");
        if (fp == NULL) {
            (void)close(fd);
            (void)io->remove(tmp_path);
            return NULL;
        }
        return fp;
    }
    return NULL;
}

static bool save_write_with_ops(const char *path, const MetaState *meta, const SaveIoOps *io,
                                bool use_custom_fopen)
{
    if (!mkdir_parents(path, io->mkdir)) {
        return false;
    }

    char payload[META_SERIALIZE_CAP + 64];
    const size_t len = meta_serialize(meta, payload, sizeof payload);
    if (len == 0 || len >= sizeof payload) {
        return false;
    }

    char tmp_path[PATH_MAX];
    FILE *fp = open_unique_temp_stream(path, io, use_custom_fopen, tmp_path, sizeof tmp_path);
    if (fp == NULL) {
        return false;
    }

    const size_t written = io->fwrite(payload, 1, len, fp);
    const bool write_ok = (written == len);
    const bool flush_ok = write_ok && (io->fflush(fp) == 0);
    const bool close_ok = (io->fclose(fp) == 0);
    if (!write_ok || !flush_ok || !close_ok) {
        (void)io->remove(tmp_path);
        return false;
    }

    if (io->rename(tmp_path, path) != 0) {
        (void)io->remove(tmp_path);
        return false;
    }
    return true;
}

bool save_write(const char *path, const MetaState *meta, const SaveIoOps *ops)
{
    if (path == NULL || path[0] == '\0' || meta == NULL) {
        return false;
    }

    const bool use_custom_fopen = ops != NULL && ops->fopen != NULL;
    const SaveIoOps io = {
        .mkdir = (ops != NULL && ops->mkdir != NULL) ? ops->mkdir : mkdir,
        .fopen = use_custom_fopen ? ops->fopen : fopen,
        .fwrite = (ops != NULL && ops->fwrite != NULL) ? ops->fwrite : fwrite,
        .fflush = (ops != NULL && ops->fflush != NULL) ? ops->fflush : fflush,
        .fclose = (ops != NULL && ops->fclose != NULL) ? ops->fclose : fclose,
        .rename = (ops != NULL && ops->rename != NULL) ? ops->rename : rename,
        .remove = (ops != NULL && ops->remove != NULL) ? ops->remove : remove,
    };
    return save_write_with_ops(path, meta, &io, use_custom_fopen);
}

bool save_persist_meta(const char *path, bool enabled, const MetaState *meta, bool *dirty,
                       const SaveIoOps *ops)
{
    if (!enabled || path == NULL || path[0] == '\0' || meta == NULL) {
        if (dirty != NULL) {
            *dirty = false;
        }
        return true;
    }
    if (!save_write(path, meta, ops)) {
        if (dirty != NULL) {
            *dirty = true;
        }
        return false;
    }
    if (dirty != NULL) {
        *dirty = false;
    }
    return true;
}

bool save_retry_if_dirty(const char *path, bool enabled, const MetaState *meta, bool *dirty,
                         const SaveIoOps *ops)
{
    if (dirty == NULL || !*dirty) {
        return true;
    }
    return save_persist_meta(path, enabled, meta, dirty, ops);
}

bool save_resolve_and_settle_run(MetaState *meta, GameState *game,
                                 MetaSettleBreakdown *out_breakdown)
{
    if (out_breakdown != NULL) {
        memset(out_breakdown, 0, sizeof *out_breakdown);
    }
    if (meta == NULL || game == NULL) {
        return false;
    }

    /* Drain final-kill hitstop and Act-3 boss victory aftermath so leaving or exiting right after
     * the final boss dies still settles as a campaign clear instead of an aborted run. */
    for (int t = 0; t < 30 && game->phase == RUN_PHASE_PLAYING && game_wave_goal_met(game); ++t) {
        game_step(game, NULL, NULL);
    }
    for (int t = 0; t < 90 && game_aftermath_reaches_victory(game); ++t) {
        game_step(game, NULL, NULL);
    }
    if (game->phase == RUN_PHASE_VICTORY && !game->run.run_settled) {
        return meta_settle_run(meta, game, out_breakdown);
    }
    if (game->phase != RUN_PHASE_VICTORY && game->phase != RUN_PHASE_GAME_OVER &&
        !(game->run.act_index == 0 && game->phase == RUN_PHASE_ACT_TRANSITION)) {
        const GameCommand abort_cmd = {.type = CMD_ABORT_RUN, .arg = 0};
        game_step(game, NULL, &abort_cmd);
        if ((game->phase == RUN_PHASE_GAME_OVER ||
             (game->phase == RUN_PHASE_VICTORY && game->run.endless_loop > 0)) &&
            !game->run.run_settled) {
            return meta_settle_run(meta, game, out_breakdown);
        }
    }
    return false;
}

bool save_finish_run_on_exit(const char *path, bool enabled, MetaState *meta, bool *dirty,
                             GameState *game, bool active_run, MetaSettleBreakdown *out_breakdown,
                             const SaveIoOps *ops)
{
    if (out_breakdown != NULL) {
        memset(out_breakdown, 0, sizeof *out_breakdown);
    }
    if (!enabled || meta == NULL) {
        return true;
    }

    bool settled_now = false;
    if (active_run && game != NULL) {
        settled_now = save_resolve_and_settle_run(meta, game, out_breakdown);
    }

    bool local_dirty = false;
    bool *dirty_flag = dirty != NULL ? dirty : &local_dirty;
    if (settled_now && save_persist_meta(path, enabled, meta, dirty_flag, ops)) {
        return true;
    }
    return save_retry_if_dirty(path, enabled, meta, dirty_flag, ops);
}

static FILE *open_regular_bounded_ro(const char *path, size_t max_bytes, bool nofollow)
{
    int flags = O_RDONLY | O_NONBLOCK;
#ifdef O_NOFOLLOW
    if (nofollow) {
        flags |= O_NOFOLLOW;
    }
#else
    (void)nofollow;
#endif
    const int fd = open(path, flags);
    if (fd < 0) {
        return NULL;
    }
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 ||
        (uintmax_t)st.st_size > (uintmax_t)max_bytes) {
        (void)close(fd);
        return NULL;
    }
    FILE *fp = fdopen(fd, "rb");
    if (fp == NULL) {
        (void)close(fd);
        return NULL;
    }
    return fp;
}

static bool files_identical(const char *a, const char *b)
{
    FILE *fa = open_regular_bounded_ro(a, SAVE_MAX_BACKUP_BYTES, false);
    if (fa == NULL) {
        return false;
    }
    FILE *fb = open_regular_bounded_ro(b, SAVE_MAX_BACKUP_BYTES, true);
    if (fb == NULL) {
        (void)fclose(fa);
        return false;
    }
    char ba[4096];
    char bb[4096];
    bool same = true;
    size_t total = 0;
    for (;;) {
        const size_t na = fread(ba, 1, sizeof ba, fa);
        const size_t nb = fread(bb, 1, sizeof bb, fb);
        if (na != nb || memcmp(ba, bb, na) != 0) {
            same = false;
            break;
        }
        total += na;
        if (total > (size_t)SAVE_MAX_BACKUP_BYTES) {
            same = false;
            break;
        }
        if (na < sizeof ba) {
            same = ferror(fa) == 0 && ferror(fb) == 0;
            break;
        }
    }
    const bool closed_a = fclose(fa) == 0;
    const bool closed_b = fclose(fb) == 0;
    return same && closed_a && closed_b;
}

static bool copy_into(FILE *src, FILE *dst, size_t max_bytes)
{
    char buf[4096];
    size_t total = 0;
    for (;;) {
        const size_t n = fread(buf, 1, sizeof buf, src);
        total += n;
        if (total > max_bytes) {
            return false;
        }
        if (n > 0 && fwrite(buf, 1, n, dst) != n) {
            return false;
        }
        if (n < sizeof buf) {
            return ferror(src) == 0;
        }
    }
}

static void discard_temp(const SaveBackupOps *ops, const char *tmp_path)
{
    if (ops->unlink(tmp_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "INKSTORM: could not remove temporary backup '%s'\n", tmp_path);
    }
}

/* The copy goes to a private temp file first, so an interrupted or failed copy can never be
 * mistaken for a finished backup under the final name. */
static bool copy_to_temp(const SaveBackupOps *ops, const char *path, char *tmp_path, size_t cap)
{
    const int n = snprintf(tmp_path, cap, "%s.tmpbak.XXXXXX", path);
    if (n < 0 || (size_t)n >= cap) {
        return false;
    }
    FILE *src = open_regular_bounded_ro(path, SAVE_MAX_BACKUP_BYTES, false);
    if (src == NULL) {
        return false;
    }
    const int fd = mkstemp(tmp_path);
    if (fd < 0) {
        (void)fclose(src);
        return false;
    }
    FILE *dst = fdopen(fd, "wb");
    if (dst == NULL) {
        (void)close(fd);
        discard_temp(ops, tmp_path);
        (void)fclose(src);
        return false;
    }
    bool ok = copy_into(src, dst, SAVE_MAX_BACKUP_BYTES);
    ok = (fclose(src) == 0) && ok;
    ok = (fflush(dst) == 0) && ok;
    ok = (fclose(dst) == 0) && ok;
    if (!ok) {
        discard_temp(ops, tmp_path);
    }
    return ok;
}

/* link() publishes the finished copy atomically and refuses to replace an existing file. There is
 * deliberately no rename() fallback: rename() would silently replace a backup created in the
 * meantime, so a filesystem without hard links reports failure and saving stays disabled. */
static int publish_without_replacing(const SaveBackupOps *ops, const char *tmp_path,
                                     const char *final_path)
{
    return ops->link(tmp_path, final_path) == 0 ? 0 : errno;
}

SaveBackupResult save_backup_unreadable_with(const char *path, char *out_path, size_t out_cap,
                                             const SaveBackupOps *ops)
{
    if (path == NULL || path[0] == '\0') {
        return SAVE_BACKUP_FAILED;
    }
    FILE *src_probe = open_regular_bounded_ro(path, SAVE_MAX_BACKUP_BYTES, false);
    if (src_probe == NULL) {
        return SAVE_BACKUP_FAILED;
    }
    (void)fclose(src_probe);

    const SaveBackupOps io = {
        .link = (ops != NULL && ops->link != NULL) ? ops->link : link,
        .unlink = (ops != NULL && ops->unlink != NULL) ? ops->unlink : unlink,
    };
    char tmp_path[PATH_MAX];
    bool have_copy = false;
    SaveBackupResult result = SAVE_BACKUP_FAILED;
    for (int slot = 0; slot < SAVE_BACKUP_SLOTS && result == SAVE_BACKUP_FAILED; ++slot) {
        char candidate[PATH_MAX];
        const int n = slot == 0 ? snprintf(candidate, sizeof candidate, "%s.bak", path)
                                : snprintf(candidate, sizeof candidate, "%s.bak.%d", path, slot);
        if (n < 0 || (size_t)n >= sizeof candidate) {
            break;
        }
        struct stat st;
        /* lstat() so a symlinked slot is never taken as a kept copy: its target can be the save
         * itself, which the next save_write() replaces. */
        if (lstat(candidate, &st) == 0) {
            if (S_ISREG(st.st_mode) && st.st_size >= 0 &&
                (uintmax_t)st.st_size <= (uintmax_t)SAVE_MAX_BACKUP_BYTES &&
                files_identical(path, candidate)) {
                result = SAVE_BACKUP_ALREADY_KEPT;
                if (out_path != NULL) {
                    (void)copy_checked(out_path, out_cap, "%s", candidate);
                }
            }
            continue;
        }
        if (errno != ENOENT) {
            break;
        }
        if (!have_copy) {
            have_copy = copy_to_temp(&io, path, tmp_path, sizeof tmp_path);
            if (!have_copy) {
                break;
            }
        }
        const int err = publish_without_replacing(&io, tmp_path, candidate);
        if (err == 0) {
            result = SAVE_BACKUP_CREATED;
            if (out_path != NULL) {
                (void)copy_checked(out_path, out_cap, "%s", candidate);
            }
        } else if (err != EEXIST) {
            break;
        }
    }
    if (have_copy) {
        discard_temp(&io, tmp_path);
    }
    return result;
}

SaveBackupResult save_backup_unreadable(const char *path, char *out_path, size_t out_cap)
{
    return save_backup_unreadable_with(path, out_path, out_cap, NULL);
}
