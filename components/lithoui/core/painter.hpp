#pragma once
#include "tile.hpp"
#include "litho_core.h"
#include "res_images.h"
#include <stdio.h>
#include <string.h>
#ifndef DWT_CYCCNT
#define DWT_CYCCNT (*(volatile uint32_t*)0xE0001004UL)
#endif

// A/B test: 1 = disable RLE row offset table (sequentially scan to the start
// row), 0 = O(1) table seek. Default 0; flip to 1 to measure the table's value.
// Measured draw — value scales with skip-rows × cmd-bytes/row:
//   12×100px icons (RLE 14%, ~49 B/row):  5762 vs  6245 µs  (+8.4%)
//   360×360 avatar (RLE 51%, 366 B/row): 10748 vs 37828 µs  (+252%, O(n²) skip)
#define LITHO_RLE_NO_OFFSET_TABLE 0

namespace litho {

// Image formats — defined in res_images.h, kept in sync:
//   FMT_RGB565    = 0   opaque
//   FMT_RGB565_A8 = 1   RGB + alpha mask
//   FMT_A8        = 2   single-channel alpha (fill=black, tintable)

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
            // 32-bit word fill (2 pixels per store) — same speed as benchmark ram S→S
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

        // Fast path: opaque RGB565, no alpha/tint/mask.
        // 8x unrolled word copy Flash→tile (.ramfunc → SRAM so fetch
        // never fights data on QSPI).  One cache line (32 B) per iteration.
        if (!tint && fmt == 0 && mAlpha == 255 && !mask) {
            uint16_t* tile    = mTile->buffer();
            int       tStride = mTile->stride();
            for (int y = 0; y < copyH; y++) {
                const uint32_t* sp = (const uint32_t*)((const uint16_t*)src + (srcOffY + y) * srcW + srcOffX);
                uint32_t*       dp = (uint32_t*)(tile + (ty0 + y) * tStride + tx0);
                uint32_t w = (uint32_t)copyW / 2;
                uint32_t i = 0;
                for (; i + 8 <= w; i += 8) {
                    dp[i+0]=sp[i+0]; dp[i+1]=sp[i+1]; dp[i+2]=sp[i+2]; dp[i+3]=sp[i+3];
                    dp[i+4]=sp[i+4]; dp[i+5]=sp[i+5]; dp[i+6]=sp[i+6]; dp[i+7]=sp[i+7];
                }
                for (; i < w; i++) dp[i] = sp[i];
                if (copyW & 1) ((uint16_t*)dp)[copyW-1] = ((const uint16_t*)sp)[copyW-1];
            }
            return;
        }

