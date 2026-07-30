#pragma once
#include "framework/view/view.hpp"
#include "res_images.h"

namespace litho {

// Text laid out on a circular arc — each glyph is placed on the baseline
// radius and rotated to follow the tangent (watch-bezel style).
//
// Angle convention matches resource sin/cos: 0° = +X (3 o'clock), clockwise
// positive (y-down screen). Start at 270° for 12 o'clock.
//
// Bounds: covering square of half-extent = radius + max glyph reach from
// baseline pivot (exact over any spin angle) + small AA filter pad.
// Prefer WRAP_CONTENT / intrinsic size so the ring is not clipped.
class ArcTextView : public View {
public:
    static constexpr int kMaxTextLen = 64;

    ArcTextView() {
        syncIntrinsicSize();
    }

    explicit ArcTextView(const char* text) {
        syncIntrinsicSize();
        setText(text);
    }

    void setText(const char* text) {
        if (!text) text = "";
        size_t n = 0;
        while (text[n] && n < (size_t)kMaxTextLen - 1) {
            mText[n] = text[n];
            ++n;
        }
        mText[n] = '\0';
        if (!mExplicitSize) syncIntrinsicSize();
        invalidate();
    }
    const char* text() const { return mText; }

    void setTextColor(RGB565 c) { mColor = c; invalidate(); }
    RGB565 textColor() const { return mColor; }

    // Baseline circle radius in local pixels.
    void setRadius(int r) {
        if (r < 1) r = 1;
        if (r == mRadius) return;
        mRadius = r;
        if (!mExplicitSize) syncIntrinsicSize();
        invalidate();
    }
    int radius() const { return mRadius; }

    // Arc center in local coords. Default = view center.
    void setCenter(int cx, int cy) {
        mCx = cx;
        mCy = cy;
        mCenterSet = true;
        invalidate();
    }
    void clearCenter() { mCenterSet = false; invalidate(); }

    void setStartAngleDeci(int deci) {
        mStartDeci = deci;
        invalidate();
    }
    void setStartAngle(int16_t degrees) {
        setStartAngleDeci((int)degrees * 10);
    }
    int startAngleDeci() const { return mStartDeci; }

    void setClockwise(bool cw) {
        if (cw == mClockwise) return;
        mClockwise = cw;
        invalidate();
    }
    bool clockwise() const { return mClockwise; }

    // true: readable from outside (bezel); false: readable from center.
    void setReadableFromOutside(bool v) {
        if (v == mOutside) return;
        mOutside = v;
        if (!mExplicitSize) syncIntrinsicSize();
        invalidate();
    }
    bool readableFromOutside() const { return mOutside; }

    // Extra pixels inserted between glyphs along the arc.
    void setLetterSpacing(int px) {
        mLetterSpacing = px;
        invalidate();
    }
    int letterSpacing() const { return mLetterSpacing; }

    // Continuous spin around the circle (driven by View::frameTimeMs).
    void setSpinning(bool on) {
        if (on == mSpinning) return;
        mSpinning = on;
        if (on) mSpinEpochMs = View::frameTimeMs();
        invalidate();
    }
    bool spinning() const { return mSpinning; }

    // Positive = same direction as clockwise(); units: degrees / second.
    void setSpinDegreesPerSec(int degPerSec) {
        mSpinDegPerSec = degPerSec;
        invalidate();
    }
    int spinDegreesPerSec() const { return mSpinDegPerSec; }

    // Half-size of the square covering the ring at any spin angle:
    // radius + max distance from baseline pivot to any glyph corner (+ AA).
    int contentHalfExtent() const {
        return mRadius + maxGlyphReachFromBaseline() + kFilterPad;
    }

    // Odd span so center = size/2 has equal pixel distance to all four edges
    // (even size leaves bottom/right 1px short and clips AA).
    int contentSpan() const { return 2 * contentHalfExtent() + 1; }

