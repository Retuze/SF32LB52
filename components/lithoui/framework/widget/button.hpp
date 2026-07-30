#pragma once
#include "framework/view/view.hpp"
#include "res_images.h"

namespace litho {

// Clickable button: optional solid color, optional background image, optional
// centered UTF-8 label. Feedback is either Color (darken) or Ripple (Material-like).
class Button : public View {
public:
    static constexpr int kMaxTextLen = 64;

    enum class Feedback : uint8_t {
        Color  = 0, // darken fill / dim image while pressed
        Ripple = 1, // expanding circle from touch point
    };

    Button() = default;

    Button(int w, int h) {
        mBounds.width  = (int16_t)w;
        mBounds.height = (int16_t)h;
    }

    // Solid-color button (legacy).
    Button(RGB565 color, int w, int h) : Button(w, h) {
        setBackgroundColor(color);
    }

    void setFeedback(Feedback f) {
        if (f == mFeedback) return;
        stopRipple();
        mFeedback = f;
        mPressed  = false;
        invalidate();
    }
    Feedback feedback() const { return mFeedback; }

    // Ripple ink color (drawn with painter alpha). Default: white.
    void setRippleColor(RGB565 color) {
        mRippleColor = color;
        invalidate();
    }
    RGB565 rippleColor() const { return mRippleColor; }

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

    // Corner radius in px (clamped to half the short side when drawing).
    void setCornerRadius(int16_t r) {
        if (r < 0) r = 0;
        if (r == mCornerRadius) return;
        mCornerRadius = r;
        invalidate();
    }
    int16_t cornerRadius() const { return mCornerRadius; }

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

        if (mFeedback == Feedback::Color)
            updatePressedReveal();
        else
            tickRipple();

        // 1) Solid fill — Color mode may darken while pressed.
        if (hasBackgroundColor()) {
            if (mFeedback == Feedback::Color && mPressed)
                fillShape(p, pressedFillColor());
            else
                fillShape(p, backgroundColor());
        }

        // 2) Background image
        ImageId img = mBgImage;
        if (mFeedback == Feedback::Color && mPressed && mPressedImage < IMG_COUNT)
            img = mPressedImage;
        if (img < IMG_COUNT) {
            const ImageEntry* e = imageEntry(img);
            const void* src = imagePixels(img);
            const RGB565* tint = mHasBgTint ? &mBgTint : nullptr;
            int dx = (w - (int)e->width)  / 2;
            int dy = (h - (int)e->height) / 2;
            p.drawImage(src, e->formatInfo, e->width, e->height, dx, dy, tint);
        }

        if (mFeedback == Feedback::Color && mPressed &&
            mPressedImage >= IMG_COUNT && !hasBackgroundColor()) {
            uint8_t savedA = p.alpha();
            p.setAlpha(90);
            fillShape(p, RGB565::Black());
            p.setAlpha(savedA);
        }

        // Material RippleDrawable: one semi-transparent disk expanding from touch.
        if (mFeedback == Feedback::Ripple && mRippleActive) {
            uint8_t savedA = p.alpha();
            const int cr = effectiveCornerRadius();
            if (mOverlayAlpha > 0) {
                uint32_t a = ((uint32_t)savedA * mOverlayAlpha) / 255;
                p.setAlpha((uint8_t)a);
                fillShape(p, mRippleColor);
            }
            if (mRippleRadius > 0 && mRippleAlpha > 0) {
                uint32_t a = ((uint32_t)savedA * mRippleAlpha) / 255;
                p.setAlpha((uint8_t)a);
                if (cr > 0)
                    p.fillCircle(mRippleCx, mRippleCy, mRippleRadius, mRippleColor,
                                 0, 0, w, h, cr);
                else
                    p.fillCircle(mRippleCx, mRippleCy, mRippleRadius, mRippleColor);
            }
            p.setAlpha(savedA);
        }

