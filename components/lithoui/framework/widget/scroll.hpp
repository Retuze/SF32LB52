#pragma once
#include "framework/view/view_group.hpp"

namespace litho {

/* ScrollView — ViewGroup that scrolls its own children vertically.  Like
 * Android's ScrollView: children are added directly via addView(), and the
 * scroll offset is applied in onDraw().
 *
 * Touch: try children first (with scroll offset). If none handle DOWN, capture
 * the gesture for scrolling. Do not claim sibling hits — callers should size
 * this view so it does not cover other controls. */
class ScrollView : public ViewGroup {
public:
    bool dispatchTouchEvent(TouchEvent& ev, int sx, int sy) override {
        if (ev.action == TouchAction::DOWN) {
            mLastY = ev.y;
            mTracking = false;

            // Children first (topmost), hit-test with current scroll offset.
            for (int i = (int)childCount() - 1; i >= 0; i--) {
                View* child = childAt((uint16_t)i);
                if (!child || !child->visible()) continue;

                Region tb = child->transformedBounds();
                int cx = sx + tb.x;
                int cy = sy + tb.y + mScroll;

                if (ev.x >= cx && ev.x < cx + tb.width &&
                    ev.y >= cy && ev.y < cy + tb.height) {
                    if (child->dispatchTouchEvent(ev, cx, cy)) {
                        if (!ev.handler) {
                            ev.handler   = child;
                            ev.handlerSX = cx;
                            ev.handlerSY = cy;
                        }
                        return true;
                    }
                }
            }

            // Empty area → scroll gesture
            mTracking = true;
            ev.handler   = this;
            ev.handlerSX = sx;
            ev.handlerSY = sy;
            return true;
        }

        if (ev.action == TouchAction::MOVE && mTracking) {
            int dy = ev.y - mLastY;
            mLastY = ev.y;
            if (dy != 0) {
                mScroll += dy;
                invalidate();
            }
            return true;
        }

        if (ev.action == TouchAction::UP) {
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
            if (ca == 255 && pa == 255 && tb.x == 0 && tb.y == 0) {
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
    void setScrollY(int y) { mScroll = y; invalidate(); }

private:
    int  mLastY    = 0;
    int  mScroll   = 0;
    bool mTracking = false;
};

} // namespace litho
