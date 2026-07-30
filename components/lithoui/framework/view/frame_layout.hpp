#pragma once
#include "framework/view/view_group.hpp"

namespace litho {

// Stacks children in the content box (bounds − border − padding);
// positions via LayoutParams::gravity.
class FrameLayout : public ViewGroup {
public:
    struct LayoutParams : public litho::LayoutParams {
        LayoutParams() = default;
        LayoutParams(int16_t w, int16_t h) : litho::LayoutParams(w, h) {}
        LayoutParams(int16_t w, int16_t h, int g) : litho::LayoutParams(w, h, g) {}
    };

    static LayoutParams* lp(int16_t w, int16_t h, int gravity = Gravity::TOP | Gravity::LEFT) {
        return new LayoutParams(w, h, gravity);
    }

protected:
    void onMeasure(int32_t widthMeasureSpec, int32_t heightMeasureSpec) override {
        int maxW = 0, maxH = 0;
        for (uint16_t i = 0; i < childCount(); i++) {
            View* child = childAt(i);
            if (!child || !child->visible()) continue;
            measureChild(child, widthMeasureSpec, heightMeasureSpec);
            litho::LayoutParams* base = child->layoutParams();
            int ml = base ? base->marginL : 0;
            int mt = base ? base->marginT : 0;
            int mr = base ? base->marginR : 0;
            int mb = base ? base->marginB : 0;
            int cw = child->measuredWidth()  + ml + mr;
            int ch = child->measuredHeight() + mt + mb;
            if (cw > maxW) maxW = cw;
            if (ch > maxH) maxH = ch;
        }
        maxW += insetHorizontal();
        maxH += insetVertical();
        if (maxW < getSuggestedMinimumWidth())  maxW = getSuggestedMinimumWidth();
        if (maxH < getSuggestedMinimumHeight()) maxH = getSuggestedMinimumHeight();

        setMeasuredDimension(
            MeasureSpec::resolveSize(maxW, widthMeasureSpec),
            MeasureSpec::resolveSize(maxH, heightMeasureSpec));
    }

    void onLayout(bool changed, int left, int top, int right, int bottom) override {
        (void)changed;
        const int parentW = right - left;
        const int parentH = bottom - top;
        const int boxL = insetLeft();
        const int boxT = insetTop();
        const int boxR = parentW - insetRight();
        const int boxB = parentH - insetBottom();
        const int boxW = boxR - boxL;
        const int boxH = boxB - boxT;

        for (uint16_t i = 0; i < childCount(); i++) {
            View* child = childAt(i);
            if (!child || !child->visible()) continue;

            litho::LayoutParams* base = child->layoutParams();
            int ml = base ? base->marginL : 0;
            int mt = base ? base->marginT : 0;
            int mr = base ? base->marginR : 0;
            int mb = base ? base->marginB : 0;
            int g  = base ? base->gravity : (Gravity::TOP | Gravity::LEFT);

            int cw = child->measuredWidth();
            int ch = child->measuredHeight();
            int cl = boxL + ml;
            int ct = boxT + mt;
            const int availW = boxW - ml - mr;
            const int availH = boxH - mt - mb;

            if (g & Gravity::CENTER_H) {
                cl = boxL + ml + (availW - cw) / 2;
            } else if (g & Gravity::RIGHT) {
                cl = boxR - mr - cw;
            }
            if (g & Gravity::CENTER_V) {
                ct = boxT + mt + (availH - ch) / 2;
            } else if (g & Gravity::BOTTOM) {
                ct = boxB - mb - ch;
            }

            child->layout(cl, ct, cl + cw, ct + ch);
        }
    }
};

} // namespace litho
