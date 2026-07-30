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
        requestLayout();
    }

    // Takes ownership of lp.
    void addView(View* child, LayoutParams* lp) {
        child->setLayoutParams(lp);
        addView(child);
    }

    // Fluent / sugar overloads (take ownership via Lp::release).
    void addView(View* child, Lp&& lp) {
        addView(child, lp.release());
    }
    void addView(View* child, int16_t w, int16_t h) {
        addView(child, new LayoutParams(w, h));
    }
    void addView(View* child, int16_t w, int16_t h, float weight) {
        addView(child, new LayoutParams(w, h, weight));
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
        // After DOWN, deliver MOVE/UP/CANCEL to the same child (Android-style).
        // Re-hit-testing CANCEL at (0,0) would miss Buttons under a LinearLayout.
        if (ev.action != TouchAction::DOWN && mTouchChild) {
            bool handled = mTouchChild->dispatchTouchEvent(
                ev, mTouchChildSX, mTouchChildSY);
            if (ev.action == TouchAction::UP || ev.action == TouchAction::CANCEL)
                mTouchChild = nullptr;
            return handled;
        }

        if (ev.action == TouchAction::DOWN)
            mTouchChild = nullptr;

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
                    if (ev.action == TouchAction::DOWN) {
                        mTouchChild   = child;
                        mTouchChildSX = cx;
                        mTouchChildSY = cy;
                    }
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

protected:
    // Absolute-compatible defaults: measure children, keep authored x/y on layout.
    void onMeasure(int32_t widthMeasureSpec, int32_t heightMeasureSpec) override {
        for (uint16_t i = 0; i < mChildCount; i++) {
            View* child = mChildren[i];
            if (!child || !child->visible()) continue;
            measureChild(child, widthMeasureSpec, heightMeasureSpec);
        }
        setMeasuredDimension(
            MeasureSpec::getDefaultSize(getSuggestedMinimumWidth(), widthMeasureSpec),
            MeasureSpec::getDefaultSize(getSuggestedMinimumHeight(), heightMeasureSpec));
    }

    void onLayout(bool changed, int left, int top, int right, int bottom) override {
        (void)changed; (void)left; (void)top; (void)right; (void)bottom;
        for (uint16_t i = 0; i < mChildCount; i++) {
            View* child = mChildren[i];
            if (!child || !child->visible()) continue;
            int cl = child->x();
            int ct = child->y();
            child->layout(cl, ct, cl + child->measuredWidth(), ct + child->measuredHeight());
        }
    }

    void measureChild(View* child, int32_t parentWidthSpec, int32_t parentHeightSpec) {
        LayoutParams* lp = child->layoutParams();
        int width  = lp ? lp->width
                        : (child->width()  > 0 ? child->width()  : LayoutParams::WRAP_CONTENT);
        int height = lp ? lp->height
                        : (child->height() > 0 ? child->height() : LayoutParams::WRAP_CONTENT);
        int ml = lp ? lp->marginL : 0;
        int mt = lp ? lp->marginT : 0;
        int mr = lp ? lp->marginR : 0;
        int mb = lp ? lp->marginB : 0;

        child->measure(
            getChildMeasureSpec(parentWidthSpec,
                                insetHorizontal() + ml + mr, width),
            getChildMeasureSpec(parentHeightSpec,
                                insetVertical() + mt + mb, height));
    }

    // Horizontal: use parent width spec; height UNSPECIFIED (scroll content).
    void measureChildWithMargins(View* child,
                                 int32_t parentWidthSpec, int widthUsed,
                                 int32_t parentHeightSpec, int heightUsed) {
        LayoutParams* lp = child->layoutParams();
        int width  = lp ? lp->width  : LayoutParams::WRAP_CONTENT;
        int height = lp ? lp->height : LayoutParams::WRAP_CONTENT;
        int ml = lp ? lp->marginL : 0;
        int mt = lp ? lp->marginT : 0;
        int mr = lp ? lp->marginR : 0;
        int mb = lp ? lp->marginB : 0;

        child->measure(
            getChildMeasureSpec(parentWidthSpec,
                insetHorizontal() + ml + mr + widthUsed, width),
            getChildMeasureSpec(parentHeightSpec,
                insetVertical() + mt + mb + heightUsed, height));
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

    // Captured child for the current pointer gesture.
    View* mTouchChild   = nullptr;
    int   mTouchChildSX = 0;
    int   mTouchChildSY = 0;
};

} // namespace litho
