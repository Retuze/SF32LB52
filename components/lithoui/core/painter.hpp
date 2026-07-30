#pragma once
#include "tile.hpp"
#include "litho_core.h"
#include "res_images.h"
#include <stdio.h>
#include <string.h>
#include "hal.h"  // dwt_cycles()

// Image formats — defined in res_images.h:
//   FMT_A8_RLE = 0        grayscale RLE, tint coloring, opaque
//   FMT_PAL_RLE = 1        palette RLE, RGB565 palette, opaque
//   FMT_PAL_ALPHA_RLE = 2  palette RLE, alpha inline in RLE stream
//   FMT_RGB565_RLE = 3     direct color RLE, opaque
//   FMT_RGB565A_RLE = 4    direct color RLE, alpha inline in RLE stream
//
// The `fmt` parameter carries formatInfo: bits 2:0 = format enum, bits 7:3 = paletteBits.
// Use LITHO_FORMAT(fmt) and LITHO_PALETTE_BITS(fmt) macros from res_images.h.
//
// Alpha-format RLE head byte (formats 2 & 4) — variable-length encoding:
//   bit7=1 (opaque, α=255):
//     [1|LLLLLLL]  7-bit run length: (head & 0x7F) + 1 = 1..128 pixels
//     Followed by color data (palette index or RGB565)
//   bit7=0 (non-opaque):
//     [0|TT|LLLLL]  TT=bits6:5 (alpha level), LLLLL=bits4:0 run length = (head & 0x1F) + 1 = 1..32
//     TT=00 → α=0   (fully transparent, 1-byte record, no color data)
//     TT=01 → α=85  (semi-transparent, head + color)
//     TT=10 → α=170 (semi-transparent, head + color)
//     TT=11 → α=213 (semi-transparent, head + color; NOT 255 — 255 uses bit7=1)

// Alpha values for non-opaque TT field (indexed directly by TT = bits 6:5 of head).
static const uint8_t kTTAlpha[4] = {0, 85, 170, 213};

static inline size_t lithoStrlen(const char* s) {
    size_t n = 0;
    if (!s) return 0;
    while (s[n]) ++n;
    return n;
}

// ── Shared inline helpers ─────────────────────────────────────────

// Alpha-blend a source RGB565 pixel onto a destination pixel.
// alpha: 0 = fully transparent (dst unchanged), 255 = fully opaque (src replaces dst)
static inline uint16_t blend565(uint16_t src, uint16_t dst, uint32_t alpha) {
    uint32_t ia = 255 - alpha;
    uint16_t r = (uint16_t)((((src >> 11) & 0x1F) * alpha + ((dst >> 11) & 0x1F) * ia) / 255) << 11;
    uint16_t g = (uint16_t)((((src >> 5)  & 0x3F) * alpha + ((dst >> 5)  & 0x3F) * ia) / 255) << 5;
    uint16_t b = (uint16_t)((( src        & 0x1F) * alpha + ( dst        & 0x1F) * ia) / 255);
    return r | g | b;
}

// Map destination pixel → source Q8 coordinate for bilinear (half-pixel centered).
static inline void scaleMapQ8(int local, int dstSize, int srcSize, int& i0, int& i1, int& frac) {
    if (dstSize <= 0 || srcSize <= 0) {
        i0 = i1 = 0;
        frac = 0;
        return;
    }
    int32_t u = ((int32_t)(local * 2 + 1) * srcSize * 128) / dstSize - 128;
    if (u < 0) u = 0;
    int32_t maxU = ((int32_t)srcSize - 1) << 8;
    if (u > maxU) u = maxU;
    i0 = (int)(u >> 8);
    frac = (int)(u & 0xFF);
    i1 = i0 + 1;
    if (i1 >= srcSize) i1 = srcSize - 1;
}

// Bilinear RGB565 + alpha (weights in 0..255). Premultiplied for soft edges.
static inline void bilinear565(uint16_t c00, uint32_t a00,
                               uint16_t c10, uint32_t a10,
                               uint16_t c01, uint32_t a01,
                               uint16_t c11, uint32_t a11,
                               int fx, int fy,
                               uint16_t& outC, uint32_t& outA) {
    const uint32_t w00 = (uint32_t)(255 - fx) * (uint32_t)(255 - fy);
    const uint32_t w10 = (uint32_t)fx * (uint32_t)(255 - fy);
    const uint32_t w01 = (uint32_t)(255 - fx) * (uint32_t)fy;
    const uint32_t w11 = (uint32_t)fx * (uint32_t)fy;
    const uint32_t A = a00 * w00 + a10 * w10 + a01 * w01 + a11 * w11;
    if (!A) {
        outC = 0;
        outA = 0;
        return;
    }
    outA = A / (255u * 255u);
    if (outA > 255) outA = 255;
    uint32_t r = ((c00 >> 11) & 0x1F) * a00 * w00 + ((c10 >> 11) & 0x1F) * a10 * w10
               + ((c01 >> 11) & 0x1F) * a01 * w01 + ((c11 >> 11) & 0x1F) * a11 * w11;
    uint32_t g = ((c00 >> 5)  & 0x3F) * a00 * w00 + ((c10 >> 5)  & 0x3F) * a10 * w10
               + ((c01 >> 5)  & 0x3F) * a01 * w01 + ((c11 >> 5)  & 0x3F) * a11 * w11;
    uint32_t b = ( c00        & 0x1F) * a00 * w00 + ( c10        & 0x1F) * a10 * w10
               + ( c01        & 0x1F) * a01 * w01 + ( c11        & 0x1F) * a11 * w11;
    r /= A;
    g /= A;
    b /= A;
    if (r > 0x1F) r = 0x1F;
    if (g > 0x3F) g = 0x3F;
    if (b > 0x1F) b = 0x1F;
    outC = (uint16_t)((r << 11) | (g << 5) | b);
}

// Fast 32-bit word fill of a uint16_t pixel buffer.
// Aligns to 4-byte boundary if needed, then writes pairs of pixels as uint32_t.
// Updates dp and cnt in-place (may advance by 1 for alignment).
static inline void wordFill32(uint16_t*& dp, int& cnt, uint16_t color) {
    if (cnt <= 0) return;
    if ((uintptr_t)dp & 3) { *dp++ = color; --cnt; }
    uint32_t c32 = ((uint32_t)color << 16) | color;
    uint32_t* d4 = (uint32_t*)dp;
    int wc = cnt >> 1;
    for (int i = 0; i < wc; i++) d4[i] = c32;
    if (cnt & 1) dp[cnt - 1] = color;
}

namespace litho {

class Painter {
public:
    void setTile(Tile& tile, int tileOrgX, int tileOrgY) {
        mTile     = &tile;
        mTileOrgX = tileOrgX;
        mTileOrgY = tileOrgY;
        mClipL = -32768; mClipT = -32768;
        mClipR =  32767; mClipB =  32767;
    }

    uint16_t* tileBuf()    const { return mTile ? mTile->buffer() : nullptr; }
    int       tileStride() const { return mTile ? mTile->stride() : 0; }

    void setScreenOrigin(int sx, int sy) {
        mScreenX = sx;
        mScreenY = sy;
        mOriginXFP = (int64_t)sx << 16;
        mOriginYFP = (int64_t)sy << 16;
    }
    // 16.16 origin — accumulate child offsets without intermediate rounding.
    void setScreenOriginFP(int64_t xFP, int64_t yFP) {
        mOriginXFP = xFP;
        mOriginYFP = yFP;
        mScreenX = (int)((xFP + 32768) >> 16);
        mScreenY = (int)((yFP + 32768) >> 16);
    }
    int  screenX() const { return mScreenX; }
    int  screenY() const { return mScreenY; }
    int64_t originXFP() const { return mOriginXFP; }
    int64_t originYFP() const { return mOriginYFP; }

