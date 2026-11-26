#ifndef INK_RENDER_PNG_RGBA_H
#define INK_RENDER_PNG_RGBA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 固定出力上限つきの非インターレース RGBA8 PNG 展開器（内部で動的確保しない）。
 * 展開結果が (4 * expect_w + 1) * expect_h バイトと一致しない入力や壊れたストリームは即座に弾く
 */
bool png_rgba_decode(const uint8_t *png, size_t png_size, int32_t expect_w, int32_t expect_h,
                     uint8_t *rgba_out, size_t rgba_cap, uint8_t *raw_scratch, size_t raw_cap);

#endif
