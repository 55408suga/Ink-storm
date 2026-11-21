#include "meta.h"
#include "save.h"
#include "unity.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void setUp(void) {}

void tearDown(void) {}

static char *make_temp_dir(char *buf, size_t size)
{
    const char *base = getenv("TMPDIR");
    if (base == NULL || base[0] == '\0') {
        base = "/tmp";
    }
    const int n = snprintf(buf, size, "%s/inkstorm-save-XXXXXX", base);
    TEST_ASSERT_TRUE(n > 0 && (size_t)n < size);
    return mkdtemp(buf);
}

static void join_path(char *out, size_t cap, const char *dir, const char *rel)
{
    const int n = snprintf(out, cap, "%s/%s", dir, rel);
    TEST_ASSERT_TRUE(n > 0 && (size_t)n < cap);
}

static void assert_meta_equal(const MetaState *expected, const MetaState *actual)
{
    TEST_ASSERT_EQUAL_UINT32(expected->version, actual->version);
    TEST_ASSERT_EQUAL_INT32(expected->ink_crystals, actual->ink_crystals);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected->upgrade_lv, actual->upgrade_lv, META_UPG_COUNT);
    TEST_ASSERT_EQUAL_HEX64(expected->unlocked_relic_mask, actual->unlocked_relic_mask);
    TEST_ASSERT_EQUAL_INT32(expected->best_score, actual->best_score);
    TEST_ASSERT_EQUAL_INT32(expected->best_act, actual->best_act);
    TEST_ASSERT_EQUAL_INT32(expected->runs_played, actual->runs_played);
    TEST_ASSERT_EQUAL_INT32(expected->clears, actual->clears);
    TEST_ASSERT_EQUAL_HEX32(expected->seen_hints_mask, actual->seen_hints_mask);
    TEST_ASSERT_EQUAL_UINT8(expected->settings.master_vol, actual->settings.master_vol);
    TEST_ASSERT_EQUAL_UINT8(expected->settings.bgm_vol, actual->settings.bgm_vol);
    TEST_ASSERT_EQUAL_UINT8(expected->settings.se_vol, actual->settings.se_vol);
    TEST_ASSERT_EQUAL_UINT8(expected->settings.shake_scale, actual->settings.shake_scale);
    TEST_ASSERT_EQUAL(expected->settings.post_shader_on, actual->settings.post_shader_on);
    TEST_ASSERT_EQUAL(expected->settings.hints_on, actual->settings.hints_on);
}

static void test_save_resolve_path_priority(void)
{
    typedef struct {
        const char *exp;
        const char *xdg;
        const char *home;
        const char *exe;
        size_t cap;
        bool ok;
        const char *want;
    } Case;

    const Case cases[] = {
        {"/custom/save.txt", "/xdg", "/home/u", "/bin", 128, true, "/custom/save.txt"},
        {NULL, "/xdg", "/home/u", "/bin", 128, true, "/xdg/inkstorm/save.txt"},
        {"", "relative/xdg", "/home/u", "/bin", 128, true,
         "/home/u/.local/share/inkstorm/save.txt"},
        {NULL, "", "/home/u", "/bin", 128, true, "/home/u/.local/share/inkstorm/save.txt"},
        {NULL, NULL, "", "/opt/game", 128, true, "/opt/game/inkstorm_save.txt"},
        {NULL, NULL, NULL, NULL, 128, false, ""},
        {NULL, "", "", "", 128, false, ""},
        {"/custom/save.txt", NULL, NULL, NULL, 8, false, ""},
        {NULL, "/xdg", NULL, NULL, 10, false, ""},
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        char out[128];
        memset(out, 'x', sizeof out);
        const bool ok = save_resolve_path(cases[i].exp, cases[i].xdg, cases[i].home, cases[i].exe,
                                          out, cases[i].cap);
        TEST_ASSERT_EQUAL(cases[i].ok, ok);
        if (cases[i].ok) {
            TEST_ASSERT_EQUAL_STRING(cases[i].want, out);
        }
    }
}