    // Fixed-point: 65536 = 1.0 (16.16, matches View::kScaleOne)
    static constexpr uint32_t kScaleOne = 65536u;
    void setScale(uint32_t s) { mScale = s ? s : 1; }
    uint32_t scale() const { return mScale; }
    static inline int applyScale(int x, uint32_t s) {
        return (int)(((int64_t)x * (int64_t)s + 32768) >> 16);
    }
    static inline int roundFP(int64_t v) {
        return (int)((v + 32768) >> 16);
    }

    void setAlpha(uint8_t a) { mAlpha = a; }
    uint8_t alpha() const { return mAlpha; }

    bool intersectsClip(int left, int top, int right, int bottom) const {
        return left < mClipR && right > mClipL &&
               top  < mClipB && bottom > mClipT;
    }

    void setScreenClip(int left, int top, int right, int bottom) {
        if (left   > mClipL) mClipL = left;
        if (top    > mClipT) mClipT = top;
        if (right  < mClipR) mClipR = right;
        if (bottom < mClipB) mClipB = bottom;
    }

    void setTileIdx(uint8_t ti) { mTileIdx = ti; }
    uint8_t tileIdx() const     { return mTileIdx; }

    __attribute__((noinline, section(".ramfunc")))
    void fillRect(int x, int y, int w, int h, RGB565 c) {
        if (!mTile || !mTile->buffer() || w <= 0 || h <= 0) return;

        // Logical → screen (scale about painter origin)
        int sx0, sy0, sx1, sy1;
        if (mScale == kScaleOne) {
            sx0 = x + mScreenX;
            sy0 = y + mScreenY;
            sx1 = sx0 + w;
            sy1 = sy0 + h;
        } else {
            // From FP origin so siblings share unrounded parent position.
            sx0 = roundFP(mOriginXFP + (int64_t)x * mScale);
            sy0 = roundFP(mOriginYFP + (int64_t)y * mScale);
            sx1 = roundFP(mOriginXFP + (int64_t)(x + w) * mScale);
            sy1 = roundFP(mOriginYFP + (int64_t)(y + h) * mScale);
            if (sx1 <= sx0) sx1 = sx0 + 1;
            if (sy1 <= sy0) sy1 = sy0 + 1;
        }

        if (sx0 < mClipL) sx0 = mClipL;
        if (sy0 < mClipT) sy0 = mClipT;
        if (sx1 > mClipR) sx1 = mClipR;
        if (sy1 > mClipB) sy1 = mClipB;
        if (sx0 >= sx1 || sy0 >= sy1) return;

        int tx0 = sx0 - mTileOrgX;
        int ty0 = sy0 - mTileOrgY;
        int tx1 = sx1 - mTileOrgX;
        int ty1 = sy1 - mTileOrgY;

        if (tx0 < 0) tx0 = 0;
        if (ty0 < 0) ty0 = 0;
        if (tx1 > mTile->width())  tx1 = mTile->width();
        if (ty1 > mTile->height()) ty1 = mTile->height();
        if (tx0 >= tx1 || ty0 >= ty1) return;

        uint16_t* row = mTile->buffer() + ty0 * mTile->stride();

        if (mAlpha == 255) {
            uint32_t c32 = ((uint32_t)c.value << 16) | c.value;
            int count = tx1 - tx0;
            for (int ty = ty0; ty < ty1; ty++) {
                uint32_t* row32 = (uint32_t*)(row + tx0);
                int wc = count / 2;
                for (int i = 0; i < wc; i++) row32[i] = c32;
                if (count & 1) row[tx0 + wc * 2] = c.value;
                row += mTile->stride();
            }
        } else {
            uint32_t a  = mAlpha;
            uint32_t ia = 255 - a;
            uint32_t sr = ((c.value >> 11) & 0x1F) * a;
            uint32_t sg = ((c.value >> 5)  & 0x3F) * a;
            uint32_t sb = ( c.value        & 0x1F) * a;

            for (int ty = ty0; ty < ty1; ty++) {
                for (int tx = tx0; tx < tx1; tx++) {
                    uint16_t d = row[tx];
                    uint16_t r = (uint16_t)((sr + ((d >> 11) & 0x1F) * ia) / 255) << 11;
                    uint16_t g = (uint16_t)((sg + ((d >> 5)  & 0x3F) * ia) / 255) << 5;
                    uint16_t b = (uint16_t)((sb + ( d        & 0x1F) * ia) / 255);
                    row[tx] = r | g | b;
                }
                row += mTile->stride();
            }
        }
    }

    // ── drawImage (straight copy, no rotation) ────────────────────

