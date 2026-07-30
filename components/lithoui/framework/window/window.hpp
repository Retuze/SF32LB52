#pragma once
#include "framework/view/view_group.hpp"
#include "core/dirty_list.hpp"

namespace litho {

class Window {
public:
    Window()  = default;
    ~Window() { delete mRootView; }

    void setDirtyList(DirtyList* dl) { mDirtyList = dl; }

    void setContentView(ViewGroup* root) {
        delete mRootView;
        mRootView = root;
        if (mDirtyList) mRootView->propagateDirtyList(mDirtyList);
        if (mRootView) mRootView->requestLayout();
    }

    ViewGroup* rootView() const { return mRootView; }

    // Run measure/layout when the tree requested it. Root is EXACTLY screen size.
    void layoutIfNeeded(int screenW, int screenH) {
        if (!mVisible || !mRootView) return;
        if (!mRootView->isLayoutRequested()) return;
        mRootView->measure(
            MeasureSpec::make(screenW, MeasureSpec::EXACTLY),
            MeasureSpec::make(screenH, MeasureSpec::EXACTLY));
        mRootView->layout(0, 0, screenW, screenH);
    }

    // Stopped activities keep their Window for the back stack, but must not
    // composite � otherwise a fade on page N reveals page N-2 underneath.
    void setVisible(bool v) { mVisible = v; }
    bool visible() const { return mVisible; }

    void setInputEnabled(bool enabled) {
        if (!enabled) cancelTouch();
        mInputEnabled = enabled;
    }
    bool inputEnabled() const { return mInputEnabled; }

    // Drop any captured pointer and notify the target (clears Button pressed, etc.).
    void cancelTouch() {
        if (!mTouchTarget.view) return;
        TouchEvent ev{};
        ev.x = 0;
        ev.y = 0;
        ev.action = TouchAction::CANCEL;
        ev.handler = nullptr;
        mTouchTarget.view->dispatchTouchEvent(
            ev, mTouchTarget.screenX, mTouchTarget.screenY);
        mTouchTarget.view = nullptr;
    }

    void draw(Painter& p) {
        if (!mVisible || !mRootView) return;
        // Apply root translation / scale / alpha so Activity transitions show.
        const int32_t txQ = mRootView->translationXQ16();
        const int32_t tyQ = mRootView->translationYQ16();
        const uint8_t a = mRootView->alpha();
        const uint32_t sc = mRootView->scale();
        if (txQ == 0 && tyQ == 0 && a == 255 && sc == View::kScaleOne) {
            mRootView->onDraw(p);
            return;
        }
        // Pivot at root center in 16.16: origin' = translation + pivot*(1 - scale)
        const int pivX = mRootView->width()  / 2;
        const int pivY = mRootView->height() / 2;
        const int64_t sc64 = (int64_t)sc;
        const int64_t originXFP =
            ((int64_t)p.screenX() << 16) + (int64_t)txQ
            + (((int64_t)pivX << 16) - (int64_t)pivX * sc64);
        const int64_t originYFP =
            ((int64_t)p.screenY() << 16) + (int64_t)tyQ
            + (((int64_t)pivY << 16) - (int64_t)pivY * sc64);
        Painter cp = p;
        cp.setScreenOriginFP(originXFP, originYFP);
        cp.setScale(sc);
        cp.setAlpha((uint8_t)((uint32_t)p.alpha() * a / 255));
        // Soft clip expanded for fractional translation.
        Region tb = mRootView->transformedBounds();
        cp.setScreenClip(p.screenX() + tb.x, p.screenY() + tb.y,
                         p.screenX() + tb.x + tb.width,
                         p.screenY() + tb.y + tb.height);
        mRootView->onDraw(cp);
    }

    void invalidateRect(const Region& r) {
        if (mDirtyList) mDirtyList->markDirty(r);
    }

    bool dispatchTouchEvent(TouchEvent& ev) {
        if (!mInputEnabled) return false;

        if (ev.action == TouchAction::DOWN) {
            ev.handler   = nullptr;
            ev.handlerSX = 0;
            ev.handlerSY = 0;
            if (!mRootView) return false;
            // Hit-test in the root's translated coordinate space.
            int ox = mRootView->translationX();
            int oy = mRootView->translationY();
            if (mRootView->dispatchTouchEvent(ev, ox, oy)) {
                if (ev.handler) {
                    mTouchTarget.view    = (View*)ev.handler;
                    mTouchTarget.screenX = ev.handlerSX;
                    mTouchTarget.screenY = ev.handlerSY;
                }
                return true;
            }
            return false;
        }

        // MOVE / UP / CANCEL � dispatch to captured target
        if (mTouchTarget.view) {
            bool handled = mTouchTarget.view->dispatchTouchEvent(
                ev, mTouchTarget.screenX, mTouchTarget.screenY);
            if (ev.action == TouchAction::UP || ev.action == TouchAction::CANCEL) {
                mTouchTarget.view = nullptr;
            }
            return handled;
        }
        return false;
    }

private:
    struct TouchTarget { View* view = nullptr; int screenX = 0; int screenY = 0; };

    ViewGroup* mRootView     = nullptr;
    DirtyList* mDirtyList    = nullptr;
    TouchTarget mTouchTarget;
    bool        mInputEnabled = true;
    bool        mVisible      = true;
};

} // namespace litho
