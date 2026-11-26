#include "png_rgba.h"

#include "sprite_rules.h"

#include <stdlib.h>
#include <string.h>

enum {
    PNG_HUFF_MAX_BITS = 15,
    PNG_LIT_MAX = 288,
    PNG_DIST_MAX = 32,
    PNG_CL_MAX = 19,
    PNG_ADLER_MOD = 65521
};

typedef struct {
    const uint8_t *png;
    size_t png_size;
    size_t chunk_off;
    size_t idat_pos;
    size_t idat_end;
    bool eof;
    uint32_t bits;
    int32_t nbits;
} PngIdatReader;

typedef struct {
    uint16_t count[PNG_HUFF_MAX_BITS + 1];
    uint16_t symbol[PNG_LIT_MAX];
    int32_t total_syms;
} PngHuff;

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void idat_init(PngIdatReader *br, const uint8_t *png, size_t png_size)
{
    *br = (PngIdatReader){.png = png, .png_size = png_size, .chunk_off = 8u};
}

static bool idat_next_byte(PngIdatReader *br, uint8_t *out)
{
    while (br->idat_pos >= br->idat_end) {
        if (br->eof || br->png_size - br->chunk_off < 12u) {
            br->eof = true;
            return false;
        }
        const uint32_t len = read_be32(br->png + br->chunk_off);
        const uint8_t *type = br->png + br->chunk_off + 4u;
        const size_t data_off = br->chunk_off + 8u;
        if ((size_t)len > br->png_size - br->chunk_off - 12u) {
            br->eof = true;
            return false;
        }
        br->chunk_off += 12u + (size_t)len;
        if (memcmp(type, "IEND", 4) == 0) {
            br->eof = true;
            return false;
        }
        if (memcmp(type, "IDAT", 4) == 0 && len > 0u) {
            br->idat_pos = data_off;
            br->idat_end = data_off + (size_t)len;
        }
    }
    *out = br->png[br->idat_pos++];
    return true;
}

static bool br_get_bits(PngIdatReader *br, int32_t n, uint32_t *out)
{
    while (br->nbits < n) {
        uint8_t byte = 0u;
        if (!idat_next_byte(br, &byte)) {
            return false;
        }
        br->bits |= (uint32_t)byte << (unsigned)br->nbits;
        br->nbits += 8;
    }
    *out = n == 0 ? 0u : (br->bits & ((UINT32_C(1) << (unsigned)n) - 1u));
    br->bits >>= (unsigned)n;
    br->nbits -= n;
    return true;
}

static void br_align_byte(PngIdatReader *br)
{
    const int32_t drop = br->nbits & 7;
    br->bits >>= (unsigned)drop;
    br->nbits -= drop;
}

static bool br_read_aligned(PngIdatReader *br, uint8_t *dst, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (br->nbits >= 8) {
            dst[i] = (uint8_t)(br->bits & 0xffu);
            br->bits >>= 8u;
            br->nbits -= 8;
        } else if (!idat_next_byte(br, &dst[i])) {
            return false;
        }
    }
    return true;
}

static bool huff_build(PngHuff *t, const uint8_t *lengths, int32_t num_syms, bool allow_empty,
                       bool allow_single)
{
    memset(t->count, 0, sizeof t->count);
    for (int32_t i = 0; i < num_syms; ++i) {
        if (lengths[i] > PNG_HUFF_MAX_BITS) {
            return false;
        }
        t->count[lengths[i]]++;
    }
    int32_t used = 0;
    int32_t left = 1;
    for (int32_t len = 1; len <= PNG_HUFF_MAX_BITS; ++len) {
        used += (int32_t)t->count[len];
        left = (left << 1) - (int32_t)t->count[len];
        if (left < 0) {
            return false;
        }
    }
    if (used == 0) {
        t->total_syms = 0;
        return allow_empty;
    }
    if (left > 0 && !(allow_single && used == 1 && t->count[1] == 1u)) {
        return false;
    }
    uint16_t offs[PNG_HUFF_MAX_BITS + 1];
    offs[1] = 0u;
    for (int32_t len = 1; len < PNG_HUFF_MAX_BITS; ++len) {
        offs[len + 1] = (uint16_t)(offs[len] + t->count[len]);
    }
    for (int32_t i = 0; i < num_syms; ++i) {
        if (lengths[i] != 0u) {
            t->symbol[offs[lengths[i]]++] = (uint16_t)i;
        }
    }
    t->total_syms = used;
    return true;
}