    __attribute__((noinline, section(".ramfunc")))
    void drawImage(const void* src, int fmt,
                   int srcW, int srcH, int dx, int dy,
                   const RGB565* tint = nullptr) {
        if (mScale != kScaleOne) {
            drawImageScaled(src, fmt, srcW, srcH, dx, dy, tint);
            return;
        }
        int imageFormat = LITHO_FORMAT(fmt);
        int paletteSize = LITHO_PALETTE_SIZE(fmt);

        int sx0 = dx + mScreenX;
        int sy0 = dy + mScreenY;
        int sx1 = sx0 + srcW;
        int sy1 = sy0 + srcH;

        if (sx0 < mClipL) sx0 = mClipL;
        if (sy0 < mClipT) sy0 = mClipT;
        if (sx1 > mClipR) sx1 = mClipR;
        if (sy1 > mClipB) sy1 = mClipB;
        if (sx0 >= sx1 || sy0 >= sy1) return;

        int tx0 = sx0 - mTileOrgX;
        int ty0 = sy0 - mTileOrgY;
        int copyW = sx1 - sx0;
        int copyH = sy1 - sy0;

        if (tx0 < 0) { copyW += tx0; tx0 = 0; }
        if (ty0 < 0) { copyH += ty0; ty0 = 0; }
        if (tx0 + copyW > mTile->width())  copyW = mTile->width()  - tx0;
        if (ty0 + copyH > mTile->height()) copyH = mTile->height() - ty0;
        if (copyW <= 0 || copyH <= 0) return;

        int srcOffX = sx0 - (dx + mScreenX);
        int srcOffY = sy0 - (dy + mScreenY);

        // ── FMT_A8_RLE (0): grayscale + RLE ─────────────────────
        if (imageFormat == 0) {
                        const uint8_t* rle = (const uint8_t*)src;
            const uint32_t* off = (const uint32_t*)rle;
            uint16_t* tile = mTile->buffer();
            int tStride = mTile->stride();
            const int visL = srcOffX, visR = srcOffX + copyW;
            uint16_t tintLut[256];
            if (tint) {
                uint32_t tr = (tint->value >> 11) & 0x1F, tg = (tint->value >> 5) & 0x3F, tb = tint->value & 0x1F;
                for (int i = 0; i < 256; i++) {
                    uint32_t r = (tr * i) / 255, g = (tg * i) / 255, b = (tb * i) / 255;
                    if (r > 0x1F) r = 0x1F; if (g > 0x3F) g = 0x3F; if (b > 0x1F) b = 0x1F;
                    tintLut[i] = (uint16_t)((r << 11) | (g << 5) | b);
                }
            }
            for (int y = 0; y < copyH; y++) {
                uint32_t rowOff = off[srcOffY + y];
                uint16_t* dstRow = tile + (ty0 + y) * tStride + tx0;
                if (rowOff & 0x80000000) {
                    // Raw grayscale row: 1B/px
                    const uint8_t* srow = rle + (rowOff & 0x7FFFFFFF);
                    for (int x = 0; x < copyW; x++) {
                        uint8_t g = srow[srcOffX + x];
                        uint16_t c = tint ? tintLut[g]
                            : (uint16_t)(((g >> 3) & 0x1F) << 11 | ((g >> 2) & 0x3F) << 5 | ((g >> 3) & 0x1F));
                        dstRow[x] = (mAlpha == 255) ? c : blend565(c, dstRow[x], mAlpha);
                    }
                } else {
                    const uint8_t* p = rle + rowOff;
                    int px = 0;
                    int loopCnt = 0;
                    while (px < srcW) {
                        if (++loopCnt > srcW * 2) {
                            printf("[drawImage] A8_RLE: loop overflow y=%d px=%d srcW=%d\r\n", y, px, srcW);
                            break;
                        }
                        uint8_t g = *p++;
                        uint8_t len = *p++;
                        int n = (int)len + 1;
                        int runR = px + n;
                        int cl = px < visL ? visL : px, cr = runR > visR ? visR : runR;
                        if (cr > cl) {
                            uint16_t* dp = dstRow + (cl - visL);
                            int cnt = cr - cl;
                            uint16_t c = tint ? tintLut[g]
                                : (uint16_t)(((g >> 3) & 0x1F) << 11 | ((g >> 2) & 0x3F) << 5 | ((g >> 3) & 0x1F));
                            if (mAlpha == 255) {
                                wordFill32(dp, cnt, c);
                            } else {
                                for (int i = 0; i < cnt; i++)
                                    dp[i] = blend565(c, dp[i], mAlpha);
                            }
                        }
                        px = runR;
                    }
                }
            }
            return;
        }

        // ── FMT_PAL_RLE (1): palette + RLE, opaque ───────────────
        if (imageFormat == 1) {
                        const uint16_t* pal = (const uint16_t*)src;
            const uint8_t*  rle = (const uint8_t*)src + paletteSize * 2;
            const uint32_t* off = (const uint32_t*)rle;
            uint16_t* tile = mTile->buffer();
            int tStride = mTile->stride();
            const int visL = srcOffX, visR = srcOffX + copyW;
            for (int y = 0; y < copyH; y++) {
                uint32_t rowOff = off[srcOffY + y];
                uint16_t* dstRow = tile + (ty0 + y) * tStride + tx0;
                if (rowOff & 0x80000000) {
                    // Raw palette index row: 1B/px
                    const uint8_t* srow = rle + (rowOff & 0x7FFFFFFF);
                    if (mAlpha == 255) {
                        for (int x = 0; x < copyW; x++)
                            dstRow[x] = pal[srow[srcOffX + x]];
                    } else {
                        for (int x = 0; x < copyW; x++)
                            dstRow[x] = blend565(pal[srow[srcOffX + x]], dstRow[x], mAlpha);
                    }
                } else {
                    const uint8_t* p = rle + rowOff;
                    int px = 0;
                    int loopCnt = 0;
                    while (px < srcW) {
                        if (++loopCnt > srcW * 2) {
                            printf("[drawImage] PAL_RLE: loop overflow y=%d px=%d srcW=%d\r\n", y, px, srcW);
                            break;
                        }
                        uint8_t ix = *p++; uint8_t len = *p++;
                        int n = (int)len + 1;
                        int runR = px + n, cl = px < visL ? visL : px, cr = runR > visR ? visR : runR;
                        if (cr > cl) {
                            uint16_t c  = pal[ix];
                            uint16_t* dp = dstRow + (cl - visL); int cnt = cr - cl;
                            if (mAlpha == 255) {
                                wordFill32(dp, cnt, c);
                            } else {
                                for (int i = 0; i < cnt; i++)
                                    dp[i] = blend565(c, dp[i], mAlpha);
                            }
                        }
                        px = runR;
                    }
                }
            }
            return;
        }

        // ── FMT_PAL_ALPHA_RLE (2): palette + RLE, alpha inline (variable-length head) ───
        if (imageFormat == 2) {
            const uint16_t* pal = (const uint16_t*)src;
            const uint8_t*  rle = (const uint8_t*)src + paletteSize * 2;
            const uint32_t* off = (const uint32_t*)rle;
            uint16_t* tile = mTile->buffer();
            int tStride = mTile->stride();
            const int visL = srcOffX, visR = srcOffX + copyW;
            for (int y = 0; y < copyH; y++) {
                const uint8_t* p = rle + off[srcOffY + y];
                uint16_t* dstRow = tile + (ty0 + y) * tStride + tx0;
                int px = 0;
                int loopCnt = 0;
                while (px < srcW) {
                    if (++loopCnt > srcW * 2) {
                        printf("[drawImage] PAL_ALPHA_RLE: loop overflow y=%d px=%d srcW=%d\r\n", y, px, srcW);
                        break;
                    }
                    uint8_t head = *p++;
                    if (head & 0x80) {
                        // ── Opaque: α=255, 7-bit run length (1..128) ──
                        int n = (head & 0x7F) + 1;
                        uint8_t ix = *p++;
                        int runR = px + n;
                        int cl = px < visL ? visL : px;
                        int cr = runR > visR ? visR : runR;
                        if (cr > cl) {
                            uint16_t c  = pal[ix];
                            uint16_t* dp = dstRow + (cl - visL); int cnt = cr - cl;
                            if (mAlpha == 255) {
                                wordFill32(dp, cnt, c);
                            } else {
                                for (int i = 0; i < cnt; i++)
                                    dp[i] = blend565(c, dp[i], mAlpha);
                            }
                        }
                        px = runR;
                    } else {
                        // ── Non-opaque: TT + 5-bit run length (1..32) ──
                        uint8_t tt = (head >> 5) & 0x03;
                        int n = (head & 0x1F) + 1;
                        int runR = px + n;
                        if (tt == 0) {
                            // Fully transparent: no color data, skip
                        } else {
                            uint8_t ix = *p++;
                            int cl = px < visL ? visL : px;
                            int cr = runR > visR ? visR : runR;
                            if (cr > cl) {
                                // tt=1→85, tt=2→170, tt=3→213
                                uint8_t a = kTTAlpha[tt];
                                while (cl < cr) {
                                    uint32_t combined = (uint32_t)a * mAlpha / 255;
                                    if (combined > 0) {
                                        dstRow[cl - visL] = blend565(pal[ix], dstRow[cl - visL], combined);
                                    }
                                    cl++;
                                }
                            }
                        }
                        px = runR;
                    }
                }
            }
            return;
        }

        // ── FMT_RGB565_RLE (3): direct color RLE, opaque ─────────
        if (imageFormat == 3 && !tint) {
            const uint8_t* rle = (const uint8_t*)src;
            const uint32_t* off = (const uint32_t*)rle;
            uint16_t* tile = mTile->buffer();
            int tStride = mTile->stride();
            const int visL = srcOffX, visR = srcOffX + copyW;
            for (int y = 0; y < copyH; y++) {
                uint32_t rowOff = off[srcOffY + y];
                uint16_t* dstRow = tile + (ty0 + y) * tStride + tx0;
                if (rowOff & 0x80000000) {
                    // Raw RGB565 row: 2B/px
                    const uint16_t* srow = (const uint16_t*)(rle + (rowOff & 0x7FFFFFFF));
                    if (mAlpha == 255) {
                        memcpy(dstRow, srow + srcOffX, (size_t)copyW * 2);
                    } else {
                        for (int x = 0; x < copyW; x++)
                            dstRow[x] = blend565(srow[srcOffX + x], dstRow[x], mAlpha);
                    }
                } else {
                const uint8_t* p = rle + rowOff;
                int px = 0;
                while (px < srcW) {
                    uint8_t cmd = *p++;
                    int n = (cmd & 0x7F) + 1;
                    int runR = px + n;
                    int cl = px < visL ? visL : px;
                    int cr = runR > visR ? visR : runR;
                    if (cmd & 0x80) {
                        // literal: n distinct pixels
                        if (cr > cl) {
                            if (mAlpha == 255) {
                                memcpy(dstRow + (cl - visL),
                                       (const uint16_t*)p + (cl - px),
                                       (size_t)(cr - cl) * 2);
                            } else {
                                const uint16_t* sp = (const uint16_t*)p + (cl - px);
                                for (int i = 0; i < cr - cl; i++)
                                    dstRow[(cl - visL) + i] = blend565(sp[i], dstRow[(cl - visL) + i], mAlpha);
                            }
                        }
                        p += n * 2;
                    } else {
                        // run: one color ×n
                        uint16_t c = *(const uint16_t*)p; p += 2;
                        if (cr > cl) {
                            if (mAlpha == 255) {
                                uint16_t* dp = dstRow + (cl - visL);
                                int cnt = cr - cl;
                                wordFill32(dp, cnt, c);
                            } else {
                                for (int i = 0; i < cr - cl; i++)
                                    dstRow[(cl - visL) + i] = blend565(c, dstRow[(cl - visL) + i], mAlpha);
                            }
                        }
                    }
                    px = runR;
                }
                } // end if raw/RLE
            }
            return;
        }

        // ── FMT_RGB565A_RLE (4): direct color RLE, alpha inline (variable-length head) ──
        if (imageFormat == 4) {
            const uint8_t* rle = (const uint8_t*)src;
            const uint32_t* off = (const uint32_t*)rle;
            uint16_t* tile = mTile->buffer();
            int tStride = mTile->stride();
            const int visL = srcOffX, visR = srcOffX + copyW;
            for (int y = 0; y < copyH; y++) {
                const uint8_t* p = rle + off[srcOffY + y];
                uint16_t* dstRow = tile + (ty0 + y) * tStride + tx0;
                int px = 0;
                while (px < srcW) {
                    uint8_t head = *p++;
                    if (head & 0x80) {
                        // ── Opaque: α=255, 7-bit run length (1..128) ──
                        int n = (head & 0x7F) + 1;
                        uint16_t c = *(const uint16_t*)p; p += 2;
                        int runR = px + n;
                        int cl = px < visL ? visL : px;
                        int cr = runR > visR ? visR : runR;
                        if (cr > cl) {
                            if (mAlpha == 255) {
                                uint16_t* dp = dstRow + (cl - visL);
                                int cnt = cr - cl;
                                wordFill32(dp, cnt, c);
                            } else {
                                for (int i = 0; i < cr - cl; i++)
                                    dstRow[(cl - visL) + i] = blend565(c, dstRow[(cl - visL) + i], mAlpha);
                            }
                        }
                        px = runR;
                    } else {
                        // ── Non-opaque: TT + 5-bit run length (1..32) ──
                        uint8_t tt = (head >> 5) & 0x03;
                        int n = (head & 0x1F) + 1;
                        int runR = px + n;
                        if (tt == 0) {
                            // Fully transparent: no color data, skip
                        } else {
                            uint16_t c = *(const uint16_t*)p; p += 2;
                            int cl = px < visL ? visL : px;
                            int cr = runR > visR ? visR : runR;
                            if (cr > cl) {
                                // tt=1→85, tt=2→170, tt=3→213
                                uint8_t a = kTTAlpha[tt];
                                while (cl < cr) {
                                    uint32_t combined = (uint32_t)a * mAlpha / 255;
                                    if (combined > 0) {
                                        dstRow[cl - visL] = blend565(c, dstRow[cl - visL], combined);
                                    }
                                    cl++;
                                }
                            }
                        }
                        px = runR;
                    }
                }
            }
            return;
        }

    }

