#pragma once
#include "window.hpp"
#include "core/pfb.hpp"
#include "core/dirty_list.hpp"
#include "framework/animation/animation_manager.hpp"
#include "port/display_adapter.hpp"
#include "port/input_adapter.hpp"
#include "port/tick_adapter.hpp"
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "hal.h"  // dwt_cycles()

extern "C" int lcd_te_late_count(void);

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
        uint32_t cyc0 = dwt_cycles();
        uint32_t touchN = 0;

        uint32_t frameTimeMs = mTick.tickMs();
        mAnimMgr.tick(frameTimeMs);
        uint32_t cycInput = dwt_cycles();

        mPFB.clearStats();
        mDisplay.clearTransferCycles();

        auto sampleAndDispatch = [this, &touchN]() {
            Event e;
            if (mInput.pollEvent(e)) {
                if (e.type == EventType::QUIT) {
                    mQuitRequested = true;
                } else if (e.type == EventType::TOUCH && mCount > 0) {
                    touchN++;
                    mWindows[mCount - 1]->dispatchTouchEvent(e.touch);
                }
            }
        };

        bool sampled = false;
        auto sampleOnce = [&]() {
            if (!sampled) {
                sampled = true;
                sampleAndDispatch();
            }
        };

        for (int ri = 0; ri < mDirtyList.count(); ri++) {
            const Region& r = mDirtyList.regions()[ri];

            mPFB.drawRegion(r, mDisplay,
                [this](Painter& p, int /*bx*/, int /*by*/, int /*bw*/, int /*bh*/) {
                    for (uint16_t wi = 0; wi < mCount; wi++) {
                        mWindows[wi]->draw(p);
                    }
                },
                sampleOnce);
        }

        // If no rendering happened this frame (idle screen), still poll touch
        // so we don't lose the event that will trigger the next frame's rendering.
        if (!sampled) {
            sampleAndDispatch();
        }

        // Handle deferred quit (can't return mid-frame)
        if (mQuitRequested) return false;
        uint32_t cycRender = dwt_cycles();  // end of PFB pipeline (draw + async xfer drained)

        bool drew = mDirtyList.count() > 0;
        mDirtyList.clear();
        if (drew) {
            mDisplay.flush();
            mFrameCount++;
        }
        uint32_t cycEnd = dwt_cycles();

        // Buffer per-frame stats into RAM only — defer all printf to a batched
        // dump between frames.  USART1 _write blocks ~10 µs/char @ 1 Mbps, so a
        // per-frame print would stall the render loop and distort scroll timing.
        // Storing a few ints here is ~free and keeps `tot` measurement clean.
        if (drew && mRingCount < kStatRing) {
            const uint32_t US = 240u;  // DWT @ 240 MHz → microseconds
            FrameStat& f = mRing[mRingCount++];
            f.tot      = (cycEnd    - cyc0)      / US;   // wall-clock frame time
            f.in       = (cycInput  - cyc0)      / US;   // input + animation tick
            f.rend     = (cycRender - cycInput)  / US;   // PFB pipeline (draw + xfer drain)
            f.draw     = mPFB.statDraw()  / US;          // Σ per-tile CPU draw
            f.xfer     = mPFB.statXfer()  / US;          // Σ DMA transfer (from IRQ cb)
            f.setup    = mPFB.statSetup() / US;
            f.waitTE   = mPFB.statWaitTE()   / US;       // Wait for TE before first xfer
            f.waitBuff = mPFB.statWaitBuff() / US;       // Wait for buffer (pool full)
            f.poll     = mPFB.statPoll()     / US;       // Touch poll + dispatch
            f.touch    = touchN;                         // touch events dispatched this frame
            // Keep the heaviest-draw frame's per-tile detail for the batch dump.
            if (mPFB.statDraw() > mWorstDrawCyc) {
                mWorstDrawCyc = mPFB.statDraw();
                mWorstTiles   = (int)mPFB.statTiles();
                if (mWorstTiles > 9) mWorstTiles = 9;
                for (int i = 0; i < mWorstTiles; i++) {
                    mWorstTDraw[i] = mPFB.tileDraw(i) / US;
                    mWorstTXfer[i] = mPFB.tileXfer(i) / US;
                    mWorstTWait[i] = mPFB.tileWaitBuff(i) / US;
                }
            }
        }
        if (mRingCount >= kStatRing) {
            dumpStats();          // single blocking burst, strictly between frames
            mRingCount    = 0;
            mWorstDrawCyc = 0;
        }
        return true;
    }

    // Flush the buffered per-frame stats in one burst. Called between frames so
    // its USART blocking never lands inside a render. Shows detailed breakdown:
    // - Summary: avg/min/max of key metrics across the batch
    // - Per-tile: only for the worst frame (highest draw time)
    // At 1 Mbps (100 KB/s), each char ≈ 10µs. Keep output under ~200 chars for <2ms.
    void dumpStats() {
        int late = lcd_te_late_count();

        // Calculate summary stats (avg/min/max)
        uint32_t sumTot=0, sumDraw=0, sumXfer=0, sumWaitBuff=0;
        uint32_t minTot=999999, maxTot=0;
        uint32_t minDraw=999999, maxDraw=0;
        int touchTotal = 0;

        for (int i = 0; i < mRingCount; i++) {
            const FrameStat& f = mRing[i];
            sumTot += f.tot;
            sumDraw += f.draw;
            sumXfer += f.xfer;
            sumWaitBuff += f.waitBuff;
            touchTotal += f.touch;

            if (f.tot < minTot) minTot = f.tot;
            if (f.tot > maxTot) maxTot = f.tot;
            if (f.draw < minDraw) minDraw = f.draw;
            if (f.draw > maxDraw) maxDraw = f.draw;
        }

        uint32_t avgTot = sumTot / mRingCount;
        uint32_t avgDraw = sumDraw / mRingCount;
        uint32_t avgXfer = sumXfer / mRingCount;
        uint32_t avgWaitBuff = sumWaitBuff / mRingCount;

        // Compact summary: ~120 chars, <1.2ms @ 1Mbps
        printf("[%dF] t:%lu~%lu~%lu d:%lu~%lu x:%lu wb:%lu tch:%d late:%d\r\n",
               mRingCount,
               (unsigned long)minTot, (unsigned long)avgTot, (unsigned long)maxTot,
               (unsigned long)minDraw, (unsigned long)avgDraw,
               (unsigned long)avgXfer, (unsigned long)avgWaitBuff,
               touchTotal, late);

        // Per-tile detail only for worst frame: ~20 chars/tile, <2ms total
        if (mWorstTiles > 0) {
            printf("[W]");
            for (int i = 0; i < mWorstTiles; i++) {
                printf(" %lu/%lu", (unsigned long)mWorstTDraw[i], (unsigned long)mWorstTXfer[i]);
                if (mWorstTWait[i] > 0) printf("!%lu", (unsigned long)mWorstTWait[i]);
            }
            printf("\r\n");
        }
    }

    void run() {
        mRunning = true;
        while (mRunning) {
            if (!runOnce()) mRunning = false;
        }
    }

    void quit() { mRunning = false; }

    /** Re-dirty a region for the next frame (benchmark / free-run). */
    void invalidateRegion(const Region& r) { mDirtyList.markDirty(r); }

    void invalidateAll() {
        Region r;
        r.x = 0; r.y = 0;
        r.width  = (int16_t)mDisplay.width();
        r.height = (int16_t)mDisplay.height();
        mDirtyList.markDirty(r);
    }

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
    bool      mQuitRequested = false;

    // Deferred per-frame stats — written during render, printed only between
    // frames in one burst (see dumpStats) so USART blocking never taints timing.
    struct FrameStat {
        uint32_t tot, in, rend, draw, xfer, setup, touch;
        uint32_t waitTE, waitBuff, poll;
    };
    static const int kStatRing = 60;   // ~1 batch dump per 60 rendered frames
    FrameStat mRing[kStatRing] = {};
    int       mRingCount   = 0;
    uint32_t  mWorstDrawCyc = 0;       // heaviest-draw frame in the batch
    int       mWorstTiles   = 0;
    uint32_t  mWorstTDraw[9] = {};
    uint32_t  mWorstTXfer[9] = {};
    uint32_t  mWorstTWait[9] = {};     // Per-tile wait time before buffer available
};

} // namespace litho
