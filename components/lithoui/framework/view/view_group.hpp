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

            const uint32_t sc = p.scale();
            int sx, sy, sr, sb;
            if (sc == Painter::kScaleOne) {
                sx = p.screenX() + tb.x;
                sy = p.screenY() + tb.y;
                sr = sx + tb.width;
                sb = sy + tb.height;
            } else {
                // Accumulate in 16.16 from parent origin — round only for clip.
                int64_t cx = p.originXFP() + (int64_t)tb.x * (int64_t)sc;
                int64_t cy = p.originYFP() + (int64_t)tb.y * (int64_t)sc;
                int64_t cr = p.originXFP() + (int64_t)(tb.x + tb.width)  * (int64_t)sc;
                int64_t cb = p.originYFP() + (int64_t)(tb.y + tb.height) * (int64_t)sc;
                sx = Painter::roundFP(cx);
                sy = Painter::roundFP(cy);
                sr = Painter::roundFP(cr);
                sb = Painter::roundFP(cb);
                if (sr <= sx) sr = sx + 1;
                if (sb <= sy) sb = sy + 1;

                if (!p.intersectsClip(sx, sy, sr, sb)) continue;

                uint8_t ca = child->alpha();
                Painter cp = p;
                cp.setScreenOriginFP(cx, cy);
                cp.setScreenClip(sx, sy, sr, sb);
                cp.setScale(sc);
                cp.setAlpha((uint8_t)((uint32_t)pa * ca / 255));
                child->onDraw(cp);
                continue;
            }

            if (!p.intersectsClip(sx, sy, sr, sb)) continue;

            uint8_t ca = child->alpha();
            if (ca == 255 && pa == 255 && tb.x == 0 && tb.y == 0) {
                child->onDraw(p);
            } else {
                Painter cp = p;
                cp.setScreenOrigin(sx, sy);
                cp.setScreenClip(sx, sy, sr, sb);
                cp.setScale(sc);
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
