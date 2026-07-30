#pragma once
#include "framework/view/view_group.hpp"

namespace litho {

// Lays out children in a single row or column.
class LinearLayout : public ViewGroup {
public:
    enum Orientation : uint8_t { HORIZONTAL = 0, VERTICAL = 1 };

    LinearLayout() = default;
    explicit LinearLayout(Orientation o) : mOrientation(o) {}

    struct LayoutParams : public litho::LayoutParams {
        LayoutParams() = default;
        LayoutParams(int16_t w, int16_t h) : litho::LayoutParams(w, h) {}
        LayoutParams(int16_t w, int16_t h, float wt)
            : litho::LayoutParams(w, h, wt) {}
        LayoutParams(int16_t w, int16_t h, float wt, int g)
            : litho::LayoutParams(w, h, wt, g) {}
    };

    static LayoutParams* lp(int16_t w, int16_t h, float weight = 0.f,
                            int gravity = Gravity::TOP | Gravity::LEFT) {
        return new LayoutParams(w, h, weight, gravity);
    }

    void setOrientation(Orientation o) {
        if (o == mOrientation) return;
        mOrientation = o;
        requestLayout();
    }
    Orientation orientation() const { return mOrientation; }

    // Extra space between children (in addition to margins).
    void setGap(int16_t gap) {
        if (gap == mGap) return;
        mGap = gap;
        requestLayout();
    }
    int gap() const { return mGap; }

    // Default gravity for the pack of children inside leftover space.
    void setGravity(int g) {
        if (g == mGravity) return;
        mGravity = g;
        requestLayout();
    }
    int gravity() const { return mGravity; }

protected:
    void onMeasure(int32_t widthMeasureSpec, int32_t heightMeasureSpec) override {
        if (mOrientation == VERTICAL)
            measureVertical(widthMeasureSpec, heightMeasureSpec);
        else
            measureHorizontal(widthMeasureSpec, heightMeasureSpec);
    }

    void onLayout(bool changed, int left, int top, int right, int bottom) override {
        (void)changed;
        if (mOrientation == VERTICAL)
            layoutVertical(left, top, right, bottom);
        else
            layoutHorizontal(left, top, right, bottom);
    }

private:
    void measureVertical(int32_t widthMeasureSpec, int32_t heightMeasureSpec) {
        const int widthMode  = MeasureSpec::getMode(widthMeasureSpec);
        const int heightMode = MeasureSpec::getMode(heightMeasureSpec);
        const int widthSize  = MeasureSpec::getSize(widthMeasureSpec);
        const int heightSize = MeasureSpec::getSize(heightMeasureSpec);

        int totalLength = 0;
        int maxWidth = 0;
        float totalWeight = 0.f;
        int weightedCount = 0;
        int childCountVis = 0;

        // Pass 1: measure non-weight children; skip weight size on main axis.
        for (uint16_t i = 0; i < childCount(); i++) {
            View* child = childAt(i);
            if (!child || !child->visible()) continue;
            childCountVis++;

            litho::LayoutParams* base = child->layoutParams();
            float wgt = base ? base->weight : 0.f;
            int ml = base ? base->marginL : 0;
            int mt = base ? base->marginT : 0;
            int mr = base ? base->marginR : 0;
            int mb = base ? base->marginB : 0;

            if (wgt > 0.f) {
                totalWeight += wgt;
                weightedCount++;
                // Measure with UNSPECIFIED height so WRAP can report intrinsic;
                // weighted size assigned in pass 2.
                int heightDim = base ? base->height : LayoutParams::WRAP_CONTENT;
                if (heightDim == LayoutParams::MATCH_PARENT)
                    heightDim = LayoutParams::WRAP_CONTENT;
                child->measure(
                    getChildMeasureSpec(widthMeasureSpec,
                        insetHorizontal() + ml + mr, base ? base->width : LayoutParams::WRAP_CONTENT),
                    MeasureSpec::make(0, MeasureSpec::UNSPECIFIED));
                totalLength += mt + mb; // weight child contributes margins only for now
            } else {
                measureChildWithMargins(child, widthMeasureSpec, 0, heightMeasureSpec, totalLength);
                totalLength += child->measuredHeight() + mt + mb;
            }

            int cw = child->measuredWidth() + ml + mr;
            if (cw > maxWidth) maxWidth = cw;
        }

        if (childCountVis > 1 && mGap > 0)
            totalLength += mGap * (childCountVis - 1);

        totalLength += insetVertical();
        maxWidth += insetHorizontal();

        int height = totalLength;
        if (height < getSuggestedMinimumHeight()) height = getSuggestedMinimumHeight();
        int width = maxWidth;
        if (width < getSuggestedMinimumWidth()) width = getSuggestedMinimumWidth();

        width  = MeasureSpec::resolveSize(width, widthMeasureSpec);
        height = MeasureSpec::resolveSize(height, heightMeasureSpec);

        // Pass 2: distribute leftover height to weighted children.
        if (totalWeight > 0.f && heightMode != MeasureSpec::UNSPECIFIED) {
            int remaining = height - totalLength;
            // totalLength already includes non-weight sizes + all margins + gap + pad.
            // Weighted children were counted as margins only ??add their share.
            // Recompute consumed by non-weight properly:
            int consumed = insetVertical();
            int visIdx = 0;
            for (uint16_t i = 0; i < childCount(); i++) {
                View* child = childAt(i);
                if (!child || !child->visible()) continue;
                if (visIdx++ > 0 && mGap > 0) consumed += mGap;
                litho::LayoutParams* base = child->layoutParams();
                float wgt = base ? base->weight : 0.f;
                int mt = base ? base->marginT : 0;
                int mb = base ? base->marginB : 0;
                if (wgt > 0.f) consumed += mt + mb;
                else consumed += child->measuredHeight() + mt + mb;
            }
            remaining = height - consumed;
            if (remaining < 0) remaining = 0;

            for (uint16_t i = 0; i < childCount(); i++) {
                View* child = childAt(i);
                if (!child || !child->visible()) continue;
                litho::LayoutParams* base = child->layoutParams();
                float wgt = base ? base->weight : 0.f;
                if (wgt <= 0.f) continue;
                int ml = base ? base->marginL : 0;
                int mr = base ? base->marginR : 0;
                int share = (int)((float)remaining * (wgt / totalWeight));
                child->measure(
                    getChildMeasureSpec(widthMeasureSpec,
                        insetHorizontal() + ml + mr,
                        base ? base->width : LayoutParams::WRAP_CONTENT),
                    MeasureSpec::make(share, MeasureSpec::EXACTLY));
                int cw = child->measuredWidth() + ml + mr;
                if (cw + insetHorizontal() > maxWidth)
                    maxWidth = cw + insetHorizontal();
            }
            width = MeasureSpec::resolveSize(maxWidth, widthMeasureSpec);
        }

        (void)widthMode;
        (void)widthSize;
        (void)heightSize;
        (void)weightedCount;
        setMeasuredDimension(width, height);
    }

