#pragma once
#include "framework/view/view_group.hpp"

namespace litho {

/* ScrollView — ViewGroup that scrolls its own children vertically.  Like
 * Android's ScrollView: children are added directly via addView(), and the
 * scroll offset is applied in onDraw().  Touch interception is self-contained
 * — DOWN captures the target, MOVE adjusts the scroll, UP releases. */
class ScrollView : public ViewGroup {
public:
    bool dispatchTouchEvent(TouchEvent& ev, int sx, int sy) override {
        if (ev.action == TouchAction::DOWN) {
            mLastY = ev.y;
            mTracking = true;
            ev.handler   = this;
            ev.handlerSX = sx;
            ev.handlerSY = sy;
            return true;
        }
        if (ev.action == TouchAction::MOVE && mTracking) {
            int dy = ev.y - mLastY;
            mLastY  = ev.y;
            mScroll += dy;
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

private:
    int mLastY  = 0;
    int mScroll = 0;
    bool mTracking = false;
};

} // namespace litho
