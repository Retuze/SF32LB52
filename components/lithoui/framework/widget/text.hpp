#pragma once
#include "framework/view/view.hpp"
#include "res_images.h"

namespace litho {

class TextView : public View {
public:
    static constexpr int kMaxTextLen = 128;

    TextView() = default;

    explicit TextView(const char* text) {
        setText(text);
    }

    TextView(int w, int h) : mExplicitSize(true) {
        mBounds.width  = (int16_t)w;
        mBounds.height = (int16_t)h;
    }

    void setText(const char* text) {
        if (!text) text = "";
        size_t n = 0;
        while (text[n] && n < (size_t)kMaxTextLen - 1) {
            mText[n] = text[n];
            ++n;
        }
        mText[n] = '\0';
        if (!mExplicitSize) {
            int h = 0;
            int w = Painter::measureText(mText, &h);
            mBounds.width  = (int16_t)w;
            mBounds.height = (int16_t)h;
        }
        invalidate();
    }

    const char* text() const { return mText; }

    void setTextColor(RGB565 color) {
        mColor = color;
        invalidate();
    }
    RGB565 textColor() const { return mColor; }

    // Intrinsic size from glyph advances / lineHeight.
    void measure(int& outW, int& outH) const {
        outW = Painter::measureText(mText, &outH);
    }

    void onDraw(Painter& p) override {
        if (mText[0] == '\0') return;
        if (!fontSection()) return;
        p.drawText(mText, 0, 0, mColor);
    }

private:
    char   mText[kMaxTextLen] = {0};
    RGB565 mColor = RGB565::fromRGB(255, 255, 255);
    bool   mExplicitSize = false;
};

} // namespace litho