    void measureHorizontal(int32_t widthMeasureSpec, int32_t heightMeasureSpec) {
        // Mirror of vertical with axes swapped.
        const int widthMode = MeasureSpec::getMode(widthMeasureSpec);

        int totalLength = 0;
        int maxHeight = 0;
        float totalWeight = 0.f;
        int childCountVis = 0;

        for (uint16_t i = 0; i < childCount(); i++) {
            View* child = childAt(i);
            if (!child || !child->visible()) continue;
            childCountVis++;

            litho::LayoutParams* base = child->layoutParams();
            float wgt = base ? base->weight : 0.f;
            int ml = base ? base->marginL : 0;
            int mt = base ? base->marginT : 0;
            int mr = base ? base->marginR : 0;
            int mb = base ? base->marginB : 0;

            if (wgt > 0.f) {
                totalWeight += wgt;
                child->measure(
                    MeasureSpec::make(0, MeasureSpec::UNSPECIFIED),
                    getChildMeasureSpec(heightMeasureSpec,
                        insetVertical() + mt + mb,
                        base ? base->height : LayoutParams::WRAP_CONTENT));
                totalLength += ml + mr;
            } else {
                measureChildWithMargins(child, widthMeasureSpec, totalLength, heightMeasureSpec, 0);
                totalLength += child->measuredWidth() + ml + mr;
            }
            int ch = child->measuredHeight() + mt + mb;
            if (ch > maxHeight) maxHeight = ch;
        }

        if (childCountVis > 1 && mGap > 0)
            totalLength += mGap * (childCountVis - 1);

        totalLength += insetHorizontal();
        maxHeight += insetVertical();

        int width = totalLength;
        if (width < getSuggestedMinimumWidth()) width = getSuggestedMinimumWidth();
        int height = maxHeight;
        if (height < getSuggestedMinimumHeight()) height = getSuggestedMinimumHeight();

        width  = MeasureSpec::resolveSize(width, widthMeasureSpec);
        height = MeasureSpec::resolveSize(height, heightMeasureSpec);

        if (totalWeight > 0.f && widthMode != MeasureSpec::UNSPECIFIED) {
            int consumed = insetHorizontal();
            int visIdx = 0;
            for (uint16_t i = 0; i < childCount(); i++) {
                View* child = childAt(i);
                if (!child || !child->visible()) continue;
                if (visIdx++ > 0 && mGap > 0) consumed += mGap;
                litho::LayoutParams* base = child->layoutParams();
                float wgt = base ? base->weight : 0.f;
                int ml = base ? base->marginL : 0;
                int mr = base ? base->marginR : 0;
                if (wgt > 0.f) consumed += ml + mr;
                else consumed += child->measuredWidth() + ml + mr;
            }
            int remaining = width - consumed;
            if (remaining < 0) remaining = 0;

            for (uint16_t i = 0; i < childCount(); i++) {
                View* child = childAt(i);
                if (!child || !child->visible()) continue;
                litho::LayoutParams* base = child->layoutParams();
                float wgt = base ? base->weight : 0.f;
                if (wgt <= 0.f) continue;
                int mt = base ? base->marginT : 0;
                int mb = base ? base->marginB : 0;
                int share = (int)((float)remaining * (wgt / totalWeight));
                child->measure(
                    MeasureSpec::make(share, MeasureSpec::EXACTLY),
                    getChildMeasureSpec(heightMeasureSpec,
                        insetVertical() + mt + mb,
                        base ? base->height : LayoutParams::WRAP_CONTENT));
            }
        }

        setMeasuredDimension(width, height);
    }