    // ── drawImageRotated (deci-degree core) ───────────────────────

    void drawImageRotated(const void* src, int fmt,
                          int srcW, int srcH,
                          int dx, int dy,
                          int rotCx, int rotCy,
                          int16_t angleDeg,
                          const RGB565* tint   = nullptr,
                          Tile* rotBuffer      = nullptr) {
        drawImageRotatedDeci(src, fmt, srcW, srcH, dx, dy, rotCx, rotCy,
                             (int)angleDeg * 10, tint, rotBuffer);
    }

    void drawImageRotatedDeci(const void* src, int fmt,
                          int srcW, int srcH,
                          int dx, int dy,
                          int rotCx, int rotCy,
                          int angleDeci,
                          const RGB565* tint   = nullptr,
                          Tile* rotBuffer      = nullptr) {

        int imageFormat = LITHO_FORMAT(fmt);
        int palCount    = LITHO_PALETTE_SIZE(fmt);
        int palBytes    = palCount * 2;

        if (!resSinTable() && angleDeci % 900 != 0) return;

        angleDeci = ((angleDeci % 3600) + 3600) % 3600;

        int32_t cosA, sinA;
        int outW, outH;
        switch (angleDeci) {
        case 0:    cosA = 65536;  sinA = 0;       outW = srcW; outH = srcH; break;
        case 900:  cosA = 0;      sinA = 65536;   outW = srcH; outH = srcW; break;
        case 1800: cosA = -65536; sinA = 0;       outW = srcW; outH = srcH; break;
        case 2700: cosA = 0;      sinA = -65536;  outW = srcH; outH = srcW; break;
        default:
            cosA = (int32_t)cosDeci(angleDeci) << 1;
            sinA = (int32_t)sinDeci(angleDeci) << 1;
            outW = outH = 0;
            break;
        }

        int corners[4][2] = {{0,0}, {srcW,0}, {srcW,srcH}, {0,srcH}};
        int minX = 0x7FFFFFFF, maxX = -0x80000000;
        int minY = 0x7FFFFFFF, maxY = -0x80000000;
        for (int i = 0; i < 4; i++) {
            int rx = ((int32_t)(corners[i][0] - rotCx) * cosA -
                      (int32_t)(corners[i][1] - rotCy) * sinA) >> 16;
            int ry = ((int32_t)(corners[i][0] - rotCx) * sinA +
                      (int32_t)(corners[i][1] - rotCy) * cosA) >> 16;
            if (rx < minX) minX = rx; if (ry < minY) minY = ry;
            if (rx > maxX) maxX = rx; if (ry > maxY) maxY = ry;
        }
        if (angleDeci % 900 != 0) {
            outW = maxX - minX;
            outH = maxY - minY;
        }

        int32_t stepSX_dx =  cosA;
        int32_t stepSY_dx = -sinA;
        int32_t stepSX_dy =  sinA;
        int32_t stepSY_dy =  cosA;

        int32_t const halfX = (cosA + sinA) / 2 - 32768;
        int32_t const halfY = (cosA - sinA) / 2 - 32768;

        int32_t baseSX = (int32_t)rotCx * 65536
                       + (int32_t)minX * cosA
                       + (int32_t)minY * sinA
                       + halfX;
        int32_t baseSY = (int32_t)rotCy * 65536
                       - (int32_t)minX * sinA
                       + (int32_t)minY * cosA
                       + halfY;

        bool useRotBuf = (rotBuffer &&
                          rotBuffer->width()  >= outW &&
                          rotBuffer->height() >= outH);

        uint16_t* dstBuf    = useRotBuf ? rotBuffer->buffer() : mTile->buffer();
        int       dstStride = useRotBuf ? rotBuffer->stride() : mTile->stride();

        for (int y = 0; y < outH; y++) {
            int32_t curSX = baseSX;
            int32_t curSY = baseSY;

            uint16_t* dstRow;
            if (useRotBuf) {
                dstRow = dstBuf + y * dstStride;
            } else {
                int sdsty = dy + mScreenY + y;
                if (sdsty < mClipT || sdsty >= mClipB) { baseSX += stepSX_dy; baseSY += stepSY_dy; continue; }
                int tdsty = sdsty - mTileOrgY;
                if (tdsty < 0 || tdsty >= mTile->height()) { baseSX += stepSX_dy; baseSY += stepSY_dy; continue; }
                dstRow = dstBuf + tdsty * dstStride;
            }

            for (int x = 0; x < outW; x++) {
                int sx = curSX >> 16;
                int sy = curSY >> 16;

                if (sx >= 0 && sx < srcW && sy >= 0 && sy < srcH) {
                    uint16_t s;
                    uint32_t pixelA = 255;

                    // ── FMT_A8_RLE (0): [gray][8bit-len] RLE or raw row ──
                    if (imageFormat == 0) {
                        const uint8_t* rle = (const uint8_t*)src;
                        const uint32_t* off = (const uint32_t*)rle;
                        uint32_t rowOff = off[sy];
                        if (rowOff & 0x80000000) {
                            // Raw grayscale row: 1B/px
                            uint8_t g = (rle + (rowOff & 0x7FFFFFFF))[sx];
                            if (tint) {
                                uint32_t tr = (tint->value >> 11) & 0x1F, tg = (tint->value >> 5) & 0x3F, tb = tint->value & 0x1F;
                                uint32_t r = (tr * g) / 255, gg = (tg * g) / 255, b = (tb * g) / 255;
                                if (r > 0x1F) r = 0x1F; if (gg > 0x3F) gg = 0x3F; if (b > 0x1F) b = 0x1F;
                                s = (uint16_t)((r << 11) | (gg << 5) | b);
                            } else {
                                uint32_t g5 = (g >> 3) & 0x1F, g6 = (g >> 2) & 0x3F;
                                s = (uint16_t)((g5 << 11) | (g6 << 5) | g5);
                            }
                        } else {
                            const uint8_t* p = rle + rowOff;
                            int px = 0;
                            while (px <= sx) {
                                uint8_t g = *p++;
                                uint8_t len = *p++;
                                int n = (int)len + 1;  // 1..256
                                if (px + n > sx) {
                                    if (tint) {
                                        uint32_t tr = (tint->value >> 11) & 0x1F, tg = (tint->value >> 5) & 0x3F, tb = tint->value & 0x1F;
                                        uint32_t r = (tr * g) / 255, gg = (tg * g) / 255, b = (tb * g) / 255;
                                        if (r > 0x1F) r = 0x1F; if (gg > 0x3F) gg = 0x3F; if (b > 0x1F) b = 0x1F;
                                        s = (uint16_t)((r << 11) | (gg << 5) | b);
                                    } else {
                                        uint32_t g5 = (g >> 3) & 0x1F, g6 = (g >> 2) & 0x3F;
                                        s = (uint16_t)((g5 << 11) | (g6 << 5) | g5);
                                    }
                                    pixelA = 255;
                                    break;
                                }
                                px += n;
                            }
                        }
                    // ── FMT_PAL_RLE (1): [idx][8bit-len] RLE or raw row ──
                    } else if (imageFormat == 1) {
                        const uint16_t* pal = (const uint16_t*)src;
                        const uint8_t*  rle = (const uint8_t*)src + palBytes;
                        const uint32_t* off = (const uint32_t*)rle;
                        uint32_t rowOff = off[sy];
                        if (rowOff & 0x80000000) {
                            // Raw palette index row: 1B/px
                            s = pal[(rle + (rowOff & 0x7FFFFFFF))[sx]];
                        } else {
                            const uint8_t* p = rle + rowOff;
                            int px = 0;
                            while (px <= sx) {
                                uint8_t ix = *p++;
                                uint8_t len = *p++;
                                int n = (int)len + 1;  // 1..256
                                if (px + n > sx) {
                                    s = pal[ix];
                                    pixelA = 255;
                                    break;
                                }
                                px += n;
                            }
                        }
                    // ── FMT_PAL_ALPHA_RLE (2): variable-length head byte ──
                    } else if (imageFormat == 2) {
                        const uint16_t* pal = (const uint16_t*)src;
                        const uint8_t*  rle = (const uint8_t*)src + palBytes;
                        const uint32_t* off = (const uint32_t*)rle;
                        const uint8_t* p = rle + off[sy];  // no raw fallback for alpha
                        int px = 0;
                        while (px <= sx) {
                            uint8_t head = *p++;
                            if (head & 0x80) {
                                // Opaque: α=255, 7-bit run length (1..128)
                                int n = (head & 0x7F) + 1;
                                uint8_t val = *p++;
                                if (px + n > sx) {
                                    s = pal[val];
                                    pixelA = 255;
                                    break;
                                }
                                px += n;
                            } else {
                                // Non-opaque: TT + 5-bit run length (1..32)
                                uint8_t tt = (head >> 5) & 0x03;
                                int n = (head & 0x1F) + 1;
                                if (tt == 0) {
                                    // Fully transparent
                                    if (px + n > sx) {
                                        s = 0;
                                        pixelA = 0;
                                        break;
                                    }
                                } else {
                                    uint8_t val = *p++;
                                    if (px + n > sx) {
                                        s = pal[val];
                                        pixelA = kTTAlpha[tt];  // tt=1→85, tt=2→170, tt=3→213
                                        break;
                                    }
                                }
                                px += n;
                            }
                        }
                    // FMT_RGB565_RLE (3): run/literal command encoding or raw row
                    } else if (imageFormat == 3) {
                        const uint8_t* rle = (const uint8_t*)src;
                        const uint32_t* off = (const uint32_t*)rle;
                        uint32_t rowOff = off[sy];
                        if (rowOff & 0x80000000) {
                            s = ((const uint16_t*)(rle + (rowOff & 0x7FFFFFFF)))[sx];
                        } else {
                        const uint8_t* p = rle + rowOff;
                        int px = 0;
                        while (px <= sx) {
                            uint8_t cmd = *p++;
                            int n = (cmd & 0x7F) + 1;
                            if (px + n > sx) {
                                if (cmd & 0x80) {
                                    s = *(const uint16_t*)(p + (sx - px) * 2);
                                } else {
                                    s = *(const uint16_t*)p;
                                }
                                break;
                            }
                            if (cmd & 0x80) p += n * 2;
                            else p += 2;
                            px += n;
                        }
                        } // end if raw/RLE
                    // FMT_RGB565A_RLE (4): variable-length head byte
                    } else if (imageFormat == 4) {
                        const uint8_t* rle = (const uint8_t*)src;
                        const uint32_t* off = (const uint32_t*)rle;
                        const uint8_t* p = rle + off[sy];  // no raw fallback for alpha
                        int px = 0;
                        while (px <= sx) {
                            uint8_t head = *p++;
                            if (head & 0x80) {
                                // Opaque: α=255, 7-bit run length (1..128)
                                int n = (head & 0x7F) + 1;
                                uint16_t c = *(const uint16_t*)p; p += 2;
                                if (px + n > sx) {
                                    s = c;
                                    pixelA = 255;
                                    break;
                                }
                                px += n;
                            } else {
                                // Non-opaque: TT + 5-bit run length (1..32)
                                uint8_t tt = (head >> 5) & 0x03;
                                int n = (head & 0x1F) + 1;
                                if (tt == 0) {
                                    // Fully transparent, no color data
                                    if (px + n > sx) {
                                        s = 0;
                                        pixelA = 0;
                                        break;
                                    }
                                } else {
                                    uint16_t c = *(const uint16_t*)p; p += 2;
                                    if (px + n > sx) {
                                        s = c;
                                        pixelA = kTTAlpha[tt];  // tt=1→85, tt=2→170, tt=3→213
                                        break;
                                    }
                                }
                                px += n;
                            }
                        }
                    } else {
                        s = 0;
                    }

                    if (useRotBuf) {
                        dstRow[x] = s;
                    } else {
                        int sdstx = dx + mScreenX + x;
                        if (sdstx < mClipL || sdstx >= mClipR) goto next_pixel;
                        int tdstx = sdstx - mTileOrgX;
                        if (tdstx < 0 || tdstx >= mTile->width()) goto next_pixel;

                        uint16_t d = dstRow[tdstx];
                        if (pixelA == 255 && mAlpha == 255) {
                            dstRow[tdstx] = s;
                        } else if (pixelA > 0) {
                            uint32_t a = pixelA * (uint32_t)mAlpha / 255;
                            if (a > 0) {
                                dstRow[tdstx] = blend565(s, d, a);
                            }
                        }
                    }
                }

            next_pixel:
                curSX += stepSX_dx;
                curSY += stepSY_dx;
            }
            baseSX += stepSX_dy;
            baseSY += stepSY_dy;
        }

        // Blit rotation buffer to tile
        if (useRotBuf) {
            int sx = dx + mScreenX;
            int sy = dy + mScreenY;
            int dstStride = mTile->stride();
            int srcStride = rotBuffer->stride();
            uint16_t* dstBase = mTile->buffer();
            const uint16_t* srcBase = rotBuffer->buffer();

            for (int y = 0; y < outH; y++) {
                int sdsty = sy + y;
                if (sdsty < mClipT || sdsty >= mClipB) continue;
                int tdsty = sdsty - mTileOrgY;
                if (tdsty < 0 || tdsty >= mTile->height()) continue;

                uint16_t*       dstRow = dstBase + tdsty * dstStride;
                const uint16_t* srcRow = srcBase + y * srcStride;

                for (int x = 0; x < outW; x++) {
                    int sdstx = sx + x;
                    if (sdstx < mClipL || sdstx >= mClipR) continue;
                    int tdstx = sdstx - mTileOrgX;
                    if (tdstx < 0 || tdstx >= mTile->width()) continue;

                    uint16_t s = srcRow[x];
                    if (mAlpha == 255) {
                        dstRow[tdstx] = s;
                    } else {
                        dstRow[tdstx] = blend565(s, dstRow[tdstx], mAlpha);
                    }
                }
            }
        }
    }

