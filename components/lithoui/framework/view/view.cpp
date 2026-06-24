#include "framework/view/view.hpp"
#include "framework/view/view_group.hpp"
#include "framework/animation/view_property_animator.hpp"

namespace litho {

View::~View() {
    delete mAnimator;
}

Region View::screenBounds() const {
    Region r = transformedBounds();
    ViewGroup* p = mParent;
    while (p) {
        r.x += p->bounds().x + p->translationX();
        r.y += p->bounds().y + p->translationY();
        p = p->parent();
    }
    return r;
}

void View::invalidate() {
    if (!mDirtyList) return;
    mDirtyList->markDirty(screenBounds());
}

void View::setTranslationX(int16_t tx) {
    if (tx == mTranslationX) return;
    // Capture old screen rect before mutation, then invalidate both
    // old and new positions so the previous frame's pixels are cleaned up.
    Region old = screenBounds();
    mTranslationX = tx;
    invalidate();                                // new position
    if (mDirtyList) mDirtyList->markDirty(old);  // old position
}

void View::setTranslationY(int16_t ty) {
    if (ty == mTranslationY) return;
    Region old = screenBounds();
    mTranslationY = ty;
    invalidate();
    if (mDirtyList) mDirtyList->markDirty(old);
}

bool View::dispatchTouchEvent(TouchEvent& ev, int screenX, int screenY) {
    (void)screenX; (void)screenY;
    if (onTouchEvent(ev)) {
        ev.handler   = this;
        ev.handlerSX = screenX;
        ev.handlerSY = screenY;
        return true;
    }
    return false;
}

ViewPropertyAnimator& View::animate() {
    if (mAnimator) {
        delete mAnimator;
        mAnimator = nullptr;
    }
    mAnimator = new ViewPropertyAnimator(this);
    return *mAnimator;
}

} // namespace litho
