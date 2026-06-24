#pragma once
#include <stdint.h>
#include "port/display_adapter.hpp"

extern "C" {
void lcd_ref_bitblt(uint16_t x, uint16_t y,
                    uint16_t w, uint16_t h,
                    const uint16_t* rgb565);
void lcd_ref_fill_rect(uint16_t x0, uint16_t y0,
                       uint16_t x1, uint16_t y1,
                       uint16_t color);
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
        lcd_ref_bitblt((uint16_t)x, (uint16_t)y,
                       (uint16_t)w, (uint16_t)h, data);
    }

    void flush() override {}

    int width()  const override { return mWidth; }
    int height() const override { return mHeight; }

private:
    int mWidth  = 390;
    int mHeight = 450;
};

} // namespace litho
