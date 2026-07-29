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

            int sx = p.screenX() + tb.x;
            int sy = p.screenY() + tb.y + mScroll;
            int sr = sx + tb.width;
            int sb = sy + tb.height;

            if (!p.intersectsClip(sx, sy, sr, sb)) continue;

            uint8_t ca = child->alpha();
            // Always apply origin when scrolled — (0,0) fast path would skip offset.
            if (ca == 255 && pa == 255 && tb.x == 0 && tb.y == 0 && mScroll == 0) {
                child->onDraw(p);
            } else {
                Painter cp = p;
                cp.setScreenOrigin(sx, sy);
                cp.setScreenClip(sx, sy, sr, sb);
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