        // RLE decompress path — fmt=3 (FMT_RGB565_RLE), opaque only
        // Format: [uint32_t off[h]][compressed rows], each row self-delimiting
        if (fmt == 3 && mAlpha == 255 && !tint && !mask) {
            const uint8_t* rle = (const uint8_t*)src;
            const uint32_t* off = (const uint32_t*)rle;
            uint16_t* tile = mTile->buffer();
            int tStride = mTile->stride();

            // Jump to starting row: O(1) table seek, or (A/B test) simulate
            // having no table by sequentially scanning every preceding row.
#if LITHO_RLE_NO_OFFSET_TABLE
            (void)off;
            const uint8_t* p = rle + srcH * 4;   // data starts past the table
            for (int skip = 0; skip < srcOffY; skip++) {
                int spx = 0;
                while (spx < srcW) {
                    uint8_t cmd = *p++;
                    int n = (cmd & 0x7F) + 1;
                    p += (cmd & 0x80) ? n * 2 : 2;
                    spx += n;
                }
            }
#else
            const uint8_t* p = rle + off[srcOffY];
#endif
            // Clip the visible source x-span once: [visL, visR) maps to dst
            // [0, copyW). Each run/literal is intersected with it ONCE (not
            // per-pixel); runs word-fill, literals memcpy.
            const int visL = srcOffX;
            const int visR = srcOffX + copyW;
            for (int y = 0; y < copyH; y++) {
                uint16_t* dstRow = tile + (ty0 + y) * tStride + tx0;
                int px = 0;
                while (px < srcW) {
                    uint8_t cmd = *p++;
                    int n = (cmd & 0x7F) + 1;
                    int runR = px + n;
                    int cl = px   < visL ? visL : px;     // run ∩ visible span
                    int cr = runR > visR ? visR : runR;
                    if (cmd & 0x80) {
                        // literal: n distinct pixels at p
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

        // PAL8 RLE — fmt=4: [512B RGB565 palette][off table][byte-RLE 8-bit idx]
        // Same row-offset RLE as fmt 3, but values are palette indices: a run
        // word-fills palette[idx]; a literal does a per-pixel palette lookup.
        if (fmt == 4 && mAlpha == 255 && !tint && !mask) {
            const uint16_t* pal = (const uint16_t*)src;
            const uint8_t*  rle = (const uint8_t*)src + 512;
            const uint32_t* off = (const uint32_t*)rle;
            uint16_t* tile = mTile->buffer();
            int tStride = mTile->stride();
            const int visL = srcOffX;
            const int visR = srcOffX + copyW;
            const uint8_t* p = rle + off[srcOffY];
            for (int y = 0; y < copyH; y++) {
                uint16_t* dstRow = tile + (ty0 + y) * tStride + tx0;
                int px = 0;
                while (px < srcW) {
                    uint8_t cmd = *p++;
                    int n = (cmd & 0x7F) + 1;
                    int runR = px + n;
                    int cl = px   < visL ? visL : px;
                    int cr = runR > visR ? visR : runR;
                    if (cmd & 0x80) {
                        // literal: n index bytes → per-pixel palette lookup
                        if (cr > cl) {
                            const uint8_t* sp = p + (cl - px);
                            uint16_t* dp = dstRow + (cl - visL);
                            int cnt = cr - cl;
                            for (int k = 0; k < cnt; k++) dp[k] = pal[sp[k]];
                        }
                        p += n;
                    } else {
                        // run: one index ×n → palette → 32-bit word fill
                        uint16_t c = pal[*p]; p++;
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

        // General path
        uint16_t* dstBuf    = mTile->buffer();
        uint32_t  viewA     = mAlpha;
        int       dstStride = mTile->stride();

        uint32_t tintR = 0, tintG = 0, tintB = 0;
        bool hasTint = (tint != nullptr);
        if (hasTint) {
            tintR = (tint->value >> 11) & 0x1F;
            tintG = (tint->value >> 5)  & 0x3F;
            tintB =  tint->value        & 0x1F;
        }

        const uint16_t* src16 = (const uint16_t*)src;
        const uint8_t*  src8  = (const uint8_t*)src;

        for (int y = 0; y < copyH; y++) {
            int srcY = srcOffY + y;
            uint16_t* dstRow = dstBuf + (ty0 + y) * dstStride + tx0;

            for (int x = 0; x < copyW; x++) {
                int srcX = srcOffX + x;
                uint8_t channelA = 255;
                uint16_t s;
                switch (fmt) {
                case 0: case 1:
                    s = src16[srcY * srcW + srcX]; break;
                case 2:
                    // A8: pixel is alpha, fill color = tint or default black
                    channelA = src8[srcY * srcW + srcX];
                    s = hasTint ? tint->value : 0;
                    break;
                default: s = 0; break;
                }

                if (hasTint && fmt != 2) {
                    uint16_t sr = (s >> 11) & 0x1F;
                    uint16_t sg = (s >> 5)  & 0x3F;
                    uint16_t sb =  s        & 0x1F;
                    s = (((sr + tintR) / 2) << 11) |
                         (((sg + tintG) / 2) << 5)  |
                          ((sb + tintB) / 2);
                }

                uint32_t pixelA = viewA;
                if (fmt == 2)
                    pixelA = (uint32_t)channelA * viewA / 255;
                if (mask && (fmt == 1))
                    pixelA = mask[srcY * srcW + srcX] * viewA / 255;

                uint16_t d = dstRow[x];
                if (pixelA == 255) {
                    dstRow[x] = s;
                } else if (pixelA > 0) {
                    uint32_t ia = 255 - pixelA;
                    uint16_t r = (uint16_t)((((s >> 11) & 0x1F) * pixelA + ((d >> 11) & 0x1F) * ia) / 255) << 11;
                    uint16_t g = (uint16_t)((((s >> 5)  & 0x3F) * pixelA + ((d >> 5)  & 0x3F) * ia) / 255) << 5;
                    uint16_t b = (uint16_t)((( s        & 0x1F) * pixelA + ( d        & 0x1F) * ia) / 255);
                    dstRow[x] = r | g | b;
                }
            }
        }
    }

    // ── drawImageRotated (all rotation, incl. 90° steps) ──────────
    //
    // Degree entry point (kept for callers using whole degrees). Delegates
    // to the deci-degree core; integer degrees give identical results
    // because sinDeci(deg*10) == sinDeg(deg) (no interpolation at f=0).
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

    // Deci-degree core: angle in 1/10°, so a sweeping hand can move in
    // sub-degree steps (smooth) instead of 1° jumps.
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

        // ── Q16 sin/cos: 90° multiples (0/900/1800/2700 deci) are exact ──
        int32_t cosA, sinA;
        int outW, outH;
        bool useBilinear;  // bilinear only for arbitrary angles
        switch (angleDeci) {
        case 0:    cosA = 65536;  sinA = 0;       outW = srcW; outH = srcH; useBilinear = false; break;
        case 900:  cosA = 0;      sinA = 65536;   outW = srcH; outH = srcW; useBilinear = false; break;
        case 1800: cosA = -65536; sinA = 0;       outW = srcW; outH = srcH; useBilinear = false; break;
        case 2700: cosA = 0;      sinA = -65536;  outW = srcH; outH = srcW; useBilinear = false; break;
        default:
            cosA = (int32_t)cosDeci(angleDeci) << 1;
            sinA = (int32_t)sinDeci(angleDeci) << 1;
            outW = outH = 0; // computed from bounding box below
            useBilinear = true;
            break;
        }

        // ── rotated bounding-box (works for all angles incl. 90°) ──
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

        // DDA step vectors (Q16)
        int32_t stepSX_dx =  cosA;
        int32_t stepSY_dx = -sinA;
        int32_t stepSX_dy =  sinA;
        int32_t stepSY_dy =  cosA;

        // ── half-pixel offset ────────────────────────────────────────
        // Two independent ½-pixel corrections fold into baseSX/baseSY:
        //
        //  (1) OUTPUT side: pixel (ox,oy) occupies [ox,ox+1)×[oy,oy+1);
        //      its centre is at (ox+0.5, oy+0.5).  The bounding box is
        //      built from image corners (not pixel centres), so we shift
        //      the scan origin by +½ px in output space and map it back:
        //        R⁻¹(½,½) in Q16 = (½·cos + ½·sin, -½·sin + ½·cos).
        //
        //  (2) SOURCE side: the fetch treats an integer source coordinate
        //      as a pixel CENTRE (sx = curSX>>16, frac = curSX&0xFFFF is
        //      the weight toward sx+1).  The mapping above lands on the
        //      corner grid, so we subtract ½ px in source space to hit
        //      centres.  Without this the sampler is biased by +½ px and
        //      lattice-aligned angles (0/90/180/270°) come out at frac
        //      0.5 — blurred and shifted vs their crisp fast-path — which
        //      makes a continuously-rotating image flicker/jump every 90°.
        //
        // Since cosA/sinA are already Q16, ½·v = v / 2 and ½ px = 32768:
        int32_t const halfX = (cosA + sinA) / 2 - 32768;
        int32_t const halfY = (cosA - sinA) / 2 - 32768;

        // Row-start for output pixel (0,0) → source Q16
        // source = pivot + R⁻¹( (minX+½, minY+½) )
        int32_t baseSX = (int32_t)rotCx * 65536
                       + (int32_t)minX * cosA
                       + (int32_t)minY * sinA
                       + halfX;
        int32_t baseSY = (int32_t)rotCy * 65536
                       - (int32_t)minX * sinA
                       + (int32_t)minY * cosA
                       + halfY;

        // Coordinates outside the source are intentionally left unclamped so
        // the scan can skip transparent/outside parts of the rotated AABB.

        // Decide: rotation buffer or direct path
        bool useRotBuf = (rotBuffer &&
                          rotBuffer->width()  >= outW &&
                          rotBuffer->height() >= outH);

        const uint16_t* src16 = (const uint16_t*)src;
        const uint8_t*  src8  = (const uint8_t*)src;

        uint32_t tintR = 0, tintG = 0, tintB = 0;
        bool hasTint = (tint != nullptr);
        if (hasTint) {
            tintR = (tint->value >> 11) & 0x1F;
            tintG = (tint->value >> 5)  & 0x3F;
            tintB =  tint->value        & 0x1F;
        }

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

                if (sx >= -1 && sx < srcW && sy >= -1 && sy < srcH) {

                uint16_t s;
                uint32_t pixelA = 255;

                if (useBilinear) {
                // ── bilinear fetch ─────────────────────────────
                uint32_t fx = (uint32_t)(curSX & 0xFFFF);
                uint32_t fy = (uint32_t)(curSY & 0xFFFF);

                // clamp neighbour indices for border pixels
                int sx0 = (sx < 0) ? 0 : (sx >= srcW ? srcW-1 : sx);
                int sy0 = (sy < 0) ? 0 : (sy >= srcH ? srcH-1 : sy);
                int sx1 = (sx+1 < srcW) ? sx+1 : (sx < 0 ? 0 : srcW-1);
                int sy1 = (sy+1 < srcH) ? sy+1 : (sy < 0 ? 0 : srcH-1);
                if (sx < 0) fx = 0;
                if (sy < 0) fy = 0;

                uint32_t ifx = 65536 - fx;
                uint32_t ify = 65536 - fy;
                uint32_t w00 = (ifx * ify) >> 16;
                uint32_t w10 = ( fx * ify) >> 16;
                uint32_t w01 = (ifx *  fy) >> 16;
                // w11 as the remainder so the four weights partition unity
                // exactly (Σw = 65536).  Truncating each (f·f)>>16 lets the
                // sum fall below 65536, which drops a fully-opaque pixel's
                // coverage to 254 → the final composite then blends in 1/255
                // of the background and dims every interpolated pixel by
                // ~1 LSB/channel (≈6 luma) at most angles.
                uint32_t w11 = 65536 - w00 - w10 - w01;

                uint16_t p00, p10, p01, p11;
                uint8_t  a00=255, a10=255, a01=255, a11=255;

                switch (fmt) {
                case 0:
                    p00=src16[sy0*srcW+sx0]; p10=src16[sy0*srcW+sx1];
                    p01=src16[sy1*srcW+sx0]; p11=src16[sy1*srcW+sx1];
                    break;
                case 1:
                    p00=src16[sy0*srcW+sx0]; p10=src16[sy0*srcW+sx1];
                    p01=src16[sy1*srcW+sx0]; p11=src16[sy1*srcW+sx1];
                    if (mask) {
                        a00=mask[sy0*srcW+sx0]; a10=mask[sy0*srcW+sx1];
                        a01=mask[sy1*srcW+sx0]; a11=mask[sy1*srcW+sx1];
                    }
                    break;
                case 2:
                    a00=src8[sy0*srcW+sx0]; a10=src8[sy0*srcW+sx1];
                    a01=src8[sy1*srcW+sx0]; a11=src8[sy1*srcW+sx1];
                    p00=p10=p01=p11=hasTint?tint->value:0;
                    break;
                default: p00=p10=p01=p11=0; break;
                }

                // ── premultiplied-alpha blend ───────────────────
                // Weight each texel's RGB by its OWN coverage (wᵢ·aᵢ),
                // then normalise by Σwᵢ·aᵢ.  A transparent texel (aᵢ=0)
                // contributes nothing, so its (usually black) RGB can no
                // longer darken the edge — this removes the dark fringe
                // that made off-axis frames dimmer and produced a
                // brightness flicker every 90° during a spin.  For opaque
                // pixels (aᵢ=255) the aᵢ factor cancels and this is exactly
                // the previous positional bilinear (bit-identical).
                if ((w00|w10|w01|w11) != 0) {
                    uint32_t aw00=w00*a00, aw10=w10*a10,
                             aw01=w01*a01, aw11=w11*a11;
                    uint32_t awSum = aw00+aw10+aw01+aw11;   // Σ wᵢ·aᵢ
                    if (awSum != 0) {
                        uint32_t r0=(p00>>11)&0x1F,g0=(p00>>5)&0x3F,b0=p00&0x1F;
                        uint32_t r1=(p10>>11)&0x1F,g1=(p10>>5)&0x3F,b1=p10&0x1F;
                        uint32_t r2=(p01>>11)&0x1F,g2=(p01>>5)&0x3F,b2=p01&0x1F;
                        uint32_t r3=(p11>>11)&0x1F,g3=(p11>>5)&0x3F,b3=p11&0x1F;
                        // Round-to-nearest (+½·Σw) instead of truncating:
                        // truncation loses ~½ LSB/channel on every
                        // interpolated pixel, systematically dimming the
                        // whole body at off-axis angles vs the crisp
                        // (un-interpolated) lattice-aligned frames.
                        uint32_t half = awSum >> 1;
                        uint32_t rb=(r0*aw00+r1*aw10+r2*aw01+r3*aw11+half)/awSum;
                        uint32_t gb=(g0*aw00+g1*aw10+g2*aw01+g3*aw11+half)/awSum;
                        uint32_t bb=(b0*aw00+b1*aw10+b2*aw01+b3*aw11+half)/awSum;
                        if(rb>0x1F)rb=0x1F; if(gb>0x3F)gb=0x3F; if(bb>0x1F)bb=0x1F;
                        s = (uint16_t)((rb<<11)|(gb<<5)|bb);
                    } else {
                        s = 0;   // fully transparent → colour unused (pixelA=0)
                    }

                    if (fmt==2 || (mask && fmt==1)) {
                        pixelA = awSum>>16;   // (Σ wᵢ·aᵢ)>>16, as before
                        if(pixelA>255) pixelA=255;
                    }
                } else {
                    s = p00;
                    if (fmt==2 || (mask && fmt==1)) pixelA = a00;
                }

                } else {
                // ── nearest-neighbour fetch ────────────────────
                if (sx >= 0 && sx < srcW && sy >= 0 && sy < srcH) {
                    uint8_t ca = 255;
                    switch (fmt) {
                    case 0: case 1:
                        s = src16[sy * srcW + sx]; break;
                    case 2:
                        ca = src8[sy * srcW + sx];
                        s = hasTint ? tint->value : 0;
                        break;
                    default: s = 0; break;
                    }
                    if (fmt == 2)
                        pixelA = (uint32_t)ca;
                    if (mask && (fmt == 1))
                        pixelA = mask[sy * srcW + sx];
                } else {
                    goto next_pixel;  // out of bounds → skip
                }
                }

                // ── apply tint (common to NN and bilinear) ───────
                if (hasTint && fmt != 2) {
                    uint16_t sr = (s >> 11) & 0x1F;
                    uint16_t sg = (s >> 5)  & 0x3F;
                    uint16_t sb =  s        & 0x1F;
                    s = (uint16_t)((((sr + tintR) / 2) << 11) |
                                   (((sg + tintG) / 2) << 5)  |
                                    ((sb + tintB) / 2));
                }

                if (useRotBuf) {
                    dstRow[x] = s;
                } else {
                    int sdstx = dx + mScreenX + x;
                    if (sdstx < mClipL || sdstx >= mClipR) goto next_pixel;
                    int tdstx = sdstx - mTileOrgX;
                    if (tdstx < 0 || tdstx >= mTile->width()) goto next_pixel;

                    uint16_t d = dstRow[tdstx];
                    if (pixelA == 255) {
                        dstRow[tdstx] = s;
                    } else if (pixelA > 0) {
                        uint32_t ia = 255 - pixelA;
                        uint16_t r = (uint16_t)((((s >> 11) & 0x1F) * pixelA + ((d >> 11) & 0x1F) * ia) / 255) << 11;
                        uint16_t g = (uint16_t)((((s >> 5)  & 0x3F) * pixelA + ((d >> 5)  & 0x3F) * ia) / 255) << 5;
                        uint16_t b = (uint16_t)((( s        & 0x1F) * pixelA + ( d        & 0x1F) * ia) / 255);
                        dstRow[tdstx] = r | g | b;
                    }
                }
                } // if (sx in range)

            next_pixel:
                curSX += stepSX_dx;
                curSY += stepSY_dx;
            }
            baseSX += stepSX_dy;
            baseSY += stepSY_dy;
        }

        // Blit rotation buffer to real destination.
        // WARNING: the DDA wrote into rotBuf using rotBuffer->stride()
        // as the row stride, NOT outW.  We must use the buffer's actual
        // stride when reading, otherwise rows beyond the first are
        // misaligned.
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