    void onDraw(Painter& p) override {
        View::onDraw(p);
        if (mText[0] == '\0') return;
        if (!fontSection()) return;
        if (!resSinTable()) return;

        int startDeci = mStartDeci;
        int startFrac = 0; // additional 1/1024 deg beyond deci (0..102), for subpixel phase
        if (mSpinning) {
            uint32_t now = View::frameTimeMs();
            int dir = mClockwise ? 1 : -1;
            // Phase in 1/1024 degree: deg/s * ms * 1024 / 1000
            int64_t phase1024 = ((int64_t)(now - mSpinEpochMs) * mSpinDegPerSec * 1024) / 1000;
            int64_t start1024 = (int64_t)mStartDeci * 102 + dir * phase1024;
            // deci = 1/10 deg = 102.4/1024 deg ≈ 102/1024
            startDeci = (int)(start1024 / 102);
            startFrac = (int)(start1024 - (int64_t)startDeci * 102); // 0..101
            if (startFrac < 0) { startFrac += 102; startDeci -= 1; }
            startDeci = ((startDeci % 3600) + 3600) % 3600;
            invalidate();
        }

        const int cx = mCenterSet ? mCx : (mBounds.width  / 2);
        const int cy = mCenterSet ? mCy : (mBounds.height / 2);
        const int r  = mRadius;
        const int dir = mClockwise ? 1 : -1;
        const int tangent = mOutside
            ? (mClockwise ? 900 : -900)
            : (mClockwise ? -900 : 900);

        const char* s = mText;
        const char* end = mText + lithoStrlen(mText);
        // Track angle in 1/1024 deg for smooth arc steps + spin phase.
        int64_t angle1024 = (int64_t)startDeci * 102 + startFrac;

        while (s < end) {
            if (*s == '\n') { ++s; continue; }
            int cp = Painter::utf8Decode(s, end);
            if (cp < 0) continue;

            const GlyphEntry* g = fontFindGlyph((uint32_t)cp);
            if (!g) continue;

            const int adv = (int)g->advance + mLetterSpacing;
            const int step1024 = arcStep1024(adv, r);
            const int64_t aMid1024 = angle1024 + dir * (step1024 / 2);

            int64_t q = aMid1024 / 102;
            int f = (int)(aMid1024 - q * 102);
            if (f < 0) { f += 102; --q; }
            int aMidDeci = (int)(((q % 3600) + 3600) % 3600);
            int a1 = (aMidDeci + 1) % 3600;
            int c0 = cosDeci(aMidDeci), c1 = cosDeci(a1);
            int s0 = sinDeci(aMidDeci), s1 = sinDeci(a1);
            int cosQ15 = c0 + (c1 - c0) * f / 102;
            int sinQ15 = s0 + (s1 - s0) * f / 102;

            // 16.16 baseline: r * cosQ15 / 32768 in Q16 = r * cosQ15 * 2
            const int32_t bxQ16 = ((int32_t)cx << 16)
                                + (int32_t)((int64_t)r * cosQ15 * 2);
            const int32_t byQ16 = ((int32_t)cy << 16)
                                + (int32_t)((int64_t)r * sinQ15 * 2);
            const int rot = aMidDeci + tangent;

            if (g->width > 0 && g->height > 0 && g->size > 0) {
                p.drawGlyphA8Rotated(glyphPixels(g),
                                     (int)g->width, (int)g->height,
                                     bxQ16, byQ16,
                                     (int)g->bearingX, (int)g->bearingY,
                                     rot, mColor);
            }
            angle1024 += dir * step1024;
        }
    }

protected:
    void onMeasure(int32_t widthMeasureSpec, int32_t heightMeasureSpec) override {
        int span = contentSpan();
        int tw = span;
        int th = span;
        if (mExplicitSize) {
            tw = mBounds.width;
            th = mBounds.height;
        }
        if (layoutParams()) {
            if (layoutParams()->width  >= 0) tw = layoutParams()->width;
            if (layoutParams()->height >= 0) th = layoutParams()->height;
        }
        setMeasuredDimension(
            MeasureSpec::resolveSize(tw, widthMeasureSpec),
            MeasureSpec::resolveSize(th, heightMeasureSpec));
    }

private:
    // Arc step in 1/1024 degree (≈ deci * 102).
    static int arcStep1024(int arcPx, int radius) {
        if (radius < 1) radius = 1;
        // 1024/deg * 180/π ≈ 58570 / r  → arcPx * 58570 / r
        return (int)(((int64_t)arcPx * 58570 + radius / 2) / radius);
    }

    // Bilinear + 2×2 SS (±1) + subpixel baseline past geometry.
    static constexpr int kFilterPad = 5;

    static int isqrtCeil(int n) {
        if (n <= 0) return 0;
        int x = n, y = (x + 1) / 2;
        while (y < x) {
            x = y;
            y = (x + n / x) / 2;
        }
        if (x * x < n) ++x;
        return x;
    }

    static int fontMetricReach() {
        const FontSectionHeader* fh = fontSection();
        if (!fh) return 16;
        int a = (int)fh->ascent;
        int d = (int)fh->descent;
        if (d < 0) d = -d;
        return a > d ? a : d;
    }

    // Max distance from baseline origin to any corner of any glyph in mText.
    // Rotation-invariant: covering circle around each pivot is r + this.
    int maxGlyphReachFromBaseline() const {
        const int floorReach = fontMetricReach();
        if (!fontSection() || mText[0] == '\0') return floorReach;

        int maxD2 = 0;
        const char* s = mText;
        const char* end = mText + lithoStrlen(mText);
        while (s < end) {
            if (*s == '\n') { ++s; continue; }
            int cp = Painter::utf8Decode(s, end);
            if (cp < 0) continue;
            const GlyphEntry* g = fontFindGlyph((uint32_t)cp);
            if (!g || g->width <= 0 || g->height <= 0) continue;

            // Bitmap relative to baseline origin (same as drawGlyphA8Rotated).
            const int x0 = (int)g->bearingX;
            const int y0 = -(int)g->bearingY;
            const int x1 = x0 + (int)g->width;
            const int y1 = y0 + (int)g->height;
            const int xs[2] = { x0, x1 };
            const int ys[2] = { y0, y1 };
            for (int i = 0; i < 2; i++) {
                for (int j = 0; j < 2; j++) {
                    int d2 = xs[i] * xs[i] + ys[j] * ys[j];
                    if (d2 > maxD2) maxD2 = d2;
                }
            }
        }
        int reach = isqrtCeil(maxD2);
        return reach > floorReach ? reach : floorReach;
    }

    void syncIntrinsicSize() {
        int s = contentSpan();
        mBounds.width  = (int16_t)s;
        mBounds.height = (int16_t)s;
        requestLayout();
    }

    char   mText[kMaxTextLen] = {0};
    RGB565 mColor = RGB565::fromRGB(255, 255, 255);
    int    mRadius = 100;
    int    mCx = 0, mCy = 0;
    bool   mCenterSet = false;
    int    mStartDeci = 2700; // 12 o'clock
    bool   mClockwise = true;
    bool   mOutside = true;
    int    mLetterSpacing = 2;
    bool   mExplicitSize = false;
    bool   mSpinning = false;
    int    mSpinDegPerSec = 45;
    uint32_t mSpinEpochMs = 0;
};

} // namespace litho
