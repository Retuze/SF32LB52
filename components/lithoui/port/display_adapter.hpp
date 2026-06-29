#pragma once
#include <stdint.h>

namespace litho {

class DisplayAdapter {
public:
    virtual ~DisplayAdapter() = default;

    virtual bool init(int width, int height) = 0;

    virtual void bitblt(const uint16_t* data,
                       int x, int y, int w, int h) = 0;

    virtual void bitbltAsync(const uint16_t* data,
                             int x, int y, int w, int h,
                             void (*done)(void* ctx), void* ctx) {
        bitblt(data, x, y, w, h);
        if (done) done(ctx);
    }

    virtual void waitReady() {}

    virtual void flush() = 0;

    virtual int width()  const = 0;
    virtual int height() const = 0;

    /** Optional: transfer time in CPU cycles (0 if not tracked). */
    virtual uint32_t transferCycles()    const { return 0; }
    virtual void     clearTransferCycles()     {}
};

} // namespace litho