    void copyTile(const Tile& src, int dx, int dy) {
        int sx0 = dx + mScreenX;
        int sy0 = dy + mScreenY;
        int w   = src.width();
        int h   = src.height();
        int sx1 = sx0 + w;
        int sy1 = sy0 + h;

        if (sx0 < mClipL) sx0 = mClipL;
        if (sy0 < mClipT) sy0 = mClipT;
        if (sx1 > mClipR) sx1 = mClipR;
        if (sy1 > mClipB) sy1 = mClipB;
        if (sx0 >= sx1 || sy0 >= sy1) return;

        int tx0   = sx0 - mTileOrgX;
        int ty0   = sy0 - mTileOrgY;
        int copyW = sx1 - sx0;
        int copyH = sy1 - sy0;

        if (tx0 < 0) { copyW += tx0; tx0 = 0; }
        if (ty0 < 0) { copyH += ty0; ty0 = 0; }
        if (tx0 + copyW > mTile->width())  copyW = mTile->width()  - tx0;
        if (ty0 + copyH > mTile->height()) copyH = mTile->height() - ty0;
        if (copyW <= 0 || copyH <= 0) return;

        int srcOffX = sx0 - dx - mScreenX;
        int srcOffY = sy0 - dy - mScreenY;

        if (mAlpha == 255) {
            for (int y = 0; y < copyH; y++) {
                uint16_t*       dstRow = mTile->buffer() + (ty0 + y) * mTile->stride() + tx0;
                const uint16_t* srcRow = src.buffer() + (srcOffY + y) * src.stride() + srcOffX;
                for (int x = 0; x < copyW; x++) dstRow[x] = srcRow[x];
            }
        } else {
            for (int y = 0; y < copyH; y++) {
                uint16_t*       dstRow = mTile->buffer() + (ty0 + y) * mTile->stride() + tx0;
                const uint16_t* srcRow = src.buffer() + (srcOffY + y) * src.stride() + srcOffX;
                for (int x = 0; x < copyW; x++) {
                    dstRow[x] = blend565(srcRow[x], dstRow[x], mAlpha);
                }
            }
        }
        }

