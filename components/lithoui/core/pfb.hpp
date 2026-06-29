#pragma once
#include "tile.hpp"
#include "region.hpp"
#include "painter.hpp"
#include <stdint.h>
#include <stdio.h>
#ifndef DWT_CYCCNT
#define DWT_CYCCNT (*(volatile uint32_t*)0xE0001004UL)
#endif

namespace litho {

// PFB (Partial Frame Buffer) — divides the screen into fixed-size blocks and
// manages a small tile buffer pool.  Rendering iterates over the blocks that
// cover a dirty region; each block gets a Painter configured with the correct
// tile, screen origin, and clip.
//
// Tile pool uses an internal free-list stack for O(1) acquire / release.
// Pool exhaustion (should not happen with correct sizing) logs a warning
// and falls back to tile[0] — visual corruption is likely.

class PFB {
public:
    bool init(int blockW, int blockH, int poolSize,
              int screenW, int screenH) {
        mBlockW   = blockW;
        mBlockH   = blockH;
        mScreenW  = screenW;
        mScreenH  = screenH;
        mCols     = (screenW + blockW - 1) / blockW;
        mRows     = (screenH + blockH - 1) / blockH;

        int pixPerBlock = blockW * blockH;
        mPoolSize  = poolSize;
        mPoolTiles = new Tile[poolSize];
        mPoolBufs  = new uint16_t[pixPerBlock * poolSize];

        // Free-list stack — all tiles start available
        mFreeList  = new int[poolSize];
        for (int i = 0; i < poolSize; i++) {
            mPoolTiles[i].attach(mPoolBufs + i * pixPerBlock,
                                  blockW, blockH);
            mFreeList[i] = i;
        }
        mFreeCount = poolSize;

        // PFB initialized
        return true;
    }

    ~PFB() {
        delete[] mPoolTiles;
        delete[] mPoolBufs;
        delete[] mFreeList;
    }

    int blockW() const { return mBlockW; }
    int blockH() const { return mBlockH; }
    int cols()   const { return mCols; }
    int rows()   const { return mRows; }

    // Per-frame stats
    void clearStats()  { mStatDraw=0; mStatSetup=0; mStatXfer=0; mStatTiles=0; }
    uint32_t statDraw()  const { return mStatDraw; }
    uint32_t statSetup() const { return mStatSetup; }
    uint32_t statXfer()  const { return mStatXfer; }
    uint32_t statTiles() const { return mStatTiles; }
    uint32_t tileDraw(int i) const { return (i < mStatTiles) ? mTileDraw[i] : 0; }
    uint32_t tileXfer(int i) const { return (i < mStatTiles) ? mTileXfer[i] : 0; }

