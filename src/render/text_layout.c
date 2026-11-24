#include "text_layout.h"

#include <string.h>

size_t text_utf8_next(const char *s, size_t i, size_t n)
{
    if (i >= n) {
        return n;
    }
    size_t j = i + 1;
    while (j < n && ((unsigned char)s[j] & 0xC0u) == 0x80u) {
        ++j;
    }
    return j;
}

size_t text_utf8_clip(const char *s, size_t len, size_t max_bytes)
{
    if (s == NULL || len <= max_bytes) {
        return s == NULL ? 0 : len;
    }
    size_t n = max_bytes;
    while (n > 0 && ((unsigned char)s[n] & 0xC0u) == 0x80u) {
        --n;
    }
    return n;
}

static uint32_t decode_at(const char *s, size_t i, size_t n)
{
    const unsigned char c0 = (unsigned char)s[i];
    if (c0 < 0x80u) {
        return c0;
    }
    const size_t end = text_utf8_next(s, i, n);
    const size_t len = end - i;
    uint32_t cp = 0;
    if (len == 2) {
        cp = c0 & 0x1Fu;
    } else if (len == 3) {
        cp = c0 & 0x0Fu;
    } else {
        cp = c0 & 0x07u;
    }
    for (size_t k = i + 1; k < end; ++k) {
        cp = (cp << 6) | ((unsigned char)s[k] & 0x3Fu);
    }
    return cp;
}

/* Japanese line-breaking (kinsoku): closing marks may not start a line, opening marks may not
 * end one. */
static bool no_line_start(uint32_t cp)
{
    static const uint32_t k_closers[] = {
        0x3001, 0x3002, 0xFF0C, 0xFF0E, 0xFF09, 0x300D, 0x300F, 0xFF01, 0xFF1F, 0x30FC, 0x30FB,
        0x2026, 0xFF1A, 0xFF1B, ')',    ',',    '.',    '!',    '?',    ':',    ';',    '/'};
    for (size_t k = 0; k < sizeof k_closers / sizeof k_closers[0]; ++k) {
        if (k_closers[k] == cp) {
            return true;
        }
    }
    return false;
}

static bool no_line_end(uint32_t cp)
{
    return cp == 0x300C || cp == 0x300E || cp == 0xFF08 || cp == '(';
}

static bool can_break_between(uint32_t prev, uint32_t next)
{
    if (no_line_start(next) || no_line_end(prev)) {
        return false;
    }
    return prev == ' ' || next == ' ' || prev >= 0x80u || next >= 0x80u;
}

/* Width grows monotonically with the prefix, so a binary search over codepoint boundaries keeps
 * this at O(log n) measurements; these helpers run for many labels on every frame. */
size_t text_layout_fit_prefix(const char *text, size_t len, float max_w, TextMeasureFn measure,
                              void *ctx)
{
    if (text == NULL || measure == NULL || len == 0) {
        return 0;
    }
    if (measure(ctx, text, len) <= max_w) {
        return len;
    }
    enum { MAX_BOUNDS = 512 };
    size_t bounds[MAX_BOUNDS];
    int32_t n = 0;
    for (size_t i = text_utf8_next(text, 0, len); i < len && n < MAX_BOUNDS;
         i = text_utf8_next(text, i, len)) {
        bounds[n++] = i;
    }
    int32_t lo = 0;
    int32_t hi = n - 1;
    size_t best = 0;
    while (lo <= hi) {
        const int32_t mid = lo + (hi - lo) / 2;
        if (measure(ctx, text, bounds[mid]) <= max_w) {
            best = bounds[mid];
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return best;
}

static size_t skip_spaces(const char *text, size_t i, size_t n)
{
    while (i < n && text[i] == ' ') {
        ++i;
    }
    return i;
}

int32_t text_layout_wrap(const char *text, float max_w, TextMeasureFn measure, void *ctx,
                         TextLine *lines, int32_t max_lines, bool *truncated)
{
    if (truncated != NULL) {
        *truncated = false;
    }
    if (text == NULL || measure == NULL || lines == NULL || max_lines <= 0) {
        return 0;
    }
    const size_t n = strlen(text);
    int32_t count = 0;
    size_t start = skip_spaces(text, 0, n);
    while (start < n) {
        if (count >= max_lines) {
            if (truncated != NULL) {
                *truncated = true;
            }
            break;
        }
        const char *nl = memchr(text + start, '\n', n - start);
        const size_t para_end = nl != NULL ? (size_t)(nl - text) : n;
        const size_t avail = para_end - start;
        size_t take = text_layout_fit_prefix(text + start, avail, max_w, measure, ctx);
        if (take < avail) {
            const char *seg = text + start;
            size_t brk = 0;
            size_t prev = 0;
            size_t i = text_utf8_next(seg, 0, avail);
            while (i <= take && i < avail) {
                if (can_break_between(decode_at(seg, prev, avail), decode_at(seg, i, avail))) {
                    brk = i;
                }
                prev = i;
                i = text_utf8_next(seg, i, avail);
            }
            if (brk > 0) {
                take = brk;
            } else if (take == 0) {
                take = text_utf8_next(text + start, 0, avail);
            }
        }
        size_t line_len = take;
        while (line_len > 0 && text[start + line_len - 1] == ' ') {
            --line_len;
        }
        lines[count++] = (TextLine){.start = start, .len = line_len};
        size_t next = start + take;
        if (next == para_end && nl != NULL) {
            ++next;
        }
        start = skip_spaces(text, next, n);
    }
    return count;
}