static bool huff_decode(PngIdatReader *br, const PngHuff *t, uint32_t *sym_out)
{
    if (t->total_syms <= 0) {
        return false;
    }
    int32_t code = 0;
    int32_t first = 0;
    int32_t index = 0;
    for (int32_t len = 1; len <= PNG_HUFF_MAX_BITS; ++len) {
        uint32_t bit = 0u;
        if (!br_get_bits(br, 1, &bit)) {
            return false;
        }
        code |= (int32_t)bit;
        const int32_t cnt = (int32_t)t->count[len];
        if (code - cnt < first) {
            *sym_out = t->symbol[index + (code - first)];
            return true;
        }
        index += cnt;
        first = (first + cnt) << 1;
        code <<= 1;
    }
    return false;
}

static bool build_fixed_tables(PngHuff *lit, PngHuff *dist)
{
    uint8_t lit_lens[PNG_LIT_MAX];
    for (int32_t i = 0; i <= 143; ++i) {
        lit_lens[i] = 8u;
    }
    for (int32_t i = 144; i <= 255; ++i) {
        lit_lens[i] = 9u;
    }
    for (int32_t i = 256; i <= 279; ++i) {
        lit_lens[i] = 7u;
    }
    for (int32_t i = 280; i <= 287; ++i) {
        lit_lens[i] = 8u;
    }
    uint8_t dist_lens[PNG_DIST_MAX];
    for (int32_t i = 0; i < PNG_DIST_MAX; ++i) {
        dist_lens[i] = 5u;
    }
    return huff_build(lit, lit_lens, PNG_LIT_MAX, false, false) &&
           huff_build(dist, dist_lens, PNG_DIST_MAX, false, false);
}

static bool build_dynamic_tables(PngIdatReader *br, PngHuff *lit, PngHuff *dist)
{
    static const uint8_t k_cl_order[PNG_CL_MAX] = {16u, 17u, 18u, 0u, 8u,  7u, 9u,  6u, 10u, 5u,
                                                   11u, 4u,  12u, 3u, 13u, 2u, 14u, 1u, 15u};
    uint32_t hlit_r = 0u;
    uint32_t hdist_r = 0u;
    uint32_t hclen_r = 0u;
    if (!br_get_bits(br, 5, &hlit_r) || !br_get_bits(br, 5, &hdist_r) ||
        !br_get_bits(br, 4, &hclen_r)) {
        return false;
    }
    const int32_t hlit = (int32_t)hlit_r + 257;
    const int32_t hdist = (int32_t)hdist_r + 1;
    const int32_t hclen = (int32_t)hclen_r + 4;
    if (hlit > 286 || hdist > 30) {
        return false;
    }

    uint8_t cl_lens[PNG_CL_MAX] = {0};
    for (int32_t i = 0; i < hclen; ++i) {
        uint32_t v = 0u;
        if (!br_get_bits(br, 3, &v)) {
            return false;
        }
        cl_lens[k_cl_order[i]] = (uint8_t)v;
    }
    PngHuff cl_table;
    if (!huff_build(&cl_table, cl_lens, PNG_CL_MAX, false, true)) {
        return false;
    }

    uint8_t lengths[286 + 30];
    const int32_t total = hlit + hdist;
    int32_t idx = 0;
    while (idx < total) {
        uint32_t sym = 0u;
        if (!huff_decode(br, &cl_table, &sym)) {
            return false;
        }
        if (sym < 16u) {
            lengths[idx++] = (uint8_t)sym;
        } else {
            uint8_t fill = 0u;
            uint32_t extra = 0u;
            int32_t rep = 0;
            if (sym == 16u) {
                if (idx == 0 || !br_get_bits(br, 2, &extra)) {
                    return false;
                }
                fill = lengths[idx - 1];
                rep = 3 + (int32_t)extra;
            } else if (sym == 17u) {
                if (!br_get_bits(br, 3, &extra)) {
                    return false;
                }
                rep = 3 + (int32_t)extra;
            } else if (sym == 18u) {
                if (!br_get_bits(br, 7, &extra)) {
                    return false;
                }
                rep = 11 + (int32_t)extra;
            } else {
                return false;
            }
            if (idx + rep > total) {
                return false;
            }
            for (int32_t k = 0; k < rep; ++k) {
                lengths[idx++] = fill;
            }
        }
    }
    if (lengths[256] == 0u) {
        return false;
    }
    return huff_build(lit, lengths, hlit, false, false) &&
           huff_build(dist, lengths + hlit, hdist, true, true);
}

