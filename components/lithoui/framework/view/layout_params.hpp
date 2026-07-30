#pragma once
#include <stdint.h>

namespace litho {

// Bit flags for FrameLayout / LinearLayout child alignment.
namespace Gravity {
    constexpr int LEFT      = 0x01;
    constexpr int RIGHT     = 0x02;
    constexpr int TOP       = 0x04;
    constexpr int BOTTOM    = 0x08;
    constexpr int CENTER_H  = 0x10;
    constexpr int CENTER_V  = 0x20;
    constexpr int CENTER    = CENTER_H | CENTER_V;
    constexpr int START     = LEFT;   // no RTL yet
    constexpr int END       = RIGHT;
}

// Base layout params — ConstraintLayout will subclass later.
struct LayoutParams {
    static constexpr int16_t MATCH_PARENT = -1;
    static constexpr int16_t WRAP_CONTENT = -2;

    int16_t width   = WRAP_CONTENT;
    int16_t height  = WRAP_CONTENT;
    int16_t marginL = 0;
    int16_t marginT = 0;
    int16_t marginR = 0;
    int16_t marginB = 0;
    // Used by FrameLayout / LinearLayout cross-axis; ignored by absolute ViewGroup.
    int     gravity = Gravity::TOP | Gravity::LEFT;
    // LinearLayout main-axis weight; 0 = no stretch. Ignored by FrameLayout.
    float   weight  = 0.f;

    LayoutParams() = default;
    LayoutParams(int16_t w, int16_t h) : width(w), height(h) {}
    LayoutParams(int16_t w, int16_t h, int g) : width(w), height(h), gravity(g) {}
    LayoutParams(int16_t w, int16_t h, float wt, int g = Gravity::TOP | Gravity::LEFT)
        : width(w), height(h), gravity(g), weight(wt) {}

    LayoutParams& setMargins(int16_t l, int16_t t, int16_t r, int16_t b) {
        marginL = l; marginT = t; marginR = r; marginB = b;
        return *this;
    }

    virtual ~LayoutParams() = default;
};

// Short aliases used at call sites.
constexpr int16_t MP = LayoutParams::MATCH_PARENT;
constexpr int16_t WC = LayoutParams::WRAP_CONTENT;

// Fluent, owning builder — pass to ViewGroup::addView(child, Lp(...)).
// Example: addView(v, Lp(WC, WC).gravity(Gravity::CENTER).margins(8, 0, 8, 0));
class Lp {
public:
    explicit Lp(int16_t w = WC, int16_t h = WC)
        : mParams(new LayoutParams(w, h)) {}

    Lp(int16_t w, int16_t h, float weight)
        : mParams(new LayoutParams(w, h, weight)) {}

    Lp(Lp&& o) noexcept : mParams(o.mParams) { o.mParams = nullptr; }
    Lp(const Lp&) = delete;
    Lp& operator=(const Lp&) = delete;
    Lp& operator=(Lp&& o) noexcept {
        if (this != &o) {
            delete mParams;
            mParams = o.mParams;
            o.mParams = nullptr;
        }
        return *this;
    }

    ~Lp() { delete mParams; }

    Lp&& gravity(int g) && {
        mParams->gravity = g;
        return static_cast<Lp&&>(*this);
    }
    Lp&& margins(int16_t l, int16_t t, int16_t r, int16_t b) && {
        mParams->setMargins(l, t, r, b);
        return static_cast<Lp&&>(*this);
    }
    Lp&& weight(float w) && {
        mParams->weight = w;
        return static_cast<Lp&&>(*this);
    }

    LayoutParams* release() {
        LayoutParams* p = mParams;
        mParams = nullptr;
        return p;
    }

private:
    LayoutParams* mParams;
};

} // namespace litho