static void test_save_roundtrip(void)
{
    char dir[PATH_MAX];
    TEST_ASSERT_NOT_NULL(make_temp_dir(dir, sizeof dir));

    char path[PATH_MAX + 64];
    join_path(path, sizeof path, dir, "save.txt");

    MetaState src;
    meta_init_default(&src);
    src.ink_crystals = 1234;
    src.upgrade_lv[META_UPG_SHRINE_HP] = 2;
    src.upgrade_lv[META_UPG_INK_MAX] = 3;
    src.upgrade_lv[META_UPG_REROLLS] = 1;
    src.upgrade_lv[META_UPG_INK_REGEN] = 2;
    src.unlocked_relic_mask = UINT64_C(0x15);
    src.best_score = 48200;
    src.best_act = 3;
    src.runs_played = 9;
    src.clears = 2;
    src.seen_hints_mask = UINT32_C(0x0000000f);
    src.settings.master_vol = 60;
    src.settings.bgm_vol = 45;
    src.settings.se_vol = 95;
    src.settings.shake_scale = 50;
    src.settings.post_shader_on = false;
    src.settings.hints_on = false;

    TEST_ASSERT_TRUE(save_write(path, &src, NULL));

    MetaState dst;
    memset(&dst, 0xaa, sizeof dst);
    TEST_ASSERT_EQUAL_INT(SAVE_LOAD_OK, save_load(path, &dst));
    assert_meta_equal(&src, &dst);

    remove(path);
    rmdir(dir);
}

static void test_save_creates_parent_dirs(void)
{
    char dir[PATH_MAX];
    TEST_ASSERT_NOT_NULL(make_temp_dir(dir, sizeof dir));

    char sub1[PATH_MAX + 64];
    char sub2[PATH_MAX + 64];
    char path[PATH_MAX + 64];
    join_path(sub1, sizeof sub1, dir, "a");
    join_path(sub2, sizeof sub2, dir, "a/inkstorm");
    join_path(path, sizeof path, dir, "a/inkstorm/save.txt");

    MetaState meta;
    meta_init_default(&meta);
    meta.runs_played = 3;

    TEST_ASSERT_TRUE(save_write(path, &meta, NULL));
    struct stat st;
    TEST_ASSERT_EQUAL_INT(0, stat(sub2, &st));
    TEST_ASSERT_TRUE(S_ISDIR(st.st_mode));
    TEST_ASSERT_EQUAL_INT(0, stat(path, &st));

    remove(path);
    rmdir(sub2);
    rmdir(sub1);
    rmdir(dir);
}

static FILE *fail_fopen(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    errno = EIO;
    return NULL;
}

static size_t short_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    (void)ptr;
    (void)size;
    (void)stream;
    return nmemb > 1 ? nmemb - 1 : 0;
}

static int fail_fflush(FILE *stream)
{
    (void)stream;
    errno = EIO;
    return EOF;
}

static int fail_fclose(FILE *stream)
{
    (void)fclose(stream);
    errno = EIO;
    return EOF;
}

static int fail_rename(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    errno = EIO;
    return -1;
}

static void read_file_bytes(const char *path, char *buf, size_t cap, size_t *out_len)
{
    FILE *fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(fp);
    *out_len = fread(buf, 1, cap, fp);
    fclose(fp);
}

static void test_save_fault_injection_preserves_existing(void)
{
    char dir[PATH_MAX];
    TEST_ASSERT_NOT_NULL(make_temp_dir(dir, sizeof dir));

    char path[PATH_MAX + 64];
    char tmp_path[PATH_MAX + 64];
    join_path(path, sizeof path, dir, "save.txt");
    join_path(tmp_path, sizeof tmp_path, dir, "save.txt.tmp");

    MetaState orig;
    meta_init_default(&orig);
    orig.ink_crystals = 77;
    TEST_ASSERT_TRUE(save_write(path, &orig, NULL));

    char orig_bytes[1024];
    size_t orig_len = 0;
    read_file_bytes(path, orig_bytes, sizeof orig_bytes, &orig_len);

    MetaState updated = orig;
    updated.ink_crystals = 999;

    const SaveIoOps faults[5] = {
        {.fopen = fail_fopen},   {.fwrite = short_fwrite}, {.fflush = fail_fflush},
        {.fclose = fail_fclose}, {.rename = fail_rename},
    };

    for (size_t i = 0; i < 5; ++i) {
        TEST_ASSERT_FALSE(save_write(path, &updated, &faults[i]));
        struct stat st;
        TEST_ASSERT_NOT_EQUAL(0, stat(tmp_path, &st));

        char cur_bytes[1024];
        size_t cur_len = 0;
        read_file_bytes(path, cur_bytes, sizeof cur_bytes, &cur_len);
        TEST_ASSERT_EQUAL_UINT64(orig_len, cur_len);
        TEST_ASSERT_EQUAL_MEMORY(orig_bytes, cur_bytes, orig_len);
    }

    remove(path);
    rmdir(dir);
}

