#pragma once

#include <stdint.h>

namespace litho {

// RGB565 colour — 16 bits per pixel
// RRRRRGGGGGGBBBBB
struct RGB565 {
    uint16_t value;

    static RGB565 fromRGB(uint8_t r, uint8_t g, uint8_t b) {
        return { (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)) };
    }

    static RGB565 White()  { return fromRGB(255, 255, 255); }
    static RGB565 Black()  { return fromRGB(0,   0,   0);   }
    static RGB565 Red()    { return fromRGB(255, 0,   0);   }
    static RGB565 Green()  { return fromRGB(0,   255, 0);   }
    static RGB565 Blue()   { return fromRGB(0,   0,   255); }
};

} // namespace litho
