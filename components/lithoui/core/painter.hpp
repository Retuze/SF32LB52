#pragma once
#include "tile.hpp"
#include "litho_core.h"
#include "res_images.h"
#include <stdio.h>
#include <string.h>
#ifndef DWT_CYCCNT
#define DWT_CYCCNT (*(volatile uint32_t*)0xE0001004UL)
#endif

// Image formats — defined in res_images.h:
//   FMT_A8_RLE = 0        grayscale RLE, tint coloring, opaque
//   FMT_PAL_RLE = 1        palette RLE, RGB565 palette, opaque
//   FMT_PAL_ALPHA_RLE = 2  palette RLE, alpha inline in RLE stream
//   FMT_RGB565_RLE = 3     direct color RLE, opaque
//   FMT_RGB565A_RLE = 4    direct color RLE, alpha inline in RLE stream
//
// The `fmt` parameter carries formatInfo: bits 2:0 = format enum, bits 7:3 = paletteBits.
// Use LITHO_FORMAT(fmt) and LITHO_PALETTE_BITS(fmt) macros from res_images.h.

// TT field (bits7:6 of alpha-format head byte) → actual alpha value
//   TT=00 → alpha=0   (fully transparent, 1-byte record)
//   TT=01 → alpha=255 (opaque, head + color data)
//   TT=10 → alpha=85  (semi-transparent)
//   TT=11 → alpha=170 (semi-transparent)
static const uint8_t kTTAlpha[4] = {0, 255, 85, 170};

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

    void setScreenOrigin(int sx, int sy) { mScreenX = sx; mScreenY = sy; }
    int  screenX() const { return mScreenX; }
    int  screenY() const { return mScreenY; }

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
        if (!mTile || !mTile->buffer()) return;
        int sx0 = x + mScreenX;
        int sy0 = y + mScreenY;
        int sx1 = sx0 + w;
        int sy1 = sy0 + h;

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
                        dstRow[x] = tint ? tintLut[g]
                            : (uint16_t)(((g >> 3) & 0x1F) << 11 | ((g >> 2) & 0x3F) << 5 | ((g >> 3) & 0x1F));
                    }
                } else {
                    const uint8_t* p = rle + rowOff;
                    int px = 0;
                    while (px < srcW) {
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
                            wordFill32(dp, cnt, c);
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
                    for (int x = 0; x < copyW; x++)
                        dstRow[x] = pal[srow[srcOffX + x]];
                } else {
                    const uint8_t* p = rle + rowOff;
                    int px = 0;
                    while (px < srcW) {
                        uint8_t ix = *p++; uint8_t len = *p++;
                        int n = (int)len + 1;
                        int runR = px + n, cl = px < visL ? visL : px, cr = runR > visR ? visR : runR;
                        if (cr > cl) {
                            uint16_t c  = pal[ix];
                            uint16_t* dp = dstRow + (cl - visL); int cnt = cr - cl;
                            wordFill32(dp, cnt, c);
                        }
                        px = runR;
                    }
                }
            }
            return;
        }

        // ── FMT_PAL_ALPHA_RLE (2): palette + RLE, alpha inline ───
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
                while (px < srcW) {
                    uint8_t head = *p++;
                    uint8_t tt   = head >> 6;
                    int n = (head & 0x3F) + 1;  // run length 1..64
                    int runR = px + n;
                    if (tt == 0) {
                        // Fully transparent: no color data, skip
                    } else {
                        uint8_t ix = *p++;
                        int cl = px < visL ? visL : px;
                        int cr = runR > visR ? visR : runR;
                        if (cr > cl) {
                            if (tt == 1) {
                                // Opaque: word-fill
                                uint16_t c  = pal[ix];
                                uint16_t* dp = dstRow + (cl - visL); int cnt = cr - cl;
                                wordFill32(dp, cnt, c);
                            } else {
                                // Semi-transparent: blend
                                uint8_t a = kTTAlpha[tt];  // tt=2→85, tt=3→170
                                while (cl < cr) {
                                    uint32_t combined = (uint32_t)a * mAlpha / 255;
                                    if (combined > 0) {
                                        dstRow[cl - visL] = blend565(pal[ix], dstRow[cl - visL], combined);
                                    }
                                    cl++;
                                }
                            }
                        }
                    }
                    px = runR;
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

        // ── FMT_RGB565A_RLE (4): direct color RLE, alpha inline ──
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
                    uint8_t tt   = head >> 6;
                    int n = (head & 0x3F) + 1;  // run length 1..64
                    int runR = px + n;
                    if (tt == 0) {
                        // Fully transparent: no color data, skip
                    } else {
                        uint16_t c = *(const uint16_t*)p; p += 2;
                        int cl = px < visL ? visL : px;
                        int cr = runR > visR ? visR : runR;
                        if (cr > cl) {
                            if (tt == 1) {
                                // Opaque
                                if (mAlpha == 255) {
                                    uint16_t* dp = dstRow + (cl - visL);
                                    int cnt = cr - cl;
                                    wordFill32(dp, cnt, c);
                                } else {
                                    for (int i = 0; i < cr - cl; i++)
                                        dstRow[(cl - visL) + i] = blend565(c, dstRow[(cl - visL) + i], mAlpha);
                                }
                            } else {
                                // Semi-transparent (tt=2→85, tt=3→170)
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
                    }
                    px = runR;
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

                    // FMT_A8_RLE (0), FMT_PAL_RLE (1), FMT_PAL_ALPHA_RLE (2): head-byte RLE or raw row
                    if (imageFormat == 0 || imageFormat == 1 || imageFormat == 2) {
                        const uint8_t* rle;
                        const uint16_t* pal = nullptr;
                        if (imageFormat == 0) {
                            rle = (const uint8_t*)src;
                        } else {
                            pal = (const uint16_t*)src;
                            rle = (const uint8_t*)src + palBytes;
                        }
                        const uint32_t* off = (const uint32_t*)rle;
                        uint32_t rowOff = off[sy];
                        if (rowOff & 0x80000000) {
                            // Raw row: 1B/px palette index or grayscale
                            uint8_t v = (rle + (rowOff & 0x7FFFFFFF))[sx];
                            if (imageFormat == 0) {
                                uint8_t g = v;
                                if (tint) {
                                    uint32_t tr = (tint->value >> 11) & 0x1F, tg = (tint->value >> 5) & 0x3F, tb = tint->value & 0x1F;
                                    uint32_t r = (tr * g) / 255, gg = (tg * g) / 255, b = (tb * g) / 255;
                                    if (r > 0x1F) r = 0x1F; if (gg > 0x3F) gg = 0x3F; if (b > 0x1F) b = 0x1F;
                                    s = (uint16_t)((r << 11) | (gg << 5) | b);
                                } else { uint32_t g5 = (g >> 3) & 0x1F, g6 = (g >> 2) & 0x3F; s = (uint16_t)((g5 << 11) | (g6 << 5) | g5); }
                            } else {
                                s = pal[v];
                            }
                        } else {
                            const uint8_t* p = rle + rowOff;
                            int px = 0;
                        while (px <= sx) {
                            uint8_t head = *p++;
                            uint8_t tt   = head >> 6;
                            int n = (head & 0x1F) + 1;  // 1..32
                            uint8_t alpha = 255;
                            uint8_t val = 0;
                            if (tt != 0) {
                                val = *p++;
                                alpha = kTTAlpha[tt];
                            }
                            if (px + n > sx) {
                                if (imageFormat == 0) {
                                    uint8_t g = val;
                                    if (tint) {
                                        uint32_t tr = (tint->value >> 11) & 0x1F, tg = (tint->value >> 5) & 0x3F, tb = tint->value & 0x1F;
                                        uint32_t r = (tr * g) / 255, gg = (tg * g) / 255, b = (tb * g) / 255;
                                        if (r > 0x1F) r = 0x1F; if (gg > 0x3F) gg = 0x3F; if (b > 0x1F) b = 0x1F;
                                        s = (uint16_t)((r << 11) | (gg << 5) | b);
                                    } else { uint32_t g5 = (g >> 3) & 0x1F, g6 = (g >> 2) & 0x3F; s = (uint16_t)((g5 << 11) | (g6 << 5) | g5); }
                                } else {
                                    s = pal[val];
                                }
                                pixelA = alpha;
                                break;
                            }
                            px += n;
                        }
                        } // end if raw/RLE
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
                    // FMT_RGB565A_RLE (4): head-byte + optional color
                    } else if (imageFormat == 4) {
                        const uint8_t* rle = (const uint8_t*)src;
                        const uint32_t* off = (const uint32_t*)rle;
                        const uint8_t* p = rle + off[sy];
                        int px = 0;
                        while (px <= sx) {
                            uint8_t head = *p++;
                            uint8_t tt   = head >> 6;
                            int n = (head & 0x1F) + 1;  // 1..32
                            uint8_t alpha = kTTAlpha[tt];
                            uint16_t c = 0;
                            if (tt != 0) {
                                c = *(const uint16_t*)p; p += 2;
                            }
                            if (px + n > sx) {
                                s = c;
                                pixelA = alpha;
                                break;
                            }
                            px += n;
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

private:
    Tile*   mTile     = nullptr;
    int     mTileOrgX = 0;
    int     mTileOrgY = 0;
    int     mScreenX  = 0;
    int     mScreenY  = 0;
    int     mClipL    = -32768;
    int     mClipT    = -32768;
    int     mClipR    = 32767;
    int     mClipB    = 32767;
    uint8_t mAlpha    = 255;
    uint8_t mTileIdx  = 0;
};

} // namespace litho
