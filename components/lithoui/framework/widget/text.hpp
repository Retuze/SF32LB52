#pragma once
#include "framework/view/view.hpp"

namespace litho {

class TextView : public View {
public:
    TextView(int w, int h)
        : mWidth(w), mHeight(h) {}

    void onDraw(Painter& p) override {
        // Placeholder: draw a neutral gray rect
        p.fillRect(0, 0, mWidth, mHeight, RGB565::fromRGB(128, 128, 128));
    }

    // TODO: setText(const char*), font rendering

private:
    int mWidth;
    int mHeight;
};

} // namespace litho
