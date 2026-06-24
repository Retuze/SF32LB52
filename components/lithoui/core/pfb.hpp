#pragma once
#include "tile.hpp"
#include "region.hpp"
#include "painter.hpp"
#include <stdint.h>
#include <stdio.h>

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

        for (int row = r0; row < r1; row++) {
            for (int col = c0; col < c1; col++) {
                int bx = col * mBlockW;
                int by = row * mBlockH;
                int bw = blockActualW(col);
                int bh = blockActualH(row);

                Tile& tile = acquireTile();

                painter.setTile(tile, bx, by);
                painter.setScreenOrigin(0, 0);
                painter.setScreenClip(bx, by, bx + bw, by + bh);

                draw(painter, bx, by, bw, bh);

                display.bitblt(tile.buffer(), bx, by, bw, bh);
                releaseTile(tile);
            }
        }
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
};

} // namespace litho
