#pragma once
#include "framework/view/view.hpp"

namespace litho {

// Horizontal slider — value in [min, max], drag thumb to change.
class Slider : public View {
public:
    using ChangeFn = void (*)(float value, void* user);

    Slider() {
        mBounds.height = 36;
    }

    void setRange(float minV, float maxV) {
        if (maxV < minV) { float t = minV; minV = maxV; maxV = t; }
        mMin = minV;
        mMax = maxV;
        if (mValue < mMin) mValue = mMin;
        if (mValue > mMax) mValue = mMax;
        invalidate();
    }

    void setValue(float v) {
        if (v < mMin) v = mMin;
        if (v > mMax) v = mMax;
        if (v == mValue) return;
        mValue = v;
        invalidate();
    }
    float value() const { return mValue; }

    void setTrackColor(RGB565 c) { mTrack = c; invalidate(); }
    void setFillColor(RGB565 c)  { mFill  = c; invalidate(); }
    void setThumbColor(RGB565 c) { mThumb = c; invalidate(); }

    void setOnChange(ChangeFn cb, void* user) {
        mCb = cb;
        mUser = user;
    }

protected:
    void onMeasure(int32_t widthMeasureSpec, int32_t heightMeasureSpec) override {
        int tw = mBounds.width > 0 ? mBounds.width : 200;
        int th = mBounds.height > 0 ? mBounds.height : 36;
        if (layoutParams()) {
            if (layoutParams()->width  >= 0) tw = layoutParams()->width;
            if (layoutParams()->height >= 0) th = layoutParams()->height;
        }
        setMeasuredDimension(
            MeasureSpec::resolveSize(tw, widthMeasureSpec),
            MeasureSpec::resolveSize(th, heightMeasureSpec));
    }

public:
    void onDraw(Painter& p) override {
        const int w = mBounds.width;
        const int h = mBounds.height;
        if (w <= 0 || h <= 0) return;

        const int trackH = 6;
        const int trackY = (h - trackH) / 2;
        const int pad = mThumbR + 2;
        const int trackW = w - pad * 2;
        if (trackW <= 0) return;

        float t = (mMax > mMin) ? (mValue - mMin) / (mMax - mMin) : 0.f;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
        const int fillW = (int)(t * (float)trackW + 0.5f);
        const int thumbX = pad + fillW;

        p.fillRoundRect(pad, trackY, trackW, trackH, trackH / 2, mTrack);
        if (fillW > 0)
            p.fillRoundRect(pad, trackY, fillW, trackH, trackH / 2, mFill);
        p.fillCircle(thumbX, h / 2, mThumbR, mThumb);
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
        if (ev.action == TouchAction::DOWN || ev.action == TouchAction::MOVE) {
            applyTouchX(ev.x - mTouchSX);
            return true;
        }
        if (ev.action == TouchAction::UP || ev.action == TouchAction::CANCEL)
            return true;
        return false;
    }

private:
    void applyTouchX(int localX) {
        const int w = mBounds.width;
        const int pad = mThumbR + 2;
        const int trackW = w - pad * 2;
        if (trackW <= 0) return;
        float t = (float)(localX - pad) / (float)trackW;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
        float v = mMin + t * (mMax - mMin);
        if (v == mValue) return;
        mValue = v;
        invalidate();
        if (mCb) mCb(mValue, mUser);
    }

    float   mMin = 0.f;
    float   mMax = 1.f;
    float   mValue = 0.f;
    int     mThumbR = 10;
    RGB565  mTrack = RGB565::fromRGB(60, 60, 80);
    RGB565  mFill  = RGB565::fromRGB(80, 160, 255);
    RGB565  mThumb = RGB565::fromRGB(240, 240, 255);
    int     mTouchSX = 0;
    int     mTouchSY = 0;
    ChangeFn mCb = nullptr;
    void*    mUser = nullptr;
};

} // namespace litho