    // ── drawText (UTF-8, charset font from LIMB) ─────────────────
    // (x, y) is the top-left of the text box; baseline = y + font ascent.
    // A8 glyph coverage is blended onto the tile (not opaque tint write).

    static int utf8Decode(const char*& p, const char* end) {
        if (p >= end) return -1;
        unsigned char c = (unsigned char)*p++;
        if (c < 0x80) return (int)c;
        if ((c & 0xE0) == 0xC0) {
            if (p >= end) return -1;
            unsigned char c1 = (unsigned char)*p++;
            if ((c1 & 0xC0) != 0x80) return -1;
            return ((c & 0x1F) << 6) | (c1 & 0x3F);
        }
        if ((c & 0xF0) == 0xE0) {
            if (p + 1 >= end) return -1;
            unsigned char c1 = (unsigned char)*p++;
            unsigned char c2 = (unsigned char)*p++;
            if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) return -1;
            return ((c & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
        }
        if ((c & 0xF8) == 0xF0) {
            if (p + 2 >= end) return -1;
            unsigned char c1 = (unsigned char)*p++;
            unsigned char c2 = (unsigned char)*p++;
            unsigned char c3 = (unsigned char)*p++;
            if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80)
                return -1;
            return ((c & 0x07) << 18) | ((c1 & 0x3F) << 12) |
                   ((c2 & 0x3F) << 6) | (c3 & 0x3F);
        }
        return -1; // invalid / skipped
    }

    __attribute__((noinline, section(".ramfunc")))
    void drawGlyphA8Blend(const void* src, int srcW, int srcH,
                          int dx, int dy, RGB565 color) {
        if (!mTile || !mTile->buffer() || srcW <= 0 || srcH <= 0 || !src) return;

        if (mScale != kScaleOne) {
            // Same FP mapping as fillRect/drawImage: originFP + local * scale.
            int64_t s = (int64_t)mScale;
            int64_t leftFP  = mOriginXFP + (int64_t)dx * s;
            int64_t topFP   = mOriginYFP + (int64_t)dy * s;
            int64_t rightFP = leftFP + (int64_t)srcW * s;
            int64_t botFP   = topFP + (int64_t)srcH * s;
            int sx = roundFP(leftFP);
            int sy = roundFP(topFP);
            int dstW = roundFP(rightFP) - sx;
            int dstH = roundFP(botFP) - sy;
            if (dstW < 1) dstW = 1;
            if (dstH < 1) dstH = 1;
            blitGlyphA8Scaled(src, srcW, srcH, sx, sy, dstW, dstH, color);
            return;
        }

        int sx0 = dx + mScreenX;
        int sy0 = dy + mScreenY;
        int sx1 = sx0 + srcW;
        int sy1 = sy0 + srcH;

        if (sx0 < mClipL) sx0 = mClipL;
        if (sy0 < mClipT) sy0 = mClipT;
        if (sx1 > mClipR) sx1 = mClipR;
        if (sy1 > mClipB) sy1 = mClipB;
        if (sx0 >= sx1 || sy0 >= sy1) return;

        int tx0 = sx0 - mTileOrgX;
        int ty0 = sy0 - mTileOrgY;
        int copyW = sx1 - sx0;
        int copyH = sy1 - sy0;

        if (tx0 < 0) { copyW += tx0; tx0 = 0; }
        if (ty0 < 0) { copyH += ty0; ty0 = 0; }
        if (tx0 + copyW > mTile->width())  copyW = mTile->width()  - tx0;
        if (ty0 + copyH > mTile->height()) copyH = mTile->height() - ty0;
        if (copyW <= 0 || copyH <= 0) return;

        int srcOffX = sx0 - (dx + mScreenX);
        int srcOffY = sy0 - (dy + mScreenY);

        const uint8_t* rle = (const uint8_t*)src;
        const uint32_t* off = (const uint32_t*)rle;
        uint16_t* tile = mTile->buffer();
        int tStride = mTile->stride();
        const int visL = srcOffX, visR = srcOffX + copyW;
        const uint16_t srcColor = color.value;
        const uint32_t viewA = mAlpha;

        for (int y = 0; y < copyH; y++) {
            uint32_t rowOff = off[srcOffY + y];
            uint16_t* dstRow = tile + (ty0 + y) * tStride + tx0;
            if (rowOff & 0x80000000) {
                const uint8_t* srow = rle + (rowOff & 0x7FFFFFFF);
                for (int x = 0; x < copyW; x++) {
                    uint32_t a = srow[srcOffX + x];
                    if (!a) continue;
                    if (viewA != 255) a = (a * viewA) / 255;
                    if (!a) continue;
                    if (a >= 255) dstRow[x] = srcColor;
                    else dstRow[x] = blend565(srcColor, dstRow[x], a);
                }
            } else {
                const uint8_t* p = rle + rowOff;
                int px = 0;
                int loopCnt = 0;
                while (px < srcW) {
                    if (++loopCnt > srcW * 2) break;
                    uint8_t g = *p++;
                    uint8_t len = *p++;
                    int n = (int)len + 1;
                    int runR = px + n;
                    int cl = px < visL ? visL : px, cr = runR > visR ? visR : runR;
                    if (cr > cl && g) {
                        uint32_t a = g;
                        if (viewA != 255) a = (a * viewA) / 255;
                        if (a) {
                            uint16_t* dp = dstRow + (cl - visL);
                            int cnt = cr - cl;
                            if (a >= 255) {
                                wordFill32(dp, cnt, srcColor);
                            } else {
                                for (int i = 0; i < cnt; i++)
                                    dp[i] = blend565(srcColor, dp[i], a);
                            }
                        }
                    }
                    px = runR;
                }
            }
        }
    }