        drawShapeBorder(p);

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
            mArmed  = true;
            mInside = true;
            mDownMs = View::frameTimeMs();
            mRippleCx = (int16_t)(ev.x - mTouchSX);
            mRippleCy = (int16_t)(ev.y - mTouchSY);
            if (mFeedback == Feedback::Color) {
                mPressed = false;
            } else {
                // Delay reveal so scroll can cancel before ink appears.
                mRippleActive  = true;
                mRipplePending = true;
                mRippleExiting = false;
                mRippleRadius  = 0;
                mRippleAlpha   = 0;
                mOverlayAlpha  = 0;
                mEnterStartMs  = 0;
                mExitStartMs   = 0;
                mRippleMaxR    = rippleMaxRadius();
            }
            invalidate();
            return true;
        }
        if (ev.action == TouchAction::MOVE) {
            int lx = ev.x - mTouchSX;
            int ly = ev.y - mTouchSY;
            bool inside = hitLocal(lx, ly);
            if (inside != mInside) {
                mInside = inside;
                if (mFeedback == Feedback::Color) {
                    if (!inside && mPressed) {
                        mPressed = false;
                        invalidate();
                    } else if (inside) {
                        invalidate();
                    }
                } else if (!inside && mRippleActive && !mRippleExiting) {
                    beginRippleExit();
                }
            }
            if (mFeedback == Feedback::Color)
                updatePressedReveal();
            else
                tickRipple();
            return true;
        }
        if (ev.action == TouchAction::UP || ev.action == TouchAction::CANCEL) {
            const bool fire = (ev.action == TouchAction::UP) && mInside && mArmed;
            mArmed  = false;
            mInside = false;

            if (mFeedback == Feedback::Color) {
                if (mPressed) {
                    mPressed = false;
                    invalidate();
                }
            } else {
                if (ev.action == TouchAction::CANCEL) {
                    if (mRipplePending)
                        stopRipple();
                    else
                        beginRippleExit();
                } else if (mRipplePending) {
                    // Quick tap: start enter and exit together (wave still expands).
                    startRippleEnter();
                    beginRippleExit();
                } else {
                    beginRippleExit();
                }
            }

            if (fire && mCallback) mCallback(mUser);
            return true;
        }
        return false;
    }

