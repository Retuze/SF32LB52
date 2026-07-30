#include "framework/view/view.hpp"
#include "framework/view/view_group.hpp"
#include "framework/animation/view_property_animator.hpp"

namespace litho {

uint32_t View::sFrameTimeMs = 0;

View::~View() {
    if (mAnimator) {
        mAnimator->cancel();
        delete mAnimator;
        mAnimator = nullptr;
    }
    delete mLayoutParams;
    mLayoutParams = nullptr;
}

Region View::screenBounds() const {
    Region r = transformedBounds();
    ViewGroup* p = mParent;
    while (p) {
        // Parent layout offset is integer; parent translation may be fractional —
        // accumulate rounded so dirty rects stay integer Regions.
        r.x = (int16_t)(r.x + p->bounds().x + p->translationX());
        r.y = (int16_t)(r.y + p->bounds().y + p->translationY());
        p = p->parent();
    }
    return r;
}

void View::invalidate() {
    if (!mDirtyList) return;
    mDirtyList->markDirty(screenBounds());
}

void View::setVisible(bool v) {
    if (v == bVisible) return;
    bVisible = v;
    invalidate();
    requestLayout();
}

void View::setPadding(int16_t l, int16_t t, int16_t r, int16_t b) {
    if (l == mPadL && t == mPadT && r == mPadR && b == mPadB) return;
    mPadL = l; mPadT = t; mPadR = r; mPadB = b;
    requestLayout();
}

void View::setBorder(int16_t width, RGB565 color) {
    setBorder(width, width, width, width, color);
}

void View::setBorder(int16_t l, int16_t t, int16_t r, int16_t b, RGB565 color) {
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (r < 0) r = 0;
    if (b < 0) b = 0;
    if (l == mBorderL && t == mBorderT && r == mBorderR && b == mBorderB &&
        color.value == mBorderColor.value) {
        return;
    }
    mBorderL = l; mBorderT = t; mBorderR = r; mBorderB = b;
    mBorderColor = color;
    requestLayout();
    invalidate();
}

void View::clearBorder() {
    if (mBorderL == 0 && mBorderT == 0 && mBorderR == 0 && mBorderB == 0) return;
    mBorderL = mBorderT = mBorderR = mBorderB = 0;
    requestLayout();
    invalidate();
}

void View::drawBorder(Painter& p) {
    const int w = mBounds.width;
    const int h = mBounds.height;
    if (w <= 0 || h <= 0) return;
    const int bl = mBorderL, bt = mBorderT, br = mBorderR, bb = mBorderB;
    if (bl == 0 && bt == 0 && br == 0 && bb == 0) return;
    const RGB565 c = mBorderColor;
    // Four edge strips; corners belong to horizontal strips (top/bottom).
    if (bt > 0) p.fillRect(0, 0, w, bt, c);
    if (bb > 0) p.fillRect(0, h - bb, w, bb, c);
    if (bl > 0) p.fillRect(0, bt, bl, h - bt - bb, c);
    if (br > 0) p.fillRect(w - br, bt, br, h - bt - bb, c);
}

void View::setLayoutParams(LayoutParams* lp) {
    if (lp == mLayoutParams) return;
    delete mLayoutParams;
    mLayoutParams = lp;
    requestLayout();
}

void View::requestLayout() {
    mLayoutRequested = true;
    if (mParent) mParent->requestLayout();
}

void View::measure(int32_t widthMeasureSpec, int32_t heightMeasureSpec) {
    onMeasure(widthMeasureSpec, heightMeasureSpec);
}

void View::layout(int l, int t, int r, int b) {
    mLayoutRequested = false;

    int w = r - l;
    int h = b - t;
    if (w < 0) w = 0;
    if (h < 0) h = 0;

    const bool changed =
        (mBounds.x != (int16_t)l) || (mBounds.y != (int16_t)t) ||
        (mBounds.width != (int16_t)w) || (mBounds.height != (int16_t)h);

    Region old{};
    if (changed && mDirtyList) old = screenBounds();

    mBounds.x      = (int16_t)l;
    mBounds.y      = (int16_t)t;
    mBounds.width  = (int16_t)w;
    mBounds.height = (int16_t)h;

    onLayout(changed, l, t, r, b);

    if (changed) {
        invalidate();
        if (mDirtyList) mDirtyList->markDirty(old);
    }
}

void View::onMeasure(int32_t widthMeasureSpec, int32_t heightMeasureSpec) {
    int tw = getSuggestedMinimumWidth();
    int th = getSuggestedMinimumHeight();

    if (mLayoutParams) {
        if (mLayoutParams->width  >= 0) tw = mLayoutParams->width;
        if (mLayoutParams->height >= 0) th = mLayoutParams->height;
    } else {
        // Absolute-layout fallback: keep authoring size from bounds().
        if (mBounds.width  > 0) tw = mBounds.width;
        if (mBounds.height > 0) th = mBounds.height;
    }

    setMeasuredDimension(
        MeasureSpec::resolveSize(tw, widthMeasureSpec),
        MeasureSpec::resolveSize(th, heightMeasureSpec));
}

int32_t View::getChildMeasureSpec(int32_t spec, int padding, int childDimension) {
    const MeasureSpec::Mode specMode = MeasureSpec::getMode(spec);
    int specSize = MeasureSpec::getSize(spec) - padding;
    if (specSize < 0) specSize = 0;

    switch (specMode) {
    case MeasureSpec::EXACTLY:
        if (childDimension >= 0)
            return MeasureSpec::make(childDimension, MeasureSpec::EXACTLY);
        if (childDimension == LayoutParams::MATCH_PARENT)
            return MeasureSpec::make(specSize, MeasureSpec::EXACTLY);
        // WRAP_CONTENT
        return MeasureSpec::make(specSize, MeasureSpec::AT_MOST);

    case MeasureSpec::AT_MOST:
        if (childDimension >= 0)
            return MeasureSpec::make(childDimension, MeasureSpec::EXACTLY);
        // MATCH_PARENT or WRAP_CONTENT → at most remaining
        return MeasureSpec::make(specSize, MeasureSpec::AT_MOST);

    default: // UNSPECIFIED
        if (childDimension >= 0)
            return MeasureSpec::make(childDimension, MeasureSpec::EXACTLY);
        return MeasureSpec::make(0, MeasureSpec::UNSPECIFIED);
    }
}

void View::setTranslationXQ16(int32_t txQ16) {
    if (txQ16 == mTranslationXQ16) return;
    Region old = screenBounds();
    mTranslationXQ16 = txQ16;
    invalidate();
    if (mDirtyList) mDirtyList->markDirty(old);
}

void View::setTranslationYQ16(int32_t tyQ16) {
    if (tyQ16 == mTranslationYQ16) return;
    Region old = screenBounds();
    mTranslationYQ16 = tyQ16;
    invalidate();
    if (mDirtyList) mDirtyList->markDirty(old);
}

void View::setScale(uint32_t s) {
    if (s == 0) s = 1;
    if (s == mScale) return;
    Region old = screenBounds();
    mScale = s;
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
        mAnimator->cancel();
        delete mAnimator;
        mAnimator = nullptr;
    }
    mAnimator = new ViewPropertyAnimator(this);
    return *mAnimator;
}

} // namespace litho
