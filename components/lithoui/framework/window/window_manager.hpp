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
        View::setFrameTimeMs(frameTimeMs);
        mAnimMgr.tick(frameTimeMs);
        uint32_t cycInput = dwt_cycles();

        // Layout before paint so requestLayout() from anim/touch lands this frame.
        for (uint16_t wi = 0; wi < mCount; wi++) {
            if (mWindows[wi]->visible())
                mWindows[wi]->layoutIfNeeded(mDisplay.width(), mDisplay.height());
        }

        // Snapshot DMA transfer cycles BEFORE drawRegion — the delta after
        // flush captures exactly the DMA time for THIS frame's tiles.
        uint32_t xferBefore = mDisplay.transferCycles();

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
                        if (mWindows[wi]->visible())
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

        bool drew = mDirtyList.count() > 0;
        mDirtyList.clear();
        if (drew) {
            mDisplay.flush();       // waitReady — drain all remaining DMA
            mFrameCount++;
        }
        uint32_t cycRender = dwt_cycles();  // end of PFB pipeline (draw + flush drain)

        // DMA xfer snapshot: delta captures exactly this frame's DMA time.
        // flush() confirmed all DMA done, so all ISRs have fired.
        uint32_t xferAfter = mDisplay.transferCycles();
        uint32_t frameXfer = xferAfter - xferBefore;

        // Inter-frame idle gap (raw cycles): time between previous runOnce()
        // return and this invocation's entry.  Large gap → frame started late,
        // most commonly because dumpStats() was blocking on the UART.  gap is
        // NOT part of tot (which measures CPU-busy within the frame), but it IS
        // part of the effective frame interval.
        uint32_t gapCyc = mPrevCycEnd ? (cyc0 - mPrevCycEnd) : 0;
        bool justDumped = mJustDumped;
        mJustDumped = false;
        uint32_t cycEnd = dwt_cycles();  // end of frame (post flush + stat capture + clear)
        mPrevCycEnd = cycEnd;

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
            f.xfer     = frameXfer / US;                 // Σ DMA transfer (pre/post flush delta)
            f.waitDMA  = mPFB.statWaitDMA() / US;        // Σ blocked in bitbltAsync waiting for DMA
            f.setup    = mPFB.statSetup() / US;
            f.waitTE   = mPFB.statWaitTE()   / US;       // Wait for TE before first xfer
            f.waitBuff = mPFB.statWaitBuff() / US;       // Wait for buffer (pool full)
            f.poll     = mPFB.statPoll()     / US;       // Touch poll + dispatch
            f.touch    = touchN;                         // touch events dispatched this frame
            f.gap      = gapCyc / US;                     // inter-frame idle (print, OS yield)
            f.afterDump = justDumped;                     // true if started after a dumpStats

            // Commit pending per-tile xfer stats (DMA callbacks now all fired
            // since flush() drained everything).
            mPFB.commitXferStats();

            // Keep the heaviest-draw frame's per-tile detail for the batch dump.
            if (mPFB.statDraw() > mWorstDrawCyc) {
                mWorstDrawCyc = mPFB.statDraw();
                mWorstTiles   = (int)mPFB.statTiles();
                if (mWorstTiles > 9) mWorstTiles = 9;
                for (int i = 0; i < mWorstTiles; i++) {
                    mWorstTDraw[i]    = mPFB.tileDraw(i)    / US;
                    mWorstTXfer[i]    = mPFB.tileXfer(i)    / US;
                    mWorstTWaitDMA[i] = mPFB.tileWaitDMA(i) / US;
                    mWorstTWait[i]    = mPFB.tileWaitBuff(i) / US;
                }
            }
        }
        // Clear stats for NEXT frame (all DMA confirmed done by flush above).
        mPFB.clearStats();
        mDisplay.clearTransferCycles();

        if (mRingCount >= kStatRing) {
            dumpStats();          // single blocking burst, strictly between frames
            mRingCount    = 0;
            mWorstDrawCyc = 0;
            mJustDumped   = true; // next frame's gap will include this dump time
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
        uint32_t sumTot=0, sumDraw=0, sumXfer=0, sumWaitDMA=0, sumWaitBuff=0;
        uint32_t sumRend=0, sumSetup=0, sumPoll=0, sumWaitTE=0;
        uint32_t minTot=999999, maxTot=0;
        uint32_t minDraw=999999, maxDraw=0;
        int touchTotal = 0, afterDumpCount = 0;
        uint32_t maxGap = 0;
        uint32_t top3[3] = {};  // track largest 3 tot values

        for (int i = 0; i < mRingCount; i++) {
            const FrameStat& f = mRing[i];
            sumTot += f.tot;
            sumDraw += f.draw;
            sumXfer += f.xfer;
            sumWaitDMA += f.waitDMA;
            sumWaitBuff += f.waitBuff;
            sumWaitTE   += f.waitTE;
            sumRend += f.rend;
            sumSetup += f.setup;
            sumPoll += f.poll;
            touchTotal += f.touch;
            if (f.afterDump) afterDumpCount++;
            if (f.gap > maxGap) maxGap = f.gap;

            if (f.tot < minTot) minTot = f.tot;
            if (f.tot > maxTot) maxTot = f.tot;
            if (f.draw < minDraw) minDraw = f.draw;
            if (f.draw > maxDraw) maxDraw = f.draw;

            // Insertion sort into top3 (descending, keep largest 3)
            if (f.tot > top3[0]) {
                top3[2] = top3[1]; top3[1] = top3[0]; top3[0] = f.tot;
            } else if (f.tot > top3[1]) {
                top3[2] = top3[1]; top3[1] = f.tot;
            } else if (f.tot > top3[2]) {
                top3[2] = f.tot;
            }
        }

        uint32_t avgTot = sumTot / mRingCount;
        uint32_t avgDraw = sumDraw / mRingCount;
        uint32_t avgXfer = sumXfer / mRingCount;
        uint32_t avgWaitDMA = sumWaitDMA / mRingCount;
        uint32_t avgWaitBuff = sumWaitBuff / mRingCount;
        uint32_t avgRend   = sumRend   / mRingCount;
        uint32_t avgSetup  = sumSetup  / mRingCount;
        uint32_t avgPoll   = sumPoll   / mRingCount;
        uint32_t avgWaitTE = sumWaitTE / mRingCount;

        // Compact summary: <2ms @ 1Mbps
        //  t: frame wall   r: PFB pipeline   d: CPU draw   s: setup+memset
        //  te: wait-TE   p: poll+idle   x: DMA   w: wait-DMA   wb: wait-buffer
        //  r ≈ d + s + p + w + te + lcdc_config (gap = lcdc register overhead)
        printf("[%dF] t:%lu~%lu~%lu(%lu,%lu,%lu) r:%lu d:%lu~%lu s:%lu te:%lu p:%lu x:%lu w:%lu wb:%lu g:%lu tch:%d late:%d",
               mRingCount,
               (unsigned long)minTot, (unsigned long)avgTot, (unsigned long)maxTot,
               (unsigned long)top3[0], (unsigned long)top3[1], (unsigned long)top3[2],
               (unsigned long)avgRend,
               (unsigned long)minDraw, (unsigned long)avgDraw,
               (unsigned long)avgSetup, (unsigned long)avgWaitTE, (unsigned long)avgPoll,
               (unsigned long)avgXfer, (unsigned long)avgWaitDMA,
               (unsigned long)avgWaitBuff,
               (unsigned long)maxGap,
               touchTotal, late);
        if (afterDumpCount > 0) printf(" *");
        printf("\r\n");

        // Per-tile detail only for worst frame: draw/waitDMA/xfer
        //  draw:   CPU render time for this tile
        //  waitDMA: time blocked in lcd_wait_idle() before launching DMA
        //  xfer:   total submit→done (waitDMA + actual DMA)
        if (mWorstTiles > 0) {
            printf("[W]");
            for (int i = 0; i < mWorstTiles; i++) {
                printf(" %lu/%lu/%lu",
                       (unsigned long)mWorstTDraw[i],
                       (unsigned long)mWorstTWaitDMA[i],
                       (unsigned long)mWorstTXfer[i]);
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
        uint32_t tot, in, rend, draw, xfer, waitDMA, setup, touch;
        uint32_t waitTE, waitBuff, poll;
        uint32_t gap;       // inter-frame idle gap (µs) — includes dumpStats print time
        bool     afterDump; // true if this frame started right after a stats dump
    };
    static const int kStatRing = 60;   // ~1 batch dump per 60 rendered frames
    FrameStat mRing[kStatRing] = {};
    int       mRingCount   = 0;
    uint32_t  mWorstDrawCyc = 0;       // heaviest-draw frame in the batch
    int       mWorstTiles   = 0;
    uint32_t  mWorstTDraw[9]    = {};
    uint32_t  mWorstTXfer[9]    = {};
    uint32_t  mWorstTWaitDMA[9] = {};  // Per-tile: blocked in bitbltAsync waiting for DMA
    uint32_t  mWorstTWait[9]    = {};  // Per-tile wait time before buffer available
    uint32_t  mPrevCycEnd  = 0;        // cycEnd of previous runOnce() (for gap calc)
    bool      mJustDumped  = false;     // next frame started right after dumpStats
};

} // namespace litho
