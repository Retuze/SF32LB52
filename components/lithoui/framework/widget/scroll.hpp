#pragma once
#include "framework/view/view_group.hpp"

namespace litho {

/* ScrollView — ViewGroup that scrolls its own children vertically.
 *
 * Touch: claims the gesture on DOWN so vertical drags work even when the
 * finger starts on a child (e.g. Button). Children still get press/click
 * if movement stays within the touch slop. */
class ScrollView : public ViewGroup {
public:
    static constexpr int kTouchSlop = 8;

    bool dispatchTouchEvent(TouchEvent& ev, int sx, int sy) override {
        if (ev.action == TouchAction::DOWN) {
            mLastY     = ev.y;
            mDownY     = ev.y;
            mTracking  = false;
            mDidScroll = false;
            mChild     = nullptr;
            mChildSX   = 0;
            mChildSY   = 0;

            for (int i = (int)childCount() - 1; i >= 0; i--) {
                View* child = childAt((uint16_t)i);
                if (!child || !child->visible()) continue;

                Region tb = child->transformedBounds();
                int cx = sx + tb.x;
                int cy = sy + tb.y + mScroll;

                if (ev.x >= cx && ev.x < cx + tb.width &&
                    ev.y >= cy && ev.y < cy + tb.height) {
                    mChild   = child;
                    mChildSX = cx;
                    mChildSY = cy;
                    // Press feedback; reclaim capture for scroll intercept.
                    TouchEvent down = ev;
                    child->dispatchTouchEvent(down, cx, cy);
                    break;
                }
            }

            ev.handler   = this;
            ev.handlerSX = sx;
            ev.handlerSY = sy;
            return true;
        }

        if (ev.action == TouchAction::MOVE) {
            int dy = ev.y - mLastY;
            mLastY = ev.y;
            int total = ev.y - mDownY;
            if (total < 0) total = -total;

            if (!mTracking) {
                if (total > kTouchSlop) {
                    mTracking  = true;
                    mDidScroll = true;
                    cancelChild();
                } else if (mChild) {
                    mChild->dispatchTouchEvent(ev, mChildSX, mChildSY);
                    return true;
                }
            }

            if (mTracking && dy != 0) {
                mScroll += dy;
                clampScroll();
                invalidate();
            }
            return true;
        }

        if (ev.action == TouchAction::UP || ev.action == TouchAction::CANCEL) {
            if (mChild) {
                if (!mDidScroll && ev.action == TouchAction::UP) {
                    mChild->dispatchTouchEvent(ev, mChildSX, mChildSY);
                } else {
                    cancelChild();
                }
                mChild = nullptr;
            }
            mTracking = false;
            return true;
        }
        return false;
    }

    void onDraw(Painter& p) override {
        View::onDraw(p);

        uint8_t pa = p.alpha();
        uint8_t ti = p.tileIdx();

        for (uint16_t i = 0; i < childCount(); i++) {
            View* child = childAt(i);
            if (!child || !child->visible()) continue;

            uint16_t mask = child->tileMask();
            if (mask && !((mask >> ti) & 1)) continue;

            Region tb = child->transformedBounds();

            const uint32_t sc = p.scale();
            const int64_t sc64 = (int64_t)sc;
            const int64_t vx = child->visualXFP();
            const int64_t vy = child->visualYFP() + ((int64_t)mScroll << 16);
            int64_t cx = p.originXFP() + ((vx * sc64) >> 16);
            int64_t cy = p.originYFP() + ((vy * sc64) >> 16);
            int64_t cr = cx + ((int64_t)child->width()  * sc64);
            int64_t cb = cy + ((int64_t)child->height() * sc64);

            int sx = Painter::roundFP(cx);
            int sy = Painter::roundFP(cy);
            int sr = Painter::roundFP(cr);
            int sb = Painter::roundFP(cb);
            if (sr <= sx) sr = sx + 1;
            if (sb <= sy) sb = sy + 1;
            if (!p.intersectsClip(sx, sy, sr, sb)) continue;

            uint8_t ca = child->alpha();
            const bool identity =
                sc == Painter::kScaleOne && mScroll == 0
                && ca == 255 && pa == 255
                && tb.x == 0 && tb.y == 0
                && child->translationXQ16() == 0 && child->translationYQ16() == 0
                && ((p.originXFP() | p.originYFP()) & (int64_t)0xFFFF) == 0;
            if (identity) {
                child->onDraw(p);
            } else {
                Painter cp = p;
                cp.setScreenOriginFP(cx, cy);
                cp.setScreenClip(sx, sy, sr, sb);
                cp.setScale(sc);
                cp.setAlpha((uint8_t)((uint32_t)pa * ca / 255));
                child->onDraw(cp);
            }
        }
    }

