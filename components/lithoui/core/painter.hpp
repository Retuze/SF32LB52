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
//   FMT_A8 = 0 (raw)    FMT_A8_RLE = 1       grayscale ±RLE
//   FMT_PAL8 = 2 (raw)  FMT_PAL8_RLE = 3     palette ±RLE
//   FMT_PAL8_ALPHA = 4 (raw)  FMT_PAL8_ALPHA_RLE = 5   palette+alpha ±RLE

// Alpha level → actual alpha value (for FMT_PAL8_RLE_ALPHA, 2 bits → 4 levels)
static const uint8_t kAlphaLevels[4] = {0, 85, 170, 255};

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
                   const uint8_t* mask = nullptr,
                   const RGB565* tint = nullptr) {

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

        (void)mask;  // alpha is inline in RLE stream for new formats

        // ── FMT_A8 (0): grayscale raw ───────────────────────────
        if (fmt == 0) {
            const uint8_t* gray = (const uint8_t*)src;
            uint16_t* tile = mTile->buffer();
            int tStride = mTile->stride();
            uint16_t tintLut[256];
            if (tint) {
                uint32_t tr = (tint->value >> 11) & 0x1F;
                uint32_t tg = (tint->value >> 5)  & 0x3F;
                uint32_t tb =  tint->value        & 0x1F;
                for (int i = 0; i < 256; i++) {
                    uint32_t r = (tr * i) / 255, g = (tg * i) / 255, b = (tb * i) / 255;
                    if (r > 0x1F) r = 0x1F; if (g > 0x3F) g = 0x3F; if (b > 0x1F) b = 0x1F;
                    tintLut[i] = (uint16_t)((r << 11) | (g << 5) | b);
                }
            }
            for (int y = 0; y < copyH; y++) {
                const uint8_t* srow = gray + (srcOffY + y) * srcW + srcOffX;
                uint16_t* drow = tile + (ty0 + y) * tStride + tx0;
                for (int x = 0; x < copyW; x++) {
                    uint8_t g = srow[x];
                    drow[x] = tint ? tintLut[g]
                         : (uint16_t)(((g >> 3) & 0x1F) << 11 | ((g >> 2) & 0x3F) << 5 | ((g >> 3) & 0x1F));
                }
            }
            return;
        }

        // ── FMT_A8_RLE (1): grayscale + RLE ─────────────────────
        if (fmt == 1) {
            const uint8_t* rle = (const uint8_t*)src;
            const uint32_t* off = (const uint32_t*)rle;
            uint16_t* tile = mTile->buffer();
            int tStride = mTile->stride();
            const int visL = srcOffX;
            const int visR = srcOffX + copyW;
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
                const uint8_t* p = rle + off[srcOffY + y];
                uint16_t* dstRow = tile + (ty0 + y) * tStride + tx0;
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
                        if (cnt && ((uintptr_t)dp & 3)) { *dp++ = c; --cnt; }
                        uint32_t c32 = ((uint32_t)c << 16) | c;
                        uint32_t* d4 = (uint32_t*)dp;
                        int wc = cnt >> 1;
                        for (int i = 0; i < wc; i++) d4[i] = c32;
                        if (cnt & 1) dp[cnt - 1] = c;
                    }
                    px = runR;
                }
            }
            return;
        }

        // ── FMT_PAL8 (2): palette + raw index ───────────────────
        if (fmt == 2) {
            const uint16_t* pal = (const uint16_t*)src;
            const uint8_t*  idx = (const uint8_t*)src + 512;
            uint16_t* tile = mTile->buffer();
            int tStride = mTile->stride();
            for (int y = 0; y < copyH; y++) {
                const uint8_t* srow = idx + (srcOffY + y) * srcW + srcOffX;
                uint16_t* drow = tile + (ty0 + y) * tStride + tx0;
                for (int x = 0; x < copyW; x++) drow[x] = pal[srow[x]];
            }
            return;
        }

        // ── FMT_PAL8_RLE (3): palette + RLE ─────────────────────
        if (fmt == 3) {
            const uint16_t* pal = (const uint16_t*)src;
            const uint8_t*  rle = (const uint8_t*)src + 512;
            const uint32_t* off = (const uint32_t*)rle;
            uint16_t* tile = mTile->buffer();
            int tStride = mTile->stride();
            const int visL = srcOffX, visR = srcOffX + copyW;
            for (int y = 0; y < copyH; y++) {
                const uint8_t* p = rle + off[srcOffY + y];
                uint16_t* dstRow = tile + (ty0 + y) * tStride + tx0;
                int px = 0;
                while (px < srcW) {
                    uint8_t ix = *p++; uint8_t len = *p++;
                    int n = (int)len + 1;
                    int runR = px + n, cl = px < visL ? visL : px, cr = runR > visR ? visR : runR;
                    if (cr > cl) {
                        uint16_t c  = pal[ix];
                        uint16_t* dp = dstRow + (cl - visL); int cnt = cr - cl;
                        if (cnt && ((uintptr_t)dp & 3)) { *dp++ = c; --cnt; }
                        uint32_t c32 = ((uint32_t)c << 16) | c;
                        uint32_t* d4 = (uint32_t*)dp;
                        int wc = cnt >> 1;
                        for (int i = 0; i < wc; i++) d4[i] = c32;
                        if (cnt & 1) dp[cnt - 1] = c;
                    }
                    px = runR;
                }
            }
            return;
        }

        // ── FMT_PAL8_ALPHA (4): palette + raw index + raw alpha ──
        if (fmt == 4) {
            const uint16_t* pal = (const uint16_t*)src;
            const uint8_t*  idx = (const uint8_t*)src + 512;
            const uint8_t*  alp = idx + (uint32_t)srcW * srcH;
            uint16_t* tile = mTile->buffer();
            int tStride = mTile->stride();
            for (int y = 0; y < copyH; y++) {
                int sy = srcOffY + y;
                const uint8_t* srow = idx + sy * srcW + srcOffX;
                const uint8_t* arow = alp + sy * srcW + srcOffX;
                uint16_t* drow = tile + (ty0 + y) * tStride + tx0;
                for (int x = 0; x < copyW; x++) {
                    uint8_t a = arow[x];
                    if (a == 255) { drow[x] = pal[srow[x]]; }
                    else if (a > 0) {
                        uint16_t s = pal[srow[x]], d = drow[x];
                        uint32_t ia = 255 - a;
                        uint16_t r = (uint16_t)((((s >> 11) & 0x1F) * a + ((d >> 11) & 0x1F) * ia) / 255) << 11;
                        uint16_t g = (uint16_t)((((s >> 5)  & 0x3F) * a + ((d >> 5)  & 0x3F) * ia) / 255) << 5;
                        uint16_t b = (uint16_t)((( s        & 0x1F) * a + ( d        & 0x1F) * ia) / 255);
                        drow[x] = r | g | b;
                    }
                }
            }
            return;
        }

        // ── FMT_PAL8_ALPHA_RLE (5): single-pass, alpha=0 skips write ──
        if (fmt == 5) {
            const uint16_t* pal = (const uint16_t*)src;
            const uint8_t*  rle = (const uint8_t*)src + 512;
            const uint32_t* off = (const uint32_t*)rle;
            uint16_t* tile = mTile->buffer();
            int tStride = mTile->stride();
            const int visL = srcOffX, visR = srcOffX + copyW;
            for (int y = 0; y < copyH; y++) {
                const uint8_t* p = rle + off[srcOffY + y];
                uint16_t* dstRow = tile + (ty0 + y) * tStride + tx0;
                int px = 0;
                while (px < srcW) {
                    uint8_t ix = *p++; uint8_t len = *p++;
                    if (len & 0x80) {
                        // Alpha run: per-pixel blend (max 8)
                        int n = (len & 0x07) + 1;
                        uint8_t a = kAlphaLevels[(len >> 4) & 0x03];
                        int runR = px + n;
                        int cl = px < visL ? visL : px;
                        int cr = runR > visR ? visR : runR;
                        while (cl < cr) {
                            if (a > 0) {
                                uint16_t s = pal[ix];
                                uint16_t d = dstRow[cl - visL];
                                uint32_t ia = 255 - a;
                                uint16_t r = (uint16_t)((((s >> 11) & 0x1F) * a + ((d >> 11) & 0x1F) * ia) / 255) << 11;
                                uint16_t g = (uint16_t)((((s >> 5)  & 0x3F) * a + ((d >> 5)  & 0x3F) * ia) / 255) << 5;
                                uint16_t b = (uint16_t)((( s        & 0x1F) * a + ( d        & 0x1F) * ia) / 255);
                                dstRow[cl - visL] = r | g | b;
                            }
                            cl++;
                        }
                        px = runR;
                    } else {
                        // Opaque run: word-fill (max 128)
                        int n = (len & 0x7F) + 1;
                        int runR = px + n;
                        int cl = px < visL ? visL : px;
                        int cr = runR > visR ? visR : runR;
                        if (cr > cl) {
                            uint16_t c  = pal[ix];
                            uint16_t* dp = dstRow + (cl - visL); int cnt = cr - cl;
                            if (cnt && ((uintptr_t)dp & 3)) { *dp++ = c; --cnt; }
                            uint32_t c32 = ((uint32_t)c << 16) | c;
                            uint32_t* d4 = (uint32_t*)dp;
                            int wc = cnt >> 1;
                            for (int i = 0; i < wc; i++) d4[i] = c32;
                            if (cnt & 1) dp[cnt - 1] = c;
                        }
                        px = runR;
                    }
                }
            }
            return;
        }

        // ── FMT_RGB565_RLE (6): direct color RLE, no palette ─────
        if (fmt == 6 && mAlpha == 255 && !tint && !mask) {
            const uint8_t* rle = (const uint8_t*)src;
            const uint32_t* off = (const uint32_t*)rle;
            uint16_t* tile = mTile->buffer();
            int tStride = mTile->stride();
            const int visL = srcOffX, visR = srcOffX + copyW;
            const uint8_t* p = rle + off[srcOffY];
            for (int y = 0; y < copyH; y++) {
                uint16_t* dstRow = tile + (ty0 + y) * tStride + tx0;
                int px = 0;
                while (px < srcW) {
                    uint8_t cmd = *p++;
                    int n = (cmd & 0x7F) + 1;
                    int runR = px + n;
                    int cl = px < visL ? visL : px;
                    int cr = runR > visR ? visR : runR;
                    if (cmd & 0x80) {
                        // literal: n distinct pixels
                        if (cr > cl)
                            memcpy(dstRow + (cl - visL),
                                   (const uint16_t*)p + (cl - px),
                                   (size_t)(cr - cl) * 2);
                        p += n * 2;
                    } else {
                        // run: one color ×n → 32-bit word fill
                        uint16_t c = *(const uint16_t*)p; p += 2;
                        if (cr > cl) {
                            uint16_t* dp = dstRow + (cl - visL);
                            int cnt = cr - cl;
                            if (cnt && ((uintptr_t)dp & 3)) { *dp++ = c; --cnt; }
                            uint32_t c32 = ((uint32_t)c << 16) | c;
                            uint32_t* d4 = (uint32_t*)dp;
                            int wc = cnt >> 1;
                            for (int i = 0; i < wc; i++) d4[i] = c32;
                            if (cnt & 1) dp[cnt - 1] = c;
                        }
                    }
                    px = runR;
                }
            }
            return;
        }

        // Fallback: unsupported format → fill magenta
        uint16_t* tile = mTile->buffer();
        int tStride = mTile->stride();
        for (int y = 0; y < copyH; y++) {
            uint16_t* row = tile + (ty0 + y) * tStride + tx0;
            for (int x = 0; x < copyW; x++) row[x] = 0xF81F;
        }
    }

    // ── drawImageRotated (deci-degree core) ───────────────────────

    void drawImageRotated(const void* src, int fmt,
                          int srcW, int srcH,
                          int dx, int dy,
                          int rotCx, int rotCy,
                          int16_t angleDeg,
                          const uint8_t* mask   = nullptr,
                          const RGB565* tint   = nullptr,
                          Tile* rotBuffer      = nullptr) {
        drawImageRotatedDeci(src, fmt, srcW, srcH, dx, dy, rotCx, rotCy,
                             (int)angleDeg * 10, mask, tint, rotBuffer);
    }

    void drawImageRotatedDeci(const void* src, int fmt,
                          int srcW, int srcH,
                          int dx, int dy,
                          int rotCx, int rotCy,
                          int angleDeci,
                          const uint8_t* mask   = nullptr,
                          const RGB565* tint   = nullptr,
                          Tile* rotBuffer      = nullptr) {

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

        (void)fmt; (void)mask; (void)tint;
        // For RLE formats, NN sampling is done inline below

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

                    // RLE formats (1,3,5): decode from RLE stream
                    if (fmt == 1 || fmt == 3 || fmt == 5) {
                        const uint8_t* rle;
                        const uint16_t* pal = nullptr;
                        if (fmt == 1) { rle = (const uint8_t*)src; }
                        else          { pal = (const uint16_t*)src; rle = (const uint8_t*)src + 512; }
                        const uint32_t* off = (const uint32_t*)rle;
                        const uint8_t* p = rle + off[sy];
                        int px = 0;
                        while (px <= sx) {
                            uint8_t val = *p++;
                            uint8_t len = *p++;
                            int n;
                            if (fmt == 5 && (len & 0x80))      n = (len & 0x07) + 1;
                            else if (fmt == 5)                  n = (len & 0x7F) + 1;
                            else                               n = (int)len + 1;
                            if (px + n > sx) {
                                if (fmt == 1) {
                                    uint8_t g = val;
                                    if (tint) {
                                        uint32_t tr = (tint->value >> 11) & 0x1F, tg = (tint->value >> 5) & 0x3F, tb = tint->value & 0x1F;
                                        uint32_t r = (tr * g) / 255, gg = (tg * g) / 255, b = (tb * g) / 255;
                                        if (r > 0x1F) r = 0x1F; if (gg > 0x3F) gg = 0x3F; if (b > 0x1F) b = 0x1F;
                                        s = (uint16_t)((r << 11) | (gg << 5) | b);
                                    } else { uint32_t g5 = (g >> 3) & 0x1F, g6 = (g >> 2) & 0x3F; s = (uint16_t)((g5 << 11) | (g6 << 5) | g5); }
                                } else { s = pal[val]; }
                                if (fmt == 5 && (len & 0x80)) pixelA = kAlphaLevels[(len >> 4) & 0x03];
                                break;
                            }
                            px += n;
                        }
                    } else if (fmt == 0 || fmt == 2 || fmt == 4) {
                        // Raw formats: direct index into pixel array
                        if (fmt == 0) {
                            uint8_t g = ((const uint8_t*)src)[sy * srcW + sx];
                            if (tint) {
                                uint32_t tr = (tint->value >> 11) & 0x1F, tg = (tint->value >> 5) & 0x3F, tb = tint->value & 0x1F;
                                uint32_t r = (tr * g) / 255, gg = (tg * g) / 255, b = (tb * g) / 255;
                                if (r > 0x1F) r = 0x1F; if (gg > 0x3F) gg = 0x3F; if (b > 0x1F) b = 0x1F;
                                s = (uint16_t)((r << 11) | (gg << 5) | b);
                            } else { uint32_t g5 = (g >> 3) & 0x1F, g6 = (g >> 2) & 0x3F; s = (uint16_t)((g5 << 11) | (g6 << 5) | g5); }
                        } else {
                            const uint16_t* pal = (const uint16_t*)src;
                            const uint8_t*  idx = (const uint8_t*)src + 512;
                            s = pal[idx[sy * srcW + sx]];
                            if (fmt == 4) {
                                const uint8_t* alp = idx + (uint32_t)srcW * srcH;
                                pixelA = alp[sy * srcW + sx];
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
                                uint32_t ia = 255 - a;
                                uint16_t r = (uint16_t)((((s >> 11) & 0x1F) * a + ((d >> 11) & 0x1F) * ia) / 255) << 11;
                                uint16_t g = (uint16_t)((((s >> 5)  & 0x3F) * a + ((d >> 5)  & 0x3F) * ia) / 255) << 5;
                                uint16_t b = (uint16_t)((( s        & 0x1F) * a + ( d        & 0x1F) * ia) / 255);
                                dstRow[tdstx] = r | g | b;
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
                        uint32_t a  = mAlpha;
                        uint32_t ia = 255 - a;
                        uint16_t d = dstRow[tdstx];
                        uint16_t r = (uint16_t)((((s >> 11) & 0x1F) * a + ((d >> 11) & 0x1F) * ia) / 255) << 11;
                        uint16_t g = (uint16_t)((((s >> 5)  & 0x3F) * a + ((d >> 5)  & 0x3F) * ia) / 255) << 5;
                        uint16_t b = (uint16_t)((( s        & 0x1F) * a + ( d        & 0x1F) * ia) / 255);
                        dstRow[tdstx] = r | g | b;
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
            uint32_t a  = mAlpha;
            uint32_t ia = 255 - a;
            for (int y = 0; y < copyH; y++) {
                uint16_t*       dstRow = mTile->buffer() + (ty0 + y) * mTile->stride() + tx0;
                const uint16_t* srcRow = src.buffer() + (srcOffY + y) * src.stride() + srcOffX;
                for (int x = 0; x < copyW; x++) {
                    uint16_t s = srcRow[x];
                    uint16_t d = dstRow[x];
                    uint16_t r = (uint16_t)((((s >> 11) & 0x1F) * a + ((d >> 11) & 0x1F) * ia) / 255) << 11;
                    uint16_t g = (uint16_t)((((s >> 5)  & 0x3F) * a + ((d >> 5)  & 0x3F) * ia) / 255) << 5;
                    uint16_t b = (uint16_t)((( s        & 0x1F) * a + ( d        & 0x1F) * ia) / 255);
                    dstRow[x] = r | g | b;
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