    __attribute__((noinline, section(".ramfunc")))
    void drawText(const char* utf8, int x, int y, RGB565 color) {
        if (!utf8 || !mTile || !mTile->buffer()) return;
        const FontSectionHeader* fh = fontSection();
        if (!fh) return;

        const char* p = utf8;
        const char* end = utf8 + lithoStrlen(utf8);
        int penX = x;
        int baseline = y + (int)fh->ascent;
        const int lineH = (int)fh->lineHeight;
        const int originX = x;

        // Logical layout only; scale is applied inside drawGlyph via Painter FP origin.
        while (p < end) {
            if (*p == '\n') {
                ++p;
                penX = originX;
                baseline += lineH;
                continue;
            }
            int cp = utf8Decode(p, end);
            if (cp < 0) continue;

            const GlyphEntry* g = fontFindGlyph((uint32_t)cp);
            if (!g) continue;

            if (g->width > 0 && g->height > 0 && g->size > 0) {
                int dx = penX + (int)g->bearingX;
                int dy = baseline - (int)g->bearingY;
                drawGlyphA8Blend(glyphPixels(g), (int)g->width, (int)g->height,
                                 dx, dy, color);
            }
            penX += (int)g->advance;
        }
    }

    // Measure UTF-8 text using packed glyph advances. Returns width;
    // optional outH receives total height (lineHeight * lines).
    static int measureText(const char* utf8, int* outH = nullptr) {
        const FontSectionHeader* fh = fontSection();
        if (!fh || !utf8) {
            if (outH) *outH = 0;
            return 0;
        }
        const char* p = utf8;
        const char* end = utf8 + lithoStrlen(utf8);
        int lineW = 0, maxW = 0, lines = 1;
        while (p < end) {
            if (*p == '\n') {
                ++p;
                if (lineW > maxW) maxW = lineW;
                lineW = 0;
                ++lines;
                continue;
            }
            int cp = utf8Decode(p, end);
            if (cp < 0) continue;
            const GlyphEntry* g = fontFindGlyph((uint32_t)cp);
            if (g) lineW += (int)g->advance;
        }
        if (lineW > maxW) maxW = lineW;
        if (outH) *outH = lines * (int)fh->lineHeight;
        return maxW;
    }

private:
    // Bilinear blit of an A8 glyph into an already-scaled screen rectangle.
    void blitGlyphA8Scaled(const void* src, int srcW, int srcH,
                           int screenX, int screenY, int dstW, int dstH,
                           RGB565 color) {
        if (!mTile || !mTile->buffer() || !src || dstW <= 0 || dstH <= 0) return;

        int sx0 = screenX, sy0 = screenY;
        int sx1 = screenX + dstW, sy1 = screenY + dstH;
        if (sx0 < mClipL) sx0 = mClipL;
        if (sy0 < mClipT) sy0 = mClipT;
        if (sx1 > mClipR) sx1 = mClipR;
        if (sy1 > mClipB) sy1 = mClipB;
        if (sx0 >= sx1 || sy0 >= sy1) return;

        int tx0 = sx0 - mTileOrgX, ty0 = sy0 - mTileOrgY;
        int tx1 = sx1 - mTileOrgX, ty1 = sy1 - mTileOrgY;
        if (tx0 < 0) tx0 = 0;
        if (ty0 < 0) ty0 = 0;
        if (tx1 > mTile->width())  tx1 = mTile->width();
        if (ty1 > mTile->height()) ty1 = mTile->height();
        if (tx0 >= tx1 || ty0 >= ty1) return;

        const uint8_t* rle = (const uint8_t*)src;
        const uint32_t* off = (const uint32_t*)rle;
        uint16_t* tile = mTile->buffer();
        int tStride = mTile->stride();
        const uint16_t srcColor = color.value;
        const uint32_t viewA = mAlpha;

        auto sampleA8 = [&](int sx, int sy) -> uint32_t {
            if (sx < 0 || sy < 0 || sx >= srcW || sy >= srcH) return 0;
            uint32_t rowOff = off[sy];
            if (rowOff & 0x80000000) {
                return (rle + (rowOff & 0x7FFFFFFF))[sx];
            }
            const uint8_t* p = rle + rowOff;
            int px = 0;
            while (px <= sx) {
                uint8_t g = *p++;
                uint8_t len = *p++;
                int n = (int)len + 1;
                if (px + n > sx) return g;
                px += n;
            }
            return 0;
        };

        for (int ty = ty0; ty < ty1; ty++) {
            int localY = (ty + mTileOrgY) - screenY;
            if (localY < 0 || localY >= dstH) continue;
            int y0, y1, fy;
            scaleMapQ8(localY, dstH, srcH, y0, y1, fy);
            uint16_t* dstRow = tile + ty * tStride;
            for (int tx = tx0; tx < tx1; tx++) {
                int localX = (tx + mTileOrgX) - screenX;
                if (localX < 0 || localX >= dstW) continue;
                int x0, x1, fx;
                scaleMapQ8(localX, dstW, srcW, x0, x1, fx);
                uint32_t a00 = sampleA8(x0, y0);
                uint32_t a10 = sampleA8(x1, y0);
                uint32_t a01 = sampleA8(x0, y1);
                uint32_t a11 = sampleA8(x1, y1);
                uint32_t w00 = (uint32_t)(255 - fx) * (uint32_t)(255 - fy);
                uint32_t w10 = (uint32_t)fx * (uint32_t)(255 - fy);
                uint32_t w01 = (uint32_t)(255 - fx) * (uint32_t)fy;
                uint32_t w11 = (uint32_t)fx * (uint32_t)fy;
                uint32_t a = (a00 * w00 + a10 * w10 + a01 * w01 + a11 * w11) / (255u * 255u);
                if (!a) continue;
                if (viewA != 255) a = (a * viewA) / 255;
                if (!a) continue;
                if (a >= 255) dstRow[tx] = srcColor;
                else dstRow[tx] = blend565(srcColor, dstRow[tx], a);
            }
        }
    }

