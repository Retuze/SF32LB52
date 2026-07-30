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

    // Explicit pressed fill; if unset, pressed uses half-brightness of backgroundColor().
    void setPressedColor(RGB565 color) {
        mPressedColor = color;
        mHasPressedColor = true;
        invalidate();
    }
    void clearPressedColor() {
        mHasPressedColor = false;
        invalidate();
    }
    bool hasPressedColor() const { return mHasPressedColor; }

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
        requestLayout();
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

protected:
    void onMeasure(int32_t widthMeasureSpec, int32_t heightMeasureSpec) override {
        int tw = mBounds.width;
        int th = mBounds.height;
        if (tw <= 0 || th <= 0) {
            int textH = 0;
            int textW = (mText[0] != '\0') ? Painter::measureText(mText, &textH) : 0;
            if (mBgImage < IMG_COUNT) {
                const ImageEntry* e = imageEntry(mBgImage);
                if (tw <= 0) tw = (int)e->width;
                if (th <= 0) th = (int)e->height;
            }
            if (tw <= 0) tw = textW + 24;
            if (th <= 0) th = (textH > 0 ? textH : 16) + 16;
            if (tw < 48) tw = 48;
            if (th < 36) th = 36;
        }
        if (layoutParams()) {
            if (layoutParams()->width  >= 0) tw = layoutParams()->width;
            if (layoutParams()->height >= 0) th = layoutParams()->height;
        }
        setMeasuredDimension(
            MeasureSpec::resolveSize(tw, widthMeasureSpec),
            MeasureSpec::resolveSize(th, heightMeasureSpec));
    }

public:
    // ---- draw ----

    void onDraw(Painter& p) override {
        const int w = mBounds.width;
        const int h = mBounds.height;
        if (w <= 0 || h <= 0) return;

        // Hold-still: reveal pressed after delay (needs another frame while armed).
        updatePressedReveal();

        // 1) Solid fill — View bg, or pressed shade while pressed.
        if (hasBackgroundColor()) {
            if (mPressed)
                p.fillRect(0, 0, w, h, pressedFillColor());
            else
                drawBackground(p);
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

        // Pressed dim overlay for image buttons without a pressed image.
        // Solid-color buttons already use pressedFillColor() — don't double-darken.
        if (mPressed && mPressedImage >= IMG_COUNT && !hasBackgroundColor()) {
            uint8_t savedA = p.alpha();
            p.setAlpha(90);
            p.fillRect(0, 0, w, h, RGB565::Black());
            p.setAlpha(savedA);
        }

        drawBorder(p);

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
            mArmed   = true;
            mInside  = true;
            mPressed = false;
            mDownMs  = View::frameTimeMs();
            // Kick a redraw so updatePressedReveal can run after the delay.
            invalidate();
            return true;
        }
        if (ev.action == TouchAction::MOVE) {
            int lx = ev.x - mTouchSX;
            int ly = ev.y - mTouchSY;
            bool inside = (lx >= 0 && lx < mBounds.width &&
                           ly >= 0 && ly < mBounds.height);
            if (inside != mInside) {
                mInside = inside;
                if (!inside && mPressed) {
                    mPressed = false;
                    invalidate();
                } else if (inside) {
                    invalidate();
                }
            }
            updatePressedReveal();
            return true;
        }
        if (ev.action == TouchAction::UP || ev.action == TouchAction::CANCEL) {
            const bool fire = (ev.action == TouchAction::UP) && mInside && mArmed;
            if (mPressed || mInside || mArmed) {
                mPressed = false;
                mInside  = false;
                mArmed   = false;
                invalidate();
            }
            if (fire && mCallback) mCallback(mUser);
            return true;
        }
        return false;
    }

private:
    static constexpr uint16_t kPressedDelayMs = 100;

    void updatePressedReveal() {
        if (!mArmed || !mInside || mPressed) return;
        uint32_t now = View::frameTimeMs();
        if ((uint32_t)(now - mDownMs) < kPressedDelayMs) {
            // Still waiting — keep frames coming while finger is down.
            invalidate();
            return;
        }
        mPressed = true;
        invalidate();
    }

    static RGB565 halfBrightness(RGB565 c) {
        uint8_t r = (c.value >> 11) & 0x1F;
        uint8_t g = (c.value >> 5)  & 0x3F;
        uint8_t b =  c.value        & 0x1F;
        return RGB565::fromRGB(r * 128 / 31, g * 128 / 63, b * 128 / 31);
    }

    RGB565 pressedFillColor() const {
        return mHasPressedColor ? mPressedColor : halfBrightness(backgroundColor());
    }

    RGB565  mPressedColor    = {0};
    bool    mHasPressedColor = false;
    ImageId mBgImage         = IMG_COUNT;
    ImageId mPressedImage    = IMG_COUNT;
    RGB565  mBgTint          = {0};
    bool    mHasBgTint       = false;

    // Label
    char   mText[kMaxTextLen] = {0};
    RGB565 mTextColor = RGB565::fromRGB(255, 255, 255);

    // Touch / click — pressed UI is delayed so scroll gestures don't flash.
    bool     mPressed = false;
    bool     mArmed   = false;
    bool     mInside  = false;
    uint32_t mDownMs  = 0;
    int      mTouchSX = 0;
    int      mTouchSY = 0;
    void (*mCallback)(void*) = nullptr;
    void*  mUser = nullptr;
};

} // namespace litho