    int  scrollY() const { return mScroll; }
    void setScrollY(int y) {
        mScroll = y;
        clampScroll();
        invalidate();
    }

protected:
    // Viewport fills parent specs; children measured with UNSPECIFIED height
    // so content can extend beyond the viewport (vertical scroll).
    void onMeasure(int32_t widthMeasureSpec, int32_t heightMeasureSpec) override {
        const int w = MeasureSpec::getDefaultSize(getSuggestedMinimumWidth(), widthMeasureSpec);
        const int h = MeasureSpec::getDefaultSize(getSuggestedMinimumHeight(), heightMeasureSpec);
        setMeasuredDimension(w, h);

        const int32_t childWSpec = getChildMeasureSpec(
            widthMeasureSpec, insetHorizontal(), LayoutParams::MATCH_PARENT);
        const int32_t childHSpec = MeasureSpec::make(0, MeasureSpec::UNSPECIFIED);

        for (uint16_t i = 0; i < childCount(); i++) {
            View* child = childAt(i);
            if (!child || !child->visible()) continue;
            litho::LayoutParams* lp = child->layoutParams();
            // No LP → wrap / authored size (absolute grids). Explicit LP for Linear lists.
            int widthDim  = lp ? lp->width
                               : (child->width()  > 0 ? child->width()  : LayoutParams::WRAP_CONTENT);
            int heightDim = lp ? lp->height
                               : (child->height() > 0 ? child->height() : LayoutParams::WRAP_CONTENT);
            int ml = lp ? lp->marginL : 0;
            int mt = lp ? lp->marginT : 0;
            int mr = lp ? lp->marginR : 0;
            int mb = lp ? lp->marginB : 0;
            child->measure(
                getChildMeasureSpec(widthMeasureSpec,
                    insetHorizontal() + ml + mr, widthDim),
                (heightDim >= 0)
                    ? MeasureSpec::make(heightDim, MeasureSpec::EXACTLY)
                    : (heightDim == LayoutParams::MATCH_PARENT
                           ? getChildMeasureSpec(heightMeasureSpec,
                                 insetVertical() + mt + mb, heightDim)
                           : childHSpec));
            (void)childWSpec;
        }
    }

    void onLayout(bool changed, int left, int top, int right, int bottom) override {
        (void)changed; (void)right; (void)bottom;
        (void)left; (void)top;
        // LP children: top-left of content box (Linear/Grid). No LP: keep authored x/y.
        for (uint16_t i = 0; i < childCount(); i++) {
            View* child = childAt(i);
            if (!child || !child->visible()) continue;
            litho::LayoutParams* lp = child->layoutParams();
            int cl = lp ? (insetLeft()  + lp->marginL) : child->x();
            int ct = lp ? (insetTop()   + lp->marginT) : child->y();
            child->layout(cl, ct, cl + child->measuredWidth(), ct + child->measuredHeight());
        }
        clampScroll();
    }

private:
    void cancelChild() {
        if (!mChild) return;
        TouchEvent cancel{};
        cancel.x      = 0;
        cancel.y      = 0;
        cancel.action = TouchAction::CANCEL;
        mChild->dispatchTouchEvent(cancel, mChildSX, mChildSY);
        mChild = nullptr;
    }

    int contentBottom() const {
        int bottom = 0;
        for (uint16_t i = 0; i < childCount(); i++) {
            View* child = childAt(i);
            if (!child || !child->visible()) continue;
            Region tb = child->transformedBounds();
            int b = tb.y + tb.height;
            if (b > bottom) bottom = b;
        }
        return bottom;
    }

    void clampScroll() {
        int viewH = mBounds.height;
        int contentH = contentBottom();
        int minScroll = viewH - contentH;
        if (minScroll > 0) minScroll = 0; // content fits — no scroll
        if (mScroll > 0) mScroll = 0;
        if (mScroll < minScroll) mScroll = minScroll;
    }

    int   mLastY     = 0;
    int   mDownY     = 0;
    int   mScroll    = 0;
    bool  mTracking  = false;
    bool  mDidScroll = false;
    View* mChild     = nullptr;
    int   mChildSX   = 0;
    int   mChildSY   = 0;
};

} // namespace litho
