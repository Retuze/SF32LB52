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
    }

    ViewGroup* rootView() const { return mRootView; }

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
        // Apply root translation/alpha so Activity transitions are visible.
        // (Children are drawn relative to Painter screen origin.)
        const int ox = mRootView->translationX();
        const int oy = mRootView->translationY();
        const uint8_t a = mRootView->alpha();
        if (ox == 0 && oy == 0 && a == 255) {
            mRootView->onDraw(p);
            return;
        }
        Painter cp = p;
        cp.setScreenOrigin(p.screenX() + ox, p.screenY() + oy);
        cp.setAlpha((uint8_t)((uint32_t)p.alpha() * a / 255));
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
