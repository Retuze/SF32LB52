#pragma once
#include <stdint.h>

namespace litho {

class DisplayAdapter {
public:
    virtual ~DisplayAdapter() = default;

    virtual bool init(int width, int height) = 0;

    // Push RGB565 pixel rect at screen position.
    virtual void bitblt(const uint16_t* data,
                       int x, int y, int w, int h) = 0;

    // Commit all bitblted pixels to the screen.
    virtual void flush() = 0;

    virtual int width()  const = 0;
    virtual int height() const = 0;
};

} // namespace litho
