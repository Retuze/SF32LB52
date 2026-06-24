#pragma once
#include "framework/view/view.hpp"

namespace litho {

class Button : public View {
public:
    Button(RGB565 color, int w, int h)
        : mColor(color) {
        uint8_t r = (color.value >> 11) & 0x1F;
        uint8_t g = (color.value >> 5)  & 0x3F;
        uint8_t b =  color.value        & 0x1F;
        mPressedColor = RGB565::fromRGB(r * 128 / 31, g * 64 / 63, b * 128 / 31);
    }

    void onDraw(Painter& p) override {
        p.fillRect(0, 0, mBounds.width, mBounds.height,
                   mPressed ? mPressedColor : mColor);
    }

    bool dispatchTouchEvent(TouchEvent& ev, int screenX, int screenY) override {
        mTouchSX = screenX;
        mTouchSY = screenY;
        if (onTouchEvent(ev)) {
            ev.handler   = this;
            ev.handlerSX = screenX;
            ev.handlerSY = screenY;
            return true;
        }
        return false;
    }

    bool onTouchEvent(TouchEvent& ev) override {
        if (ev.action == TouchAction::DOWN) {
            if (!mPressed) { mPressed = true; invalidate(); }
            mInside = true;
            return true;
        }
        if (ev.action == TouchAction::MOVE) {
            int lx = ev.x - mTouchSX;
            int ly = ev.y - mTouchSY;
            mInside = (lx >= 0 && lx < mBounds.width &&
                       ly >= 0 && ly < mBounds.height);
            return true;
        }
        if (ev.action == TouchAction::UP) {
            if (mPressed) {
                mPressed = false;
                invalidate();
                if (mInside && mCallback) mCallback(mUser);
            }
            return true;
        }
        return false;
    }

    void setOnClick(void (*cb)(void*), void* user) { mCallback = cb; mUser = user; }

private:
    RGB565 mColor;
    RGB565 mPressedColor;
    bool   mPressed   = false;
    bool   mInside    = false;
    int    mTouchSX   = 0;
    int    mTouchSY   = 0;
    void (*mCallback)(void*) = nullptr;
    void*  mUser      = nullptr;
};

} // namespace litho
