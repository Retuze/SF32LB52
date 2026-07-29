#pragma once
#include "framework/view/view.hpp"
#include "res_images.h"

namespace litho {

// Clickable button: optional solid color, optional background image, optional
// centered UTF-8 label. Pressed state darkens the fill and/or swaps to a
// pressed image / dims the normal image.
class Button : public View {
public:
    static constexpr int kMaxTextLen = 64;

    Button() = default;

    Button(int w, int h) {
        mBounds.width  = (int16_t)w;
        mBounds.height = (int16_t)h;
    }

    // Solid-color button (legacy).
    Button(RGB565 color, int w, int h) : Button(w, h) {
        setBackgroundColor(color);
    }

    // ---- background color ----

    void setBackgroundColor(RGB565 color) {
        mBgColor = color;
        mHasBgColor = true;
        uint8_t r = (color.value >> 11) & 0x1F;
        uint8_t g = (color.value >> 5)  & 0x3F;
        uint8_t b =  color.value        & 0x1F;
        mPressedColor = RGB565::fromRGB(r * 128 / 31, g * 128 / 63, b * 128 / 31);
        invalidate();
    }
    void clearBackgroundColor() { mHasBgColor = false; invalidate(); }
    bool hasBackgroundColor() const { return mHasBgColor; }
    RGB565 backgroundColor() const { return mBgColor; }

    void setPressedColor(RGB565 color) {
        mPressedColor = color;
        invalidate();
    }

    // ---- background image ----

    void setBackgroundImage(ImageId id) {
        mBgImage = id;
        if (id < IMG_COUNT && mBounds.width == 0 && mBounds.height == 0) {
            const ImageEntry* e = imageEntry(id);
            mBounds.width  = (int16_t)e->width;
            mBounds.height = (int16_t)e->height;
        }
        invalidate();
    }
    void clearBackgroundImage() {
        mBgImage = IMG_COUNT;
        invalidate();
    }
    ImageId backgroundImage() const { return mBgImage; }

    // Optional image shown while pressed; falls back to dimming mBgImage.
    void setPressedImage(ImageId id) { mPressedImage = id; invalidate(); }
    void clearPressedImage() {
        mPressedImage = IMG_COUNT;
        invalidate();
    }

    // Tint for A8_RLE background images.
    void setBackgroundTint(RGB565 color) {
        mBgTint = color;
        mHasBgTint = true;
        invalidate();
    }
    void clearBackgroundTint() { mHasBgTint = false; invalidate(); }

    // ---- label ----

    void setText(const char* text) {
        if (!text) text = "";
        size_t n = 0;
        while (text[n] && n < (size_t)kMaxTextLen - 1) {
            mText[n] = text[n];
            ++n;
        }
        mText[n] = '\0';
        invalidate();
    }
    const char* text() const { return mText; }

    void setTextColor(RGB565 color) {
        mTextColor = color;
        invalidate();
    }
    RGB565 textColor() const { return mTextColor; }

    // ---- click ----

    void setOnClick(void (*cb)(void*), void* user) {
        mCallback = cb;
        mUser = user;
    }

    // ---- draw ----

    void onDraw(Painter& p) override {
        const int w = mBounds.width;
        const int h = mBounds.height;
        if (w <= 0 || h <= 0) return;

        // 1) Solid fill
        if (mHasBgColor) {
            p.fillRect(0, 0, w, h, mPressed ? mPressedColor : mBgColor);
        }

        // 2) Background image
        ImageId img = mBgImage;
        if (mPressed && mPressedImage < IMG_COUNT) img = mPressedImage;
        if (img < IMG_COUNT) {
            const ImageEntry* e = imageEntry(img);
            const void* src = imagePixels(img);
            const RGB565* tint = mHasBgTint ? &mBgTint : nullptr;
            int dx = (w - (int)e->width)  / 2;
            int dy = (h - (int)e->height) / 2;
            p.drawImage(src, e->formatInfo, e->width, e->height, dx, dy, tint);
        }

        // Pressed dim overlay when no dedicated pressed image (works for any bg).
        if (mPressed && mPressedImage >= IMG_COUNT) {
            uint8_t savedA = p.alpha();
            p.setAlpha(90);
            p.fillRect(0, 0, w, h, RGB565::Black());
            p.setAlpha(savedA);
        }

        // 3) Centered label
        if (mText[0] != '\0' && fontSection()) {
            int th = 0;
            int tw = Painter::measureText(mText, &th);
            int tx = (w - tw) / 2;
            int ty = (h - th) / 2;
            p.drawText(mText, tx, ty, mTextColor);
        }
    }

    // ---- touch ----

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
            bool inside = (lx >= 0 && lx < mBounds.width &&
                           ly >= 0 && ly < mBounds.height);
            if (inside != mInside) {
                mInside = inside;
                mPressed = inside;
                invalidate();
            }
            return true;
        }
        if (ev.action == TouchAction::UP) {
            bool fire = mInside && mPressed;
            if (mPressed || mInside) {
                mPressed = false;
                mInside  = false;
                invalidate();
            }
            if (fire && mCallback) mCallback(mUser);
            return true;
        }
        return false;
    }

private:
    // Background
    RGB565  mBgColor      = {0};
    RGB565  mPressedColor = {0};
    bool    mHasBgColor   = false;
    ImageId mBgImage      = IMG_COUNT;
    ImageId mPressedImage = IMG_COUNT;
    RGB565  mBgTint       = {0};
    bool    mHasBgTint    = false;

    // Label
    char   mText[kMaxTextLen] = {0};
    RGB565 mTextColor = RGB565::fromRGB(255, 255, 255);

    // Touch / click
    bool   mPressed = false;
    bool   mInside  = false;
    int    mTouchSX = 0;
    int    mTouchSY = 0;
    void (*mCallback)(void*) = nullptr;
    void*  mUser = nullptr;
};

} // namespace litho
