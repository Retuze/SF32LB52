#pragma once
#include "window.hpp"
#include "core/pfb.hpp"
#include "core/dirty_list.hpp"
#include "framework/animation/animation_manager.hpp"
#include <stdint.h>
#include <stdio.h>

extern "C" {
void lcd_ref_fill_buf(uint16_t* buf, int stride,
                      int w, int h, uint16_t color);
}

// DWT cycle counter (Cortex-M33, same as SF32LB52.h)
#ifndef DWT_CYCCNT
#define DWT_CYCCNT (*(volatile uint32_t*)0xE0001004UL)
#endif
#include "port/display_adapter.hpp"
#include "port/input_adapter.hpp"
#include "port/tick_adapter.hpp"

#include <stdint.h>
#include <assert.h>
#include <stdio.h>   // printf for FPS stats

namespace litho {

class WindowManager {
public:
    WindowManager(DisplayAdapter& display, InputAdapter& input, TickAdapter& tick)
        : mDisplay(display)
        , mInput(input)
        , mTick(tick)
    {}

    bool initPFB(int blockW, int blockH, int poolSize) {
        return mPFB.init(blockW, blockH, poolSize,
                         mDisplay.width(), mDisplay.height());
    }

    Window* createWindow() {
        assert(mCount < 4 && "max 4 windows");
        auto* w = new Window();
        w->setDirtyList(&mDirtyList);
        mWindows[mCount++] = w;
        return w;
    }

    void destroyWindow(Window* w) {
        for (uint16_t i = 0; i < mCount; i++) {
            if (mWindows[i] == w) {
                delete w;
                for (uint16_t j = i; j < mCount - 1; j++)
                    mWindows[j] = mWindows[j + 1];
                mCount--;
                return;
            }
        }
    }

    bool runOnce() {
        Event ev;
        while (mInput.pollEvent(ev)) {
            if (ev.type == EventType::QUIT) return false;
            if (ev.type == EventType::TOUCH && mCount > 0) {
                mWindows[mCount - 1]->dispatchTouchEvent(ev.touch);
            }
        }

        // Advance animations each frame (absolute time)
        uint32_t frameTimeMs = mTick.tickMs();
        mAnimMgr.tick(frameTimeMs);

        // Render each dirty region through PFB
        mPFB.clearStats();
        uint32_t frameCycles = DWT_CYCCNT;
        for (int ri = 0; ri < mDirtyList.count(); ri++) {
            const Region& r = mDirtyList.regions()[ri];

            mPFB.drawRegion(r, mDisplay,
                [this](Painter& p, int /*bx*/, int /*by*/, int /*bw*/, int /*bh*/) {
                    for (uint16_t wi = 0; wi < mCount; wi++) {
                        mWindows[wi]->draw(p);
                    }
                });
        }

        bool drew = mDirtyList.count() > 0;
        mDirtyList.clear();
        if (drew) {
            mDisplay.flush();
            // Per-frame breakdown: setup(PFB) / draw(views) / xfer(bitblt)
            uint32_t setupUs = mPFB.statSetup() / 240UL;
            uint32_t drawUs  = mPFB.statDraw()  / 240UL;
            uint32_t xferUs  = mPFB.statXfer()  / 240UL;
            uint32_t totalUs = (DWT_CYCCNT - frameCycles) / 240UL;
            uint32_t tiles   = mPFB.statTiles();
            printf("[fr %u] %lu us  setup=%lu draw=%lu xfer=%lu tiles=%lu\n",
                   (unsigned)mFrameCount++,
                   (unsigned long)totalUs,
                   (unsigned long)setupUs, (unsigned long)drawUs,
                   (unsigned long)xferUs, (unsigned long)tiles);
            // Per-tile draw times
            printf("  tile: ");
            for (uint32_t ti = 0; ti < tiles && ti < 9; ti++) {
                printf("%lu ", (unsigned long)(mPFB.tileDraw(ti) / 240UL));
            }
            printf("us\n");
            fflush(stdout);
        }
        return true;
    }

    void run() {
        mRunning = true;
        while (mRunning) {
            if (!runOnce()) mRunning = false;
        }
    }

    void quit() { mRunning = false; }

    AnimationManager& animationManager() { return mAnimMgr; }

    int displayWidth()  const { return mDisplay.width(); }
    int displayHeight() const { return mDisplay.height(); }

private:
    DisplayAdapter& mDisplay;
    InputAdapter&   mInput;
    TickAdapter&    mTick;
    PFB             mPFB;
    AnimationManager mAnimMgr;
    DirtyList       mDirtyList;

    Window*   mWindows[4] = {};
    uint16_t  mCount      = 0;
    bool      mRunning    = false;
    uint32_t  mFrameCount = 0;
};

} // namespace litho
