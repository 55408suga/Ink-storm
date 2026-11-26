#ifndef INK_APP_CLI_H
#define INK_APP_CLI_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef enum { APP_MODE_WINDOW = 0, APP_MODE_HEADLESS, APP_MODE_SHOWCASE, APP_MODE_DEMO } AppMode;

typedef enum { SAVE_MODE_DEFAULT = 0, SAVE_MODE_DISABLED, SAVE_MODE_EXPLICIT } SaveMode;

#define APP_FAULT_SHADER (UINT32_C(1) << 0)
#define APP_FAULT_FONT (UINT32_C(1) << 1)
#define APP_FAULT_SPRITES (UINT32_C(1) << 2)

typedef struct {
    AppMode mode;
    int smoke_frames;
    bool use_post;
    SaveMode save_mode;
    const char *save_path;
    const char *snap_dir; /* --snap DIR 指定時のみ非 NULL */
} CliOptions;

bool cli_parse(int argc, char *const argv[], CliOptions *out);
void cli_print_usage(FILE *stream, const char *program);
uint32_t cli_parse_debug_faults(const char *value, FILE *warn);

#endif
