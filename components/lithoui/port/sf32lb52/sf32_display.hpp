#pragma once
#include <stdint.h>
#include <stdio.h>
#include "port/display_adapter.hpp"
#include "hal.h"  // DWT_CYCCNT

extern "C" {
/* LCD framework API — components/bsp/lcd/ (lcd.c + lcd_bus_*.c) */
void     lcd_bitblt(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                    const uint16_t* rgb565,
                    void (*done)(void* ctx), void* ctx);
void     lcd_wait_idle(void);
uint32_t lcd_xfer_cycles(void);
void     lcd_clear_xfer_cycles(void);
void     lcd_te_wait(void);
}

namespace litho {

class SF32Display : public DisplayAdapter {
public:
    bool init(int w, int h) override {
        mWidth  = w;
        mHeight = h;
        return true;
    }

    void bitblt(const uint16_t* data, int x, int y, int w, int h) override {
        if (!data || w <= 0 || h <= 0) return;
        uint32_t t0 = DWT_CYCCNT;
        lcd_bitblt((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h, data, nullptr, nullptr);
        mTransferCycles += DWT_CYCCNT - t0;
    }

    void bitbltAsync(const uint16_t* data, int x, int y, int w, int h,
                     void (*done)(void* ctx), void* ctx) override {
        if (!data || w <= 0 || h <= 0) {
            if (done) done(ctx);
            return;
        }
        lcd_bitblt((uint16_t)x, (uint16_t)y,
                   (uint16_t)w, (uint16_t)h,
                   data, done, ctx);
    }

    void waitReady() override { lcd_wait_idle(); }
    void waitTE()    override { lcd_te_wait(); }
    // flush() is intentionally a no-op — DMA drain happens naturally in
    // the next frame's waitForFreeTile(), overlapping the last tile's DMA
    // with the next frame's first tile draw.
    void flush()     override {}

    int width()  const override { return mWidth; }
    int height() const override { return mHeight; }

    uint32_t transferCycles()    const override { return lcd_xfer_cycles(); }
    uint32_t waitCycles()        const override { return lcd_wait_cycles(); }
    void     clearTransferCycles()     override  { lcd_clear_xfer_cycles(); mTransferCycles = 0; }

private:
    int mWidth  = 390;
    int mHeight = 450;
    uint32_t mTransferCycles = 0;
};

} // namespace litho