    void drawImageScaled(const void* src, int fmt,
                         int srcW, int srcH, int dx, int dy,
                         const RGB565* tint) {
        if (!mTile || !mTile->buffer() || !src || srcW <= 0 || srcH <= 0) return;

        int imageFormat = LITHO_FORMAT(fmt);
        int paletteSize = LITHO_PALETTE_SIZE(fmt);
        int palBytes = paletteSize * 2;

        const int64_t s = (int64_t)mScale;
        int64_t leftFP  = mOriginXFP + (int64_t)dx * s;
        int64_t topFP   = mOriginYFP + (int64_t)dy * s;
        int64_t rightFP = leftFP + (int64_t)srcW * s;
        int64_t botFP   = topFP + (int64_t)srcH * s;
        int baseSX = roundFP(leftFP);
        int baseSY = roundFP(topFP);
        int dstW = roundFP(rightFP) - baseSX;
        int dstH = roundFP(botFP) - baseSY;
        if (dstW < 1) dstW = 1;
        if (dstH < 1) dstH = 1;

        int sx0 = baseSX, sy0 = baseSY;
        int sx1 = baseSX + dstW, sy1 = baseSY + dstH;

        if (sx0 < mClipL) sx0 = mClipL;
        if (sy0 < mClipT) sy0 = mClipT;
        if (sx1 > mClipR) sx1 = mClipR;
        if (sy1 > mClipB) sy1 = mClipB;
        if (sx0 >= sx1 || sy0 >= sy1) return;

        int tx0 = sx0 - mTileOrgX;
        int ty0 = sy0 - mTileOrgY;
        int tx1 = sx1 - mTileOrgX;
        int ty1 = sy1 - mTileOrgY;
        if (tx0 < 0) tx0 = 0;
        if (ty0 < 0) ty0 = 0;
        if (tx1 > mTile->width())  tx1 = mTile->width();
        if (ty1 > mTile->height()) ty1 = mTile->height();
        if (tx0 >= tx1 || ty0 >= ty1) return;

        uint16_t* tile = mTile->buffer();
        int tStride = mTile->stride();

        for (int ty = ty0; ty < ty1; ty++) {
            int screenY = ty + mTileOrgY;
            int localY = screenY - baseSY;
            if (localY < 0 || localY >= dstH) continue;
            int y0, y1, fy;
            scaleMapQ8(localY, dstH, srcH, y0, y1, fy);

            uint16_t* dstRow = tile + ty * tStride;
            for (int tx = tx0; tx < tx1; tx++) {
                int screenX = tx + mTileOrgX;
                int localX = screenX - baseSX;
                if (localX < 0 || localX >= dstW) continue;
                int x0, x1, fx;
                scaleMapQ8(localX, dstW, srcW, x0, x1, fx);

                uint16_t c00 = 0, c10 = 0, c01 = 0, c11 = 0;
                uint32_t a00 = 0, a10 = 0, a01 = 0, a11 = 0;
                sampleImagePixel(src, imageFormat, palBytes, srcW, srcH, x0, y0, tint, c00, a00);
                sampleImagePixel(src, imageFormat, palBytes, srcW, srcH, x1, y0, tint, c10, a10);
                sampleImagePixel(src, imageFormat, palBytes, srcW, srcH, x0, y1, tint, c01, a01);
                sampleImagePixel(src, imageFormat, palBytes, srcW, srcH, x1, y1, tint, c11, a11);

                uint16_t color = 0;
                uint32_t pixelA = 0;
                bilinear565(c00, a00, c10, a10, c01, a01, c11, a11, fx, fy, color, pixelA);
                uint32_t a = pixelA * (uint32_t)mAlpha / 255;
                if (!a) continue;
                if (a >= 255) dstRow[tx] = color;
                else dstRow[tx] = blend565(color, dstRow[tx], a);
            }
        }
    }

    static bool sampleImagePixel(const void* src, int imageFormat, int palBytes,
                                 int srcW, int srcH, int sx, int sy,
                                 const RGB565* tint,
                                 uint16_t& outColor, uint32_t& outA) {
        outColor = 0;
        outA = 0;
        if (sx < 0 || sy < 0 || sx >= srcW || sy >= srcH) return false;

        if (imageFormat == 0) {
            const uint8_t* rle = (const uint8_t*)src;
            const uint32_t* off = (const uint32_t*)rle;
            uint32_t rowOff = off[sy];
            uint8_t g = 0;
            if (rowOff & 0x80000000) {
                g = (rle + (rowOff & 0x7FFFFFFF))[sx];
            } else {
                const uint8_t* p = rle + rowOff;
                int px = 0;
                while (px <= sx) {
                    uint8_t gv = *p++;
                    uint8_t len = *p++;
                    int n = (int)len + 1;
                    if (px + n > sx) { g = gv; break; }
                    px += n;
                }
            }
            if (tint) {
                uint32_t tr = (tint->value >> 11) & 0x1F, tg = (tint->value >> 5) & 0x3F, tb = tint->value & 0x1F;
                uint32_t r = (tr * g) / 255, gg = (tg * g) / 255, b = (tb * g) / 255;
                outColor = (uint16_t)((r << 11) | (gg << 5) | b);
            } else {
                outColor = (uint16_t)(((g >> 3) & 0x1F) << 11 | ((g >> 2) & 0x3F) << 5 | ((g >> 3) & 0x1F));
            }
            outA = 255;
            return true;
        }

        if (imageFormat == 1) {
            const uint16_t* pal = (const uint16_t*)src;
            const uint8_t*  rle = (const uint8_t*)src + palBytes;
            const uint32_t* off = (const uint32_t*)rle;
            uint32_t rowOff = off[sy];
            uint8_t ix = 0;
            if (rowOff & 0x80000000) {
                ix = (rle + (rowOff & 0x7FFFFFFF))[sx];
            } else {
                const uint8_t* p = rle + rowOff;
                int px = 0;
                while (px <= sx) {
                    uint8_t i = *p++;
                    uint8_t len = *p++;
                    int n = (int)len + 1;
                    if (px + n > sx) { ix = i; break; }
                    px += n;
                }
            }
            outColor = pal[ix];
            outA = 255;
            return true;
        }

        if (imageFormat == 2) {
            const uint16_t* pal = (const uint16_t*)src;
            const uint8_t*  rle = (const uint8_t*)src + palBytes;
            const uint32_t* off = (const uint32_t*)rle;
            const uint8_t* p = rle + off[sy];
            int px = 0;
            while (px <= sx) {
                uint8_t head = *p++;
                if (head & 0x80) {
                    int n = (head & 0x7F) + 1;
                    uint8_t ix = *p++;
                    if (px + n > sx) { outColor = pal[ix]; outA = 255; return true; }
                    px += n;
                } else {
                    uint8_t tt = (head >> 5) & 0x03;
                    int n = (head & 0x1F) + 1;
                    if (tt == 0) {
                        if (px + n > sx) return false; // fully transparent
                        px += n;
                    } else {
                        uint8_t ix = *p++;
                        if (px + n > sx) {
                            outColor = pal[ix];
                            outA = kTTAlpha[tt];
                            return outA > 0;
                        }
                        px += n;
                    }
                }
            }
            return false;
        }

        if (imageFormat == 3) {
            const uint8_t* rle = (const uint8_t*)src;
            const uint32_t* off = (const uint32_t*)rle;
            uint32_t rowOff = off[sy];
            if (rowOff & 0x80000000) {
                outColor = ((const uint16_t*)(rle + (rowOff & 0x7FFFFFFF)))[sx];
                outA = 255;
                return true;
            }
            const uint8_t* p = rle + rowOff;
            int px = 0;
            while (px <= sx) {
                uint8_t cmd = *p++;
                int n = (cmd & 0x7F) + 1;
                if (cmd & 0x80) {
                    if (px + n > sx) {
                        outColor = ((const uint16_t*)p)[sx - px];
                        outA = 255;
                        return true;
                    }
                    p += n * 2;
                    px += n;
                } else {
                    uint16_t c = *(const uint16_t*)p; p += 2;
                    if (px + n > sx) { outColor = c; outA = 255; return true; }
                    px += n;
                }
            }
            return false;
        }

        if (imageFormat == 4) {
            const uint8_t* rle = (const uint8_t*)src;
            const uint32_t* off = (const uint32_t*)rle;
            const uint8_t* p = rle + off[sy];
            int px = 0;
            while (px <= sx) {
                uint8_t head = *p++;
                if (head & 0x80) {
                    int n = (head & 0x7F) + 1;
                    uint16_t c = *(const uint16_t*)p; p += 2;
                    if (px + n > sx) { outColor = c; outA = 255; return true; }
                    px += n;
                } else {
                    uint8_t tt = (head >> 5) & 0x03;
                    int n = (head & 0x1F) + 1;
                    if (tt == 0) {
                        if (px + n > sx) return false;
                        px += n;
                    } else {
                        uint16_t c = *(const uint16_t*)p; p += 2;
                        if (px + n > sx) {
                            outColor = c;
                            outA = kTTAlpha[tt];
                            return outA > 0;
                        }
                        px += n;
                    }
                }
            }
            return false;
        }
        return false;
    }

    Tile*   mTile     = nullptr;
    int     mTileOrgX = 0;
    int     mTileOrgY = 0;
    int     mScreenX  = 0;
    int     mScreenY  = 0;
    int64_t mOriginXFP = 0;
    int64_t mOriginYFP = 0;
    int     mClipL    = -32768;
    int     mClipT    = -32768;
    int     mClipR    = 32767;
    int     mClipB    = 32767;
    uint8_t mAlpha    = 255;
    uint8_t mTileIdx  = 0;
    uint32_t mScale   = kScaleOne;
};

} // namespace litho
