#pragma once
#include <stdint.h>

namespace litho {

class Tile {
public:
    Tile() = default;

    void attach(uint16_t* buffer, int width, int height) {
        mBuffer = buffer;
        mWidth  = width;
        mHeight = height;
        mStride = width;  // tightly packed
    }

    uint16_t*       buffer()       { return mBuffer; }
    const uint16_t* buffer() const { return mBuffer; }
    int width()  const { return mWidth; }
    int height() const { return mHeight; }
    int stride() const { return mStride; }

private:
    uint16_t* mBuffer = nullptr;
    int       mWidth  = 0;
    int       mHeight = 0;
    int       mStride = 0;
};

} // namespace litho