static void test_save_permission_denied(void)
{
    if (geteuid() == 0) {
        TEST_IGNORE_MESSAGE("Running as root; chmod 0500 does not deny write.");
    }

    char dir[PATH_MAX];
    TEST_ASSERT_NOT_NULL(make_temp_dir(dir, sizeof dir));

    char path[PATH_MAX + 64];
    join_path(path, sizeof path, dir, "save.txt");

    MetaState orig;
    meta_init_default(&orig);
    orig.runs_played = 4;
    TEST_ASSERT_TRUE(save_write(path, &orig, NULL));

    TEST_ASSERT_EQUAL_INT(0, chmod(dir, 0500));

    MetaState updated = orig;
    updated.runs_played = 99;
    TEST_ASSERT_FALSE(save_write(path, &updated, NULL));

    TEST_ASSERT_EQUAL_INT(0, chmod(dir, 0700));

    MetaState reloaded;
    TEST_ASSERT_EQUAL_INT(SAVE_LOAD_OK, save_load(path, &reloaded));
    assert_meta_equal(&orig, &reloaded);

    remove(path);
    rmdir(dir);
}

static void test_save_load_classifications(void)
{
    char dir[PATH_MAX];
    TEST_ASSERT_NOT_NULL(make_temp_dir(dir, sizeof dir));

    MetaState def;
    meta_init_default(&def);
    MetaState actual;

    char missing[PATH_MAX + 64];
    join_path(missing, sizeof missing, dir, "missing.txt");
    memset(&actual, 0xaa, sizeof actual);
    TEST_ASSERT_EQUAL_INT(SAVE_LOAD_MISSING, save_load(missing, &actual));
    assert_meta_equal(&def, &actual);

    char bad_ver[PATH_MAX + 64];
    join_path(bad_ver, sizeof bad_ver, dir, "bad_ver.txt");
    FILE *fp = fopen(bad_ver, "wb");
    TEST_ASSERT_NOT_NULL(fp);
    fputs("version=2\nink_crystals=10\n", fp);
    fclose(fp);
    memset(&actual, 0xaa, sizeof actual);
    TEST_ASSERT_EQUAL_INT(SAVE_LOAD_CORRUPT, save_load(bad_ver, &actual));
    assert_meta_equal(&def, &actual);

    char huge[PATH_MAX + 64];
    join_path(huge, sizeof huge, dir, "huge.txt");
    fp = fopen(huge, "wb");
    TEST_ASSERT_NOT_NULL(fp);
    for (int i = 0; i < 9000; ++i) {
        fputc('a', fp);
    }
    fclose(fp);
    memset(&actual, 0xaa, sizeof actual);
    TEST_ASSERT_EQUAL_INT(SAVE_LOAD_CORRUPT, save_load(huge, &actual));
    assert_meta_equal(&def, &actual);

    memset(&actual, 0xaa, sizeof actual);
    TEST_ASSERT_EQUAL_INT(SAVE_LOAD_IO_ERROR, save_load(dir, &actual));
    assert_meta_equal(&def, &actual);

    remove(bad_ver);
    remove(huge);
    rmdir(dir);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_save_resolve_path_priority);
    RUN_TEST(test_save_roundtrip);
    RUN_TEST(test_save_creates_parent_dirs);
    RUN_TEST(test_save_fault_injection_preserves_existing);
    RUN_TEST(test_save_permission_denied);
    RUN_TEST(test_save_load_classifications);
    return UNITY_END();
}