private:
    static constexpr uint16_t kPressedDelayMs   = 100;
    // Material-ish: enter expands radius; exit only fades opacity.
    static constexpr uint16_t kRippleEnterMs    = 300;
    static constexpr uint16_t kRippleExitMs     = 240;
    static constexpr uint8_t  kRipplePeakAlpha  = 100;
    static constexpr uint8_t  kOverlayPeakAlpha = 28;

    int effectiveCornerRadius() const {
        int r = mCornerRadius;
        int maxR = mBounds.width < mBounds.height ? mBounds.width / 2
                                                  : mBounds.height / 2;
        if (r > maxR) r = maxR;
        return r;
    }

    bool hitLocal(int lx, int ly) const {
        return Painter::pointInRoundRect(lx, ly, mBounds.width, mBounds.height,
                                         effectiveCornerRadius());
    }

    void fillShape(Painter& p, RGB565 c) const {
        const int w = mBounds.width;
        const int h = mBounds.height;
        const int r = effectiveCornerRadius();
        if (r > 0) p.fillRoundRect(0, 0, w, h, r, c);
        else       p.fillRect(0, 0, w, h, c);
    }

    void drawShapeBorder(Painter& p) const {
        const int bl = borderLeft(), bt = borderTop();
        const int br = borderRight(), bb = borderBottom();
        if (bl == 0 && bt == 0 && br == 0 && bb == 0) return;
        const int w = mBounds.width;
        const int h = mBounds.height;
        const int r = effectiveCornerRadius();
        if (r > 0 && bl == bt && bt == br && br == bb) {
            p.strokeRoundRect(0, 0, w, h, r, bl, borderColor());
        } else {
            const RGB565 c = borderColor();
            if (bt > 0) p.fillRect(0, 0, w, bt, c);
            if (bb > 0) p.fillRect(0, h - bb, w, bb, c);
            if (bl > 0) p.fillRect(0, bt, bl, h - bt - bb, c);
            if (br > 0) p.fillRect(w - br, bt, br, h - bt - bb, c);
        }
    }

    void updatePressedReveal() {
        if (!mArmed || !mInside || mPressed) return;
        uint32_t now = View::frameTimeMs();
        if ((uint32_t)(now - mDownMs) < kPressedDelayMs) {
            invalidate();
            return;
        }
        mPressed = true;
        invalidate();
    }

    int rippleMaxRadius() const {
        const int w = mBounds.width;
        const int h = mBounds.height;
        int cx = mRippleCx;
        int cy = mRippleCy;
        auto dist2 = [](int x, int y) { return x * x + y * y; };
        int d = dist2(cx, cy);
        int t = dist2(w - cx, cy);      if (t > d) d = t;
        t = dist2(cx, h - cy);          if (t > d) d = t;
        t = dist2(w - cx, h - cy);      if (t > d) d = t;
        d = d + (d >> 4) + 4;
        int lo = 0, hi = (w > h ? w : h) * 2 + 16, r = 0;
        while (lo <= hi) {
            int mid = (lo + hi) >> 1;
            if (mid * mid <= d) { r = mid; lo = mid + 1; }
            else hi = mid - 1;
        }
        return r > 0 ? r : 1;
    }

    static float easeOutCubic(float t) {
        if (t <= 0.f) return 0.f;
        if (t >= 1.f) return 1.f;
        float u = 1.f - t;
        return 1.f - u * u * u;
    }

    static float easeOutQuint(float t) {
        if (t <= 0.f) return 0.f;
        if (t >= 1.f) return 1.f;
        float u = 1.f - t;
        return 1.f - u * u * u * u * u;
    }

    void startRippleEnter() {
        mRipplePending = false;
        mRippleActive  = true;
        mRippleMaxR    = rippleMaxRadius();
        mEnterStartMs  = View::frameTimeMs();
        mRippleRadius  = 4;
        mRippleAlpha   = kRipplePeakAlpha;
        mOverlayAlpha  = 0;
        invalidate();
    }

    void beginRippleExit() {
        if (!mRippleActive || mRippleExiting) return;
        if (mRipplePending) {
            stopRipple();
            return;
        }
        mRippleExiting = true;
        mExitStartMs   = View::frameTimeMs();
        invalidate();
    }

    void stopRipple() {
        mRippleActive  = false;
        mRipplePending = false;
        mRippleExiting = false;
        mRippleRadius  = 0;
        mRippleAlpha   = 0;
        mOverlayAlpha  = 0;
        invalidate();
    }

    void tickRipple() {
        if (!mRippleActive) return;

        uint32_t now = View::frameTimeMs();

        if (mRipplePending) {
            if (!mArmed || !mInside) return;
            if ((uint32_t)(now - mDownMs) < kPressedDelayMs) {
                invalidate();
                return;
            }
            startRippleEnter();
        }

        // Enter track: radius always runs to max (even while exiting).
        float enterT = 1.f;
        if (mEnterStartMs != 0) {
            enterT = (kRippleEnterMs > 0)
                         ? (float)(now - mEnterStartMs) / (float)kRippleEnterMs
                         : 1.f;
            if (enterT > 1.f) enterT = 1.f;
        }
        float enterE = easeOutQuint(enterT);
        mRippleRadius = 4 + (int)(enterE * (float)(mRippleMaxR - 4) + 0.5f);
        if (mRippleRadius > mRippleMaxR) mRippleRadius = mRippleMaxR;

        float alphaScale = 1.f;
        if (enterT < 0.12f)
            alphaScale = enterT / 0.12f;

        if (mRippleExiting) {
            float exitT = (kRippleExitMs > 0)
                              ? (float)(now - mExitStartMs) / (float)kRippleExitMs
                              : 1.f;
            if (exitT > 1.f) exitT = 1.f;
            alphaScale *= (1.f - easeOutCubic(exitT));
            mOverlayAlpha = (uint8_t)((float)kOverlayPeakAlpha * alphaScale + 0.5f);
            mRippleAlpha  = (uint8_t)((float)kRipplePeakAlpha * alphaScale + 0.5f);
            // Wait until the wave has finished covering AND fade is done.
            if (exitT >= 1.f && enterT >= 1.f) {
                stopRipple();
                return;
            }
        } else {
            mRippleAlpha = (uint8_t)((float)kRipplePeakAlpha * alphaScale + 0.5f);
            float wash = enterE > 0.55f ? (enterE - 0.55f) / 0.45f : 0.f;
            mOverlayAlpha = (uint8_t)(wash * (float)kOverlayPeakAlpha + 0.5f);
            if (enterT >= 1.f)
                return; // held steady — stop requesting frames
        }

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

    Feedback mFeedback = Feedback::Color;
    int16_t  mCornerRadius = 0;

    RGB565  mPressedColor    = {0};
    bool    mHasPressedColor = false;
    ImageId mBgImage         = IMG_COUNT;
    ImageId mPressedImage    = IMG_COUNT;
    RGB565  mBgTint          = {0};
    bool    mHasBgTint       = false;

    char   mText[kMaxTextLen] = {0};
    RGB565 mTextColor = RGB565::fromRGB(255, 255, 255);

    bool     mPressed = false;
    bool     mArmed   = false;
    bool     mInside  = false;
    uint32_t mDownMs  = 0;
    int      mTouchSX = 0;
    int      mTouchSY = 0;
    void (*mCallback)(void*) = nullptr;
    void*  mUser = nullptr;

    // Dual-track: enter grows radius; exit only fades alpha (Android RippleDrawable).
    bool     mRippleActive  = false;
    bool     mRipplePending = false;
    bool     mRippleExiting = false;
    int16_t  mRippleCx      = 0;
    int16_t  mRippleCy      = 0;
    int      mRippleRadius  = 0;
    int      mRippleMaxR    = 0;
    uint8_t  mRippleAlpha   = 0;
    uint8_t  mOverlayAlpha  = 0;
    uint32_t mEnterStartMs  = 0;
    uint32_t mExitStartMs   = 0;
    RGB565   mRippleColor   = RGB565::White();
};

} // namespace litho
