#pragma once
#include "view.hpp"
#include <stdint.h>

namespace litho {

class ViewGroup : public View {
public:
    ~ViewGroup() override {
        for (uint16_t i = 0; i < mChildCount; i++) {
            delete mChildren[i];
        }
        delete[] mChildren;
    }

    // ---- child management ----

    void addView(View* child) {
        if (mChildCount >= mCapacity) grow();
        child->mParent    = this;
        child->mDirtyList = mDirtyList;  // propagate from parent
        mChildren[mChildCount++] = child;
    }

    View* childAt(uint16_t i) const {
        return (i < mChildCount) ? mChildren[i] : nullptr;
    }
    uint16_t childCount() const { return mChildCount; }

    // ---- draw ----

    void onDraw(Painter& p) override {
        View::onDraw(p);

        // Pre-compute parent alpha once (stays constant across children)
        uint8_t pa = p.alpha();

        uint8_t ti = p.tileIdx();

        for (uint16_t i = 0; i < mChildCount; i++) {
            View* child = mChildren[i];
            if (!child || !child->visible()) continue;

            // Tile mask quick reject: if mask set & tile not covered, skip
            uint16_t mask = child->tileMask();
            if (mask && !((mask >> ti) & 1)) continue;

            Region tb = child->transformedBounds();

            int sx = p.screenX() + tb.x;
            int sy = p.screenY() + tb.y;
            int sr = sx + tb.width;
            int sb = sy + tb.height;

            if (!p.intersectsClip(sx, sy, sr, sb)) continue;

            uint8_t ca = child->alpha();
            if (ca == 255 && pa == 255 && tb.x == 0 && tb.y == 0) {
                // Child at (0,0), no alpha → reuse parent painter directly
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

    // ---- touch dispatch ----

    bool dispatchTouchEvent(TouchEvent& ev, int screenX, int screenY) override {
        // Hit-test children in reverse draw order (topmost first)
        for (int i = mChildCount - 1; i >= 0; i--) {
            View* child = mChildren[i];
            if (!child || !child->visible()) continue;

            Region tb = child->transformedBounds();
            int cx = screenX + tb.x;
            int cy = screenY + tb.y;

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

        if (onTouchEvent(ev)) {
            if (!ev.handler) {
                ev.handler   = this;
                ev.handlerSX = screenX;
                ev.handlerSY = screenY;
            }
            return true;
        }
        return false;
    }

    void propagateDirtyList(DirtyList* dl) override {
        mDirtyList = dl;
        for (uint16_t i = 0; i < mChildCount; i++) {
            mChildren[i]->propagateDirtyList(dl);
        }
    }

private:
    void grow() {
        uint16_t newCap = (mCapacity == 0) ? 4 : mCapacity * 2;
        View**   arr    = new View*[newCap];
        for (uint16_t i = 0; i < mChildCount; i++) arr[i] = mChildren[i];
        delete[] mChildren;
        mChildren = arr;
        mCapacity = newCap;
    }

    View**   mChildren   = nullptr;
    uint16_t mChildCount = 0;
    uint16_t mCapacity   = 0;
};

} // namespace litho
