#pragma once
#include <stdint.h>
#include "port/display_adapter.hpp"
#include "hal.h"  // DWT_CYCCNT

extern "C" {
void lcd_bitblt(uint16_t x, uint16_t y,
                    uint16_t w, uint16_t h,
                    const uint16_t* rgb565);
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

    void flush() override {}

    int width()  const override { return mWidth; }
    int height() const override { return mHeight; }

    uint32_t transferCycles() const override { return mTransferCycles; }
    void     clearTransferCycles()  override  { mTransferCycles = 0; }

private:
    int mWidth  = 390;
    int mHeight = 450;
    uint32_t mTransferCycles = 0;
};

} // namespace litho
