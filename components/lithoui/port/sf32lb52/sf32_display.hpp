#pragma once
#include <stdint.h>
#include "port/display_adapter.hpp"
#include "hal.h"  // DWT_CYCCNT

extern "C" {
/* Sync bitblt via the active bus (works with both bit-bang and LCDC) */
void lcd_bitblt(uint16_t x, uint16_t y,
                uint16_t w, uint16_t h,
                const uint16_t* rgb565);

/* LCDC async API (no-ops when bit-bang bus is active) */
void lcd_bitblt_async(uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h,
                      const uint16_t* rgb565,
                      void (*done)(void* ctx), void* ctx);
void lcd_wait_idle(void);
uint32_t lcd_xfer_cycles(void);
void     lcd_clear_xfer_cycles(void);
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
        lcd_bitblt((uint16_t)x, (uint16_t)y,
                   (uint16_t)w, (uint16_t)h, data);
        mTransferCycles += DWT_CYCCNT - t0;
    }

    void bitbltAsync(const uint16_t* data, int x, int y, int w, int h,
                     void (*done)(void* ctx), void* ctx) override {
        if (!data || w <= 0 || h <= 0) {
            if (done) done(ctx);
            return;
        }
        lcd_bitblt_async((uint16_t)x, (uint16_t)y,
                         (uint16_t)w, (uint16_t)h,
                         data, done, ctx);
    }

    void waitReady() override { lcd_wait_idle(); }
    void flush()    override { waitReady(); }

    int width()  const override { return mWidth; }
    int height() const override { return mHeight; }

    uint32_t transferCycles()    const override { return lcd_xfer_cycles(); }
    void     clearTransferCycles()     override  { lcd_clear_xfer_cycles(); mTransferCycles = 0; }

private:
    int mWidth  = 390;
    int mHeight = 450;
    uint32_t mTransferCycles = 0;
};

} // namespace litho
