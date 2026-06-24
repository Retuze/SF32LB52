#pragma once
#include "window.hpp"
#include "core/pfb.hpp"
#include "core/dirty_list.hpp"
#include "framework/animation/animation_manager.hpp"
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
        uint32_t renderStart = mTick.tickMs();
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
            // ── FPS / render-time stats (printed once per second) ──
            mFpsRenderMs += (mTick.tickMs() - renderStart);
            mFpsFrames++;
            if (mFpsT0 == 0) mFpsT0 = frameTimeMs;
            uint32_t elapsed = frameTimeMs - mFpsT0;
            if (elapsed >= 1000) {
                double fps   = mFpsFrames * 1000.0 / elapsed;
                double avgMs = mFpsFrames ? (double)mFpsRenderMs / mFpsFrames : 0.0;
                printf("FPS: %.1f  (%d frames in %u ms, avg %.1f ms/frame render)\n",
                       fps, mFpsFrames, elapsed, avgMs);
                fflush(stdout);
                mFpsT0 = frameTimeMs; mFpsFrames = 0; mFpsRenderMs = 0;
            }
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

    // FPS / render-time stats (see runOnce)
    uint32_t  mFpsT0       = 0;
    uint32_t  mFpsFrames   = 0;
    uint32_t  mFpsRenderMs = 0;
};

} // namespace litho