static bool inflate_huffman_block(PngIdatReader *br, const PngHuff *lit, const PngHuff *dist,
                                  uint8_t *out, size_t expect_raw, size_t *out_pos)
{
    static const uint16_t k_len_base[29] = {3u,  4u,  5u,  6u,   7u,   8u,   9u,   10u,  11u, 13u,
                                            15u, 17u, 19u, 23u,  27u,  31u,  35u,  43u,  51u, 59u,
                                            67u, 83u, 99u, 115u, 131u, 163u, 195u, 227u, 258u};
    static const uint8_t k_len_extra[29] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u,
                                            1u, 1u, 2u, 2u, 2u, 2u, 3u, 3u, 3u, 3u,
                                            4u, 4u, 4u, 4u, 5u, 5u, 5u, 5u, 0u};
    static const uint16_t k_dist_base[30] = {
        1u,    2u,    3u,    4u,    5u,    7u,    9u,    13u,    17u,    25u,
        33u,   49u,   65u,   97u,   129u,  193u,  257u,  385u,   513u,   769u,
        1025u, 1537u, 2049u, 3073u, 4097u, 6145u, 8193u, 12289u, 16385u, 24577u};
    static const uint8_t k_dist_extra[30] = {0u, 0u, 0u,  0u,  1u,  1u,  2u,  2u,  3u,  3u,
                                             4u, 4u, 5u,  5u,  6u,  6u,  7u,  7u,  8u,  8u,
                                             9u, 9u, 10u, 10u, 11u, 11u, 12u, 12u, 13u, 13u};

    for (;;) {
        uint32_t sym = 0u;
        if (!huff_decode(br, lit, &sym)) {
            return false;
        }
        if (sym < 256u) {
            if (*out_pos >= expect_raw) {
                return false;
            }
            out[(*out_pos)++] = (uint8_t)sym;
        } else if (sym == 256u) {
            return true;
        } else if (sym <= 285u) {
            const uint32_t li = sym - 257u;
            uint32_t extra = 0u;
            if (!br_get_bits(br, (int32_t)k_len_extra[li], &extra)) {
                return false;
            }
            const size_t match_len = (size_t)k_len_base[li] + (size_t)extra;
            uint32_t dsym = 0u;
            if (!huff_decode(br, dist, &dsym) || dsym >= 30u) {
                return false;
            }
            uint32_t dextra = 0u;
            if (!br_get_bits(br, (int32_t)k_dist_extra[dsym], &dextra)) {
                return false;
            }
            const size_t distance = (size_t)k_dist_base[dsym] + (size_t)dextra;
            if (distance == 0u || distance > *out_pos || match_len > expect_raw - *out_pos) {
                return false;
            }
            for (size_t k = 0; k < match_len; ++k) {
                out[*out_pos] = out[*out_pos - distance];
                (*out_pos)++;
            }
        } else {
            return false;
        }
    }
}

static uint32_t adler32_calc(const uint8_t *data, size_t len)
{
    uint32_t s1 = 1u;
    uint32_t s2 = 0u;
    for (size_t i = 0; i < len; ++i) {
        s1 = (s1 + (uint32_t)data[i]) % PNG_ADLER_MOD;
        s2 = (s2 + s1) % PNG_ADLER_MOD;
    }
    return (s2 << 16) | s1;
}

static uint8_t paeth_pred(uint8_t a, uint8_t b, uint8_t c)
{
    const int32_t p = (int32_t)a + (int32_t)b - (int32_t)c;
    const int32_t pa = abs(p - (int32_t)a);
    const int32_t pb = abs(p - (int32_t)b);
    const int32_t pc = abs(p - (int32_t)c);
    if (pa <= pb && pa <= pc) {
        return a;
    }
    return pb <= pc ? b : c;
}