    void layoutVertical(int left, int top, int right, int bottom) {
        const int parentW = right - left;
        const int parentH = bottom - top;
        const int boxL = insetLeft();
        const int boxT = insetTop();
        const int boxR = parentW - insetRight();
        const int boxW = boxR - boxL;

        // Android: child layout_gravity is cross-axis only (H). Main axis is sequential.
        int contentH = 0;
        int vis = 0;
        for (uint16_t i = 0; i < childCount(); i++) {
            View* child = childAt(i);
            if (!child || !child->visible()) continue;
            litho::LayoutParams* base = child->layoutParams();
            int mt = base ? base->marginT : 0;
            int mb = base ? base->marginB : 0;
            if (vis++ > 0 && mGap > 0) contentH += mGap;
            contentH += child->measuredHeight() + mt + mb;
        }
        int availH = parentH - insetTop() - insetBottom();
        int childTop = boxT;
        // Parent android:gravity ? pack the whole row/column when leftover exists.
        if ((mGravity & Gravity::CENTER_V) && contentH < availH)
            childTop = boxT + (availH - contentH) / 2;
        else if ((mGravity & Gravity::BOTTOM) && contentH < availH)
            childTop = boxT + (availH - contentH);

        vis = 0;
        for (uint16_t i = 0; i < childCount(); i++) {
            View* child = childAt(i);
            if (!child || !child->visible()) continue;
            if (vis++ > 0 && mGap > 0) childTop += mGap;

            litho::LayoutParams* base = child->layoutParams();
            int ml = base ? base->marginL : 0;
            int mt = base ? base->marginT : 0;
            int mr = base ? base->marginR : 0;
            int mb = base ? base->marginB : 0;
            int g  = base ? base->gravity : (Gravity::TOP | Gravity::LEFT);

            int cw = child->measuredWidth();
            int ch = child->measuredHeight();
            int cl = boxL + ml;
            const int availW = boxW - ml - mr;
            if (g & Gravity::CENTER_H) cl = boxL + ml + (availW - cw) / 2;
            else if (g & Gravity::RIGHT) cl = boxR - mr - cw;

            childTop += mt;
            child->layout(cl, childTop, cl + cw, childTop + ch);
            childTop += ch + mb;
        }
    }

    void layoutHorizontal(int left, int top, int right, int bottom) {
        const int parentW = right - left;
        const int parentH = bottom - top;
        const int boxL = insetLeft();
        const int boxT = insetTop();
        const int boxB = parentH - insetBottom();
        const int boxH = boxB - boxT;

        // Android: child layout_gravity is cross-axis only (V). Main axis is sequential.
        int contentW = 0;
        int vis = 0;
        for (uint16_t i = 0; i < childCount(); i++) {
            View* child = childAt(i);
            if (!child || !child->visible()) continue;
            litho::LayoutParams* base = child->layoutParams();
            int ml = base ? base->marginL : 0;
            int mr = base ? base->marginR : 0;
            if (vis++ > 0 && mGap > 0) contentW += mGap;
            contentW += child->measuredWidth() + ml + mr;
        }
        int availW = parentW - insetLeft() - insetRight();
        int childLeft = boxL;
        if ((mGravity & Gravity::CENTER_H) && contentW < availW)
            childLeft = boxL + (availW - contentW) / 2;
        else if ((mGravity & Gravity::RIGHT) && contentW < availW)
            childLeft = boxL + (availW - contentW);

        vis = 0;
        for (uint16_t i = 0; i < childCount(); i++) {
            View* child = childAt(i);
            if (!child || !child->visible()) continue;
            if (vis++ > 0 && mGap > 0) childLeft += mGap;

            litho::LayoutParams* base = child->layoutParams();
            int ml = base ? base->marginL : 0;
            int mt = base ? base->marginT : 0;
            int mr = base ? base->marginR : 0;
            int mb = base ? base->marginB : 0;
            int g  = base ? base->gravity : (Gravity::TOP | Gravity::LEFT);

            int cw = child->measuredWidth();
            int ch = child->measuredHeight();
            int ct = boxT + mt;
            const int availH = boxH - mt - mb;
            if (g & Gravity::CENTER_V) ct = boxT + mt + (availH - ch) / 2;
            else if (g & Gravity::BOTTOM) ct = boxB - mb - ch;

            childLeft += ml;
            child->layout(childLeft, ct, childLeft + cw, ct + ch);
            childLeft += cw + mr;
        }
    }

    Orientation mOrientation = VERTICAL;
    int16_t     mGap         = 0;
    int         mGravity     = Gravity::TOP | Gravity::LEFT;
};

} // namespace litho
