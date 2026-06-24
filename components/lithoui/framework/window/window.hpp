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

    void draw(Painter& p) {
        if (mRootView) mRootView->onDraw(p);
    }

    void invalidateRect(const Region& r) {
        if (mDirtyList) mDirtyList->markDirty(r);
    }

    bool dispatchTouchEvent(TouchEvent& ev) {
        if (ev.action == TouchAction::DOWN) {
            ev.handler   = nullptr;
            ev.handlerSX = 0;
            ev.handlerSY = 0;
            if (mRootView && mRootView->dispatchTouchEvent(ev, 0, 0)) {
                if (ev.handler) {
                    mTouchTarget.view    = (View*)ev.handler;
                    mTouchTarget.screenX = ev.handlerSX;
                    mTouchTarget.screenY = ev.handlerSY;
                }
                return true;
            }
            return false;
        }

        // MOVE or UP â€?dispatch to captured target
        if (mTouchTarget.view) {
            bool handled = mTouchTarget.view->dispatchTouchEvent(
                ev, mTouchTarget.screenX, mTouchTarget.screenY);
            if (ev.action == TouchAction::UP) {
                mTouchTarget.view = nullptr;
            }
            return handled;
        }
        return false;
    }

private:
    struct TouchTarget { View* view = nullptr; int screenX = 0; int screenY = 0; };

    ViewGroup* mRootView    = nullptr;
    DirtyList* mDirtyList   = nullptr;
    TouchTarget mTouchTarget;
};

} // namespace litho