static bool unfilter_rgba8(const uint8_t *raw, int32_t w, int32_t h, uint8_t *rgba_out)
{
    const size_t row_bytes = (size_t)w * 4u;
    const size_t stride = row_bytes + 1u;
    for (int32_t y = 0; y < h; ++y) {
        const uint8_t ftype = raw[(size_t)y * stride];
        const uint8_t *src = raw + (size_t)y * stride + 1u;
        uint8_t *dst = rgba_out + (size_t)y * row_bytes;
        const uint8_t *prev = y > 0 ? (rgba_out + (size_t)(y - 1) * row_bytes) : NULL;
        switch (ftype) {
        case 0u:
            memcpy(dst, src, row_bytes);
            break;
        case 1u:
            for (size_t i = 0; i < row_bytes; ++i) {
                const uint8_t a = i >= 4u ? dst[i - 4u] : 0u;
                dst[i] = (uint8_t)(src[i] + a);
            }
            break;
        case 2u:
            for (size_t i = 0; i < row_bytes; ++i) {
                const uint8_t b = prev != NULL ? prev[i] : 0u;
                dst[i] = (uint8_t)(src[i] + b);
            }
            break;
        case 3u:
            for (size_t i = 0; i < row_bytes; ++i) {
                const uint32_t a = i >= 4u ? dst[i - 4u] : 0u;
                const uint32_t b = prev != NULL ? prev[i] : 0u;
                dst[i] = (uint8_t)(src[i] + ((a + b) >> 1));
            }
            break;
        case 4u:
            for (size_t i = 0; i < row_bytes; ++i) {
                const uint8_t a = i >= 4u ? dst[i - 4u] : 0u;
                const uint8_t b = prev != NULL ? prev[i] : 0u;
                const uint8_t c = (i >= 4u && prev != NULL) ? prev[i - 4u] : 0u;
                dst[i] = (uint8_t)(src[i] + paeth_pred(a, b, c));
            }
            break;
        default:
            return false;
        }
    }
    return true;
}

bool png_rgba_decode(const uint8_t *png, size_t png_size, int32_t expect_w, int32_t expect_h,
                     uint8_t *rgba_out, size_t rgba_cap, uint8_t *raw_scratch, size_t raw_cap)
{
    if (png == NULL || rgba_out == NULL || raw_scratch == NULL || expect_w <= 0 || expect_h <= 0 ||
        expect_w > SPRITE_CANVAS_MAX || expect_h > SPRITE_CANVAS_MAX ||
        !sprite_png_validate(png, png_size, expect_w, expect_h)) {
        return false;
    }
    const size_t rgba_need = (size_t)expect_w * (size_t)expect_h * 4u;
    const size_t raw_need = ((size_t)expect_w * 4u + 1u) * (size_t)expect_h;
    if (rgba_cap < rgba_need || raw_cap < raw_need) {
        return false;
    }

    PngIdatReader br;
    idat_init(&br, png, png_size);
    uint8_t hdr[2];
    if (!br_read_aligned(&br, hdr, 2u)) {
        return false;
    }
    const uint32_t cmf = hdr[0];
    const uint32_t flg = hdr[1];
    if ((cmf & 0x0fu) != 8u || (cmf >> 4) > 7u || (((cmf << 8) | flg) % 31u) != 0u ||
        (flg & 0x20u) != 0u) {
        return false;
    }

    size_t out_pos = 0u;
    uint32_t bfinal = 0u;
    do {
        uint32_t btype = 0u;
        if (!br_get_bits(&br, 1, &bfinal) || !br_get_bits(&br, 2, &btype)) {
            return false;
        }
        if (btype == 0u) {
            br_align_byte(&br);
            uint8_t len_hdr[4];
            if (!br_read_aligned(&br, len_hdr, 4u)) {
                return false;
            }
            const uint16_t len = (uint16_t)((uint16_t)len_hdr[0] | ((uint16_t)len_hdr[1] << 8));
            const uint16_t nlen = (uint16_t)((uint16_t)len_hdr[2] | ((uint16_t)len_hdr[3] << 8));
            if ((uint16_t)(~len) != nlen || (size_t)len > raw_need - out_pos ||
                !br_read_aligned(&br, raw_scratch + out_pos, (size_t)len)) {
                return false;
            }
            out_pos += (size_t)len;
        } else if (btype == 1u || btype == 2u) {
            PngHuff lit;
            PngHuff dist;
            const bool ok = btype == 1u ? build_fixed_tables(&lit, &dist)
                                        : build_dynamic_tables(&br, &lit, &dist);
            if (!ok || !inflate_huffman_block(&br, &lit, &dist, raw_scratch, raw_need, &out_pos)) {
                return false;
            }
        } else {
            return false;
        }
    } while (bfinal == 0u);

    if (out_pos != raw_need) {
        return false;
    }
    br_align_byte(&br);
    uint8_t adler_bytes[4];
    if (!br_read_aligned(&br, adler_bytes, 4u) ||
        read_be32(adler_bytes) != adler32_calc(raw_scratch, raw_need)) {
        return false;
    }
    uint8_t extra = 0u;
    if (br_read_aligned(&br, &extra, 1u)) {
        return false;
    }
    return unfilter_rgba8(raw_scratch, expect_w, expect_h, rgba_out);
}
