#pragma once
#include <stdint.h>

namespace litho {

// Android-shaped MeasureSpec packed into int32_t:
//   bits 31..30 = mode, bits 29..0 = size
struct MeasureSpec {
    enum Mode : int32_t {
        UNSPECIFIED = 0,
        EXACTLY     = 1 << 30,
        AT_MOST     = 2 << 30,
    };

    static constexpr int32_t kModeMask = 3 << 30;

    static int32_t make(int size, Mode mode) {
        if (size < 0) size = 0;
        return mode | (size & ~kModeMask);
    }

    static Mode getMode(int32_t spec) {
        return static_cast<Mode>(spec & kModeMask);
    }

    static int getSize(int32_t spec) {
        return spec & ~kModeMask;
    }

    static int resolveSize(int size, int32_t spec) {
        switch (getMode(spec)) {
        case EXACTLY:
            return getSize(spec);
        case AT_MOST: {
            int s = getSize(spec);
            return size > s ? s : size;
        }
        default:
            return size;
        }
    }

    static int getDefaultSize(int size, int32_t spec) {
        switch (getMode(spec)) {
        case EXACTLY:
        case AT_MOST:
            return getSize(spec);
        default:
            return size;
        }
    }
};

} // namespace litho
