#pragma once
#include "window.hpp"
#include "core/pfb.hpp"
#include "core/dirty_list.hpp"
#include "framework/animation/animation_manager.hpp"
#include <stdint.h>
#include <stdio.h>

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
        uint32_t cyc0 = DWT_CYCCNT;
        uint32_t touchN = 0;

        // Input is sampled at the END of the previous frame — inside the PFB
        // drain, overlapped with the last tile's DMA (see the onIdle lambda
        // below) — and dispatched here. This 1-frame deferral keeps the slow
        // bit-bang touch-I2C read off the serial critical path. We deliberately
        // do NOT call mInput.pollEvent() at the top of the frame anymore.
        if (mHasDeferredEv) {
            mHasDeferredEv = false;
            if (mDeferredEv.type == EventType::QUIT) return false;
            if (mDeferredEv.type == EventType::TOUCH && mCount > 0) {
                touchN++;
                mWindows[mCount - 1]->dispatchTouchEvent(mDeferredEv.touch);
            }
        }

        // Advance animations each frame (absolute time)
        uint32_t frameTimeMs = mTick.tickMs();
        mAnimMgr.tick(frameTimeMs);
        uint32_t cycInput = DWT_CYCCNT;   // end of input + anim

        // Render each dirty region through PFB. The onIdle callback samples the
        // NEXT frame's input while the final DMA is still transferring, so the
        // I2C read overlaps the transfer rather than stalling the frame.
        mPFB.clearStats();
        mDisplay.clearTransferCycles();
        bool sampled = false;
        auto sampleInput = [this, &sampled]() {
            if (sampled) return;
            sampled = true;
            Event e;
            if (mInput.pollEvent(e)) { mDeferredEv = e; mHasDeferredEv = true; }
        };
        for (int ri = 0; ri < mDirtyList.count(); ri++) {
            const Region& r = mDirtyList.regions()[ri];

            mPFB.drawRegion(r, mDisplay,
                [this](Painter& p, int /*bx*/, int /*by*/, int /*bw*/, int /*bh*/) {
                    for (uint16_t wi = 0; wi < mCount; wi++) {
                        mWindows[wi]->draw(p);
                    }
                },
                sampleInput);
        }
        if (!sampled) sampleInput();   // no dirty region this frame — sample anyway
        uint32_t cycRender = DWT_CYCCNT;  // end of PFB pipeline (draw + async xfer drained)

        bool drew = mDirtyList.count() > 0;
        mDirtyList.clear();
        if (drew) {
            mDisplay.flush();
            mFrameCount++;
        }
        uint32_t cycEnd = DWT_CYCCNT;

        // Buffer per-frame stats into RAM only — defer all printf to a batched
        // dump between frames.  USART1 _write blocks ~10 µs/char @ 1 Mbps, so a
        // per-frame print would stall the render loop and distort scroll timing.
        // Storing a few ints here is ~free and keeps `tot` measurement clean.
        if (drew && mRingCount < kStatRing) {
            const uint32_t US = 240u;  // DWT @ 240 MHz → microseconds
            FrameStat& f = mRing[mRingCount++];
            f.tot   = (cycEnd    - cyc0)      / US;   // wall-clock frame time
            f.in    = (cycInput  - cyc0)      / US;   // input + animation tick
            f.rend  = (cycRender - cycInput)  / US;   // PFB pipeline (draw + xfer drain)
            f.draw  = mPFB.statDraw()  / US;          // Σ per-tile CPU draw
            f.xfer  = mPFB.statXfer()  / US;          // Σ DMA transfer (from IRQ cb)
            f.setup = mPFB.statSetup() / US;
            f.touch = touchN;                         // touch events dispatched this frame
            // Keep the heaviest-draw frame's per-tile detail for the batch dump.
            if (mPFB.statDraw() > mWorstDrawCyc) {
                mWorstDrawCyc = mPFB.statDraw();
                mWorstTiles   = (int)mPFB.statTiles();
                if (mWorstTiles > 9) mWorstTiles = 9;
                for (int i = 0; i < mWorstTiles; i++) {
                    mWorstTDraw[i] = mPFB.tileDraw(i) / US;
                    mWorstTXfer[i] = mPFB.tileXfer(i) / US;
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
    // its USART blocking never lands inside a render. tot = wall-clock frame;
    // serial = draw+xfer+setup with zero overlap; tot << serial ⇒ async pipeline
    // is overlapping; tot ≈ xfer ⇒ transfer-bound (ideal); tot rising with draw
    // while xfer flat ⇒ CPU draw broke through the DMA and is now the bottleneck.
    void dumpStats() {
        printf("[batch] %d frames, us | idx  tot   in rend draw xfer setup tch\r\n", mRingCount);
        for (int i = 0; i < mRingCount; i++) {
            const FrameStat& f = mRing[i];
            printf("%3d %5lu %4lu %5lu %5lu %5lu %4lu %3lu\r\n", i,
                   (unsigned long)f.tot,  (unsigned long)f.in,
                   (unsigned long)f.rend, (unsigned long)f.draw,
                   (unsigned long)f.xfer, (unsigned long)f.setup,
                   (unsigned long)f.touch);
        }
        printf("[worst] max-draw frame, per-tile d/x(us):");
        for (int i = 0; i < mWorstTiles; i++)
            printf(" %lu/%lu",
                   (unsigned long)mWorstTDraw[i], (unsigned long)mWorstTXfer[i]);
        printf("\r\n");
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

    // Input deferred by one frame: sampled at end of frame N (overlapping the
    // last DMA), dispatched at the start of frame N+1. Keeps touch-I2C off the
    // serial critical path.
    Event     mDeferredEv   = {};
    bool      mHasDeferredEv = false;

    // Deferred per-frame stats — written during render, printed only between
    // frames in one burst (see dumpStats) so USART blocking never taints timing.
    struct FrameStat { uint32_t tot, in, rend, draw, xfer, setup, touch; };
    static const int kStatRing = 60;   // ~1 batch dump per 60 rendered frames
    FrameStat mRing[kStatRing] = {};
    int       mRingCount   = 0;
    uint32_t  mWorstDrawCyc = 0;       // heaviest-draw frame in the batch
    int       mWorstTiles   = 0;
    uint32_t  mWorstTDraw[9] = {};
    uint32_t  mWorstTXfer[9] = {};
};

} // namespace litho