    // Iterate the blocks covering a screen region. For each block:
    //   1. Acquire a tile from the pool
    //   2. Configure the Painter (tile, screen origin, block-sized clip)
    //   3. Call `draw(painter, bx, by, bw, bh)` — client draws the view tree
    //   4. bitblt the tile to the display
    //   5. Release the tile back to the pool
    template<typename Display, typename DrawFn>
    void drawRegion(const Region& region, Display& display, DrawFn&& draw) {
        int c0 = region.x / mBlockW;
        int r0 = region.y / mBlockH;
        int c1 = (region.x + region.width  + mBlockW  - 1) / mBlockW;
        int r1 = (region.y + region.height + mBlockH - 1) / mBlockH;

        if (c0 < 0)     c0 = 0;
        if (r0 < 0)     r0 = 0;
        if (c1 > mCols) c1 = mCols;
        if (r1 > mRows) r1 = mRows;

        Painter painter;
        struct AsyncSlot {
            Tile* tile = nullptr;
            volatile int done = 0;
            uint32_t submitCycle = 0;
            volatile uint32_t doneCycle = 0;
            int statIndex = 0;
        };
        AsyncSlot slots[4];

        auto releaseCompleted = [&]() {
            for (int i = 0; i < mPoolSize && i < 4; i++) {
                if (slots[i].tile != nullptr && slots[i].done) {
                    if (slots[i].statIndex < 9) {
                        mTileXfer[slots[i].statIndex] = slots[i].doneCycle - slots[i].submitCycle;
                    }
                    releaseTile(*slots[i].tile);
                    slots[i].tile = nullptr;
                    slots[i].done = 0;
                }
            }
        };

        auto waitForFreeTile = [&]() {
            releaseCompleted();
            while (mFreeCount == 0) {
                display.waitReady();
                releaseCompleted();
            }
        };

        for (int row = r0; row < r1; row++) {
            for (int col = c0; col < c1; col++) {
                int bx = col * mBlockW;
                int by = row * mBlockH;
                int bw = blockActualW(col);
                int bh = blockActualH(row);

                waitForFreeTile();

                uint32_t ts0 = DWT_CYCCNT;
                Tile& tile = acquireTile();
                {
                    uint32_t* p = (uint32_t*)tile.buffer();
                    uint32_t w = (uint32_t)(tile.width() * tile.height()) / 2;
                    for (uint32_t i = 0; i < w; i++) p[i] = 0;
                }
                painter.setTile(tile, bx, by);
                painter.setScreenOrigin(0, 0);
                painter.setScreenClip(bx, by, bx + bw, by + bh);
                uint32_t ts1 = DWT_CYCCNT;

                painter.setTileIdx((uint8_t)row);
                draw(painter, bx, by, bw, bh);
                uint32_t ts2 = DWT_CYCCNT;

                int slotIndex = -1;
                for (int i = 0; i < mPoolSize && i < 4; i++) {
                    if (slots[i].tile == nullptr) { slotIndex = i; break; }
                }
                if (slotIndex < 0) {
                    display.waitReady();
                    releaseCompleted();
                    for (int i = 0; i < mPoolSize && i < 4; i++) {
                        if (slots[i].tile == nullptr) { slotIndex = i; break; }
                    }
                }

                int ti = mStatTiles;
                AsyncSlot& slot = slots[slotIndex];
                slot.tile = &tile;
                slot.done = 0;
                slot.submitCycle = DWT_CYCCNT;
                slot.doneCycle = slot.submitCycle;
                slot.statIndex = ti;
                display.bitbltAsync(tile.buffer(), bx, by, bw, bh,
                    [](void* ctx) {
                        auto* s = static_cast<AsyncSlot*>(ctx);
                        s->doneCycle = DWT_CYCCNT;
                        s->done = 1;
                    }, &slot);

                if (ti < 9) { mTileDraw[ti] = ts2 - ts1; mTileXfer[ti] = 0; }
                mStatSetup += ts1 - ts0;
                mStatDraw  += ts2 - ts1;
                mStatTiles++;
            }
        }

        display.waitReady();
        releaseCompleted();
        for (int i = 0; i < mPoolSize && i < 4; i++) {
            if (slots[i].tile != nullptr) {
                releaseTile(*slots[i].tile);
                slots[i].tile = nullptr;
            }
        }
        mStatXfer = display.transferCycles();
    }

private:
    // ---- Tile pool (O(1) free-list stack) ----

    Tile& acquireTile() {
        if (mFreeCount > 0) {
            return mPoolTiles[mFreeList[--mFreeCount]];
        }
        // Pool exhausted — increase pool size
        return mPoolTiles[0]; // degraded fallback
    }

    void releaseTile(Tile& tile) {
        int idx = (int)(&tile - mPoolTiles);
        if (idx >= 0 && idx < mPoolSize && mFreeCount < mPoolSize) {
            mFreeList[mFreeCount++] = idx;
        }
    }

    // ---- Block size helpers ----

    int blockActualW(int col) const {
        int x = col * mBlockW;
        return (x + mBlockW <= mScreenW) ? mBlockW : mScreenW - x;
    }
    int blockActualH(int row) const {
        int y = row * mBlockH;
        return (y + mBlockH <= mScreenH) ? mBlockH : mScreenH - y;
    }

    int       mBlockW   = 0;
    int       mBlockH   = 0;
    int       mScreenW  = 0;
    int       mScreenH  = 0;
    int       mCols     = 0;
    int       mRows     = 0;
    int       mPoolSize = 0;

    Tile*     mPoolTiles = nullptr;
    uint16_t* mPoolBufs  = nullptr;
    int*      mFreeList  = nullptr;
    int       mFreeCount = 0;

    uint32_t  mStatSetup = 0;
    uint32_t  mStatDraw  = 0;
    uint32_t  mStatXfer  = 0;
    uint32_t  mStatTiles = 0;
    uint32_t  mTileDraw[9] = {};
    uint32_t  mTileXfer[9] = {};
};

} // namespace litho
