#pragma once
#include <stdint.h>

namespace litho {

struct Region {
    int16_t x      = 0;
    int16_t y      = 0;
    int16_t width  = 0;
    int16_t height = 0;

    bool contains(int px, int py) const {
        return px >= x && px < x + width &&
               py >= y && py < y + height;
    }

    bool isEmpty() const { return width <= 0 || height <= 0; }
};

} // namespace litho
