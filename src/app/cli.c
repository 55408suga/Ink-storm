#include "cli.h"

#include <ctype.h>
#include <limits.h>
#include <string.h>

static bool parse_positive_int(const char *s, int *out_val)
{
    if (s == NULL || s[0] == '\0') {
        return false;
    }
    unsigned long long acc = 0;
    for (const char *p = s; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            return false;
        }
        acc = acc * 10u + (unsigned long long)(*p - '0');
        if (acc > (unsigned long long)INT_MAX) {
            return false;
        }
    }
    if (acc == 0) {
        return false;
    }
    *out_val = (int)acc;
    return true;
}

static bool is_valid_save_path(const char *p)
{
    if (p == NULL || p[0] == '\0') {
        return false;
    }
    if (p[0] == '-' && p[1] == '-') {
        return false;
    }
    return strlen(p) < (size_t)PATH_MAX;
}

bool cli_parse(int argc, char *const argv[], CliOptions *out)
{
    if (out == NULL || argc < 0 || (argc > 0 && argv == NULL)) {
        return false;
    }

    bool seen_headless = false;
    bool seen_smoke_frames = false;
    bool seen_no_post = false;
    bool seen_no_save = false;
    bool seen_save_path = false;
    bool seen_showcase = false;
    int smoke_frames = 0;
    const char *save_path = NULL;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (arg == NULL) {
            return false;
        }
        if (strcmp(arg, "--headless-smoke") == 0) {
            if (seen_headless) {
                return false;
            }
            seen_headless = true;
        } else if (strcmp(arg, "--smoke-frames") == 0) {
            if (seen_smoke_frames || i + 1 >= argc) {
                return false;
            }
            if (!parse_positive_int(argv[++i], &smoke_frames)) {
                return false;
            }
            seen_smoke_frames = true;
        } else if (strcmp(arg, "--no-post") == 0) {
            if (seen_no_post) {
                return false;
            }
            seen_no_post = true;
        } else if (strcmp(arg, "--no-save") == 0) {
            if (seen_no_save) {
                return false;
            }
            seen_no_save = true;
        } else if (strcmp(arg, "--save-path") == 0) {
            if (seen_save_path || i + 1 >= argc) {
                return false;
            }
            const char *path = argv[++i];
            if (!is_valid_save_path(path)) {
                return false;
            }
            save_path = path;
            seen_save_path = true;
        } else if (strcmp(arg, "--showcase") == 0) {
            if (seen_showcase) {
                return false;
            }
            seen_showcase = true;
        } else {
            return false;
        }
    }

    if (seen_headless && (seen_smoke_frames || seen_showcase || seen_save_path)) {
        return false;
    }
    if (seen_save_path && (seen_no_save || seen_smoke_frames || seen_showcase)) {
        return false;
    }

    out->mode = seen_headless   ? APP_MODE_HEADLESS
                : seen_showcase ? APP_MODE_SHOWCASE
                                : APP_MODE_WINDOW;
    out->smoke_frames = smoke_frames;
    out->use_post = !seen_no_post;
    if (seen_save_path) {
        out->save_mode = SAVE_MODE_EXPLICIT;
        out->save_path = save_path;
    } else if (seen_headless || seen_smoke_frames || seen_showcase || seen_no_save) {
        out->save_mode = SAVE_MODE_DISABLED;
        out->save_path = NULL;
    } else {
        out->save_mode = SAVE_MODE_DEFAULT;
        out->save_path = NULL;
    }
    return true;
}

void cli_print_usage(FILE *stream, const char *program)
{
    if (stream == NULL) {
        return;
    }
    const char *prog = (program != NULL && program[0] != '\0') ? program : "inkstorm";
    fprintf(stream,
            "Usage: %s [--headless-smoke | --smoke-frames N] [--showcase] [--no-post] "
            "[--no-save | --save-path PATH]\n",
            prog);
}

uint32_t cli_parse_debug_faults(const char *value, FILE *warn)
{
    if (value == NULL || value[0] == '\0') {
        return 0;
    }
    uint32_t mask = 0;
    const char *p = value;
    while (*p != '\0') {
        const char *comma = strchr(p, ',');
        const char *seg_end = comma != NULL ? comma : p + strlen(p);
        const char *start = p;
        while (start < seg_end && isspace((unsigned char)*start)) {
            ++start;
        }
        size_t len = (size_t)(seg_end - start);
        while (len > 0 && isspace((unsigned char)start[len - 1])) {
            --len;
        }
        if (len > 0) {
            if (len == 6 && memcmp(start, "shader", 6) == 0) {
                mask |= APP_FAULT_SHADER;
            } else if (len == 4 && memcmp(start, "font", 4) == 0) {
                mask |= APP_FAULT_FONT;
            } else if (warn != NULL) {
                const int print_len = len > (size_t)INT_MAX ? INT_MAX : (int)len;
                fprintf(warn, "INKSTORM: unknown debug fault '%.*s' ignored\n", print_len, start);
            }
        }
        if (comma == NULL) {
            break;
        }
        p = comma + 1;
    }
    return mask;
}
