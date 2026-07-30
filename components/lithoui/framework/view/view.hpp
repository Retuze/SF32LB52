#pragma once
#include "core/region.hpp"
#include "core/painter.hpp"
#include "core/dirty_list.hpp"
#include "framework/base/object.hpp"
#include "framework/event/event_types.hpp"
#include "framework/view/measure_spec.hpp"
#include "framework/view/layout_params.hpp"

namespace litho {

class ViewGroup;
class ViewPropertyAnimator;

class View : public Object {
public:
    View()  = default;
    ~View() override;

    Region&       bounds()       { return mBounds; }
    const Region& bounds() const { return mBounds; }

    int  x()      const { return mBounds.x; }
    int  y()      const { return mBounds.y; }
    int  width()  const { return mBounds.width; }
    int  height() const { return mBounds.height; }

    int measuredWidth()  const { return mMeasuredWidth; }
    int measuredHeight() const { return mMeasuredHeight; }

    // Animated visual properties
    int16_t  translationX() const { return mTranslationX; }
    int16_t  translationY() const { return mTranslationY; }
    uint8_t  alpha()        const { return mAlpha; }
    // Fixed-point scale: kScaleOne (65536) = 1.0 (16.16). Higher precision
    // than 8.8 avoids 1–2px snapping of child positions during scale anims.
    static constexpr uint32_t kScaleOne = 65536u;
    uint32_t scale()        const { return mScale; }

    // Setters invalidate both old and new screen rects to prevent ghosting
    // during animation. Alpha changes only need single invalidate (no motion).
    void setTranslationX(int16_t tx);
    void setTranslationY(int16_t ty);
    void setAlpha(uint8_t a)         { mAlpha = a; invalidate(); }
    void setScale(uint32_t s);

    // Round local → scaled pixels: (x * scale + 0.5) in 16.16.
    static inline int applyScale(int x, uint32_t s) {
        return (int)(((int64_t)x * (int64_t)s + 32768) >> 16);
    }

    ViewPropertyAnimator& animate();

    ViewGroup* parent()    const { return mParent; }
    bool       visible()   const { return bVisible; }
    void       setVisible(bool v);

    // Solid fill drawn before subclass content (ViewGroup draws this, then children).
    virtual void setBackgroundColor(RGB565 c) { mHasBg = true; mBgColor = c; invalidate(); }
    virtual void clearBackgroundColor()       { mHasBg = false; invalidate(); }
    bool   hasBackgroundColor() const   { return mHasBg; }
    RGB565 backgroundColor()    const   { return mBgColor; }

    void setPadding(int16_t l, int16_t t, int16_t r, int16_t b);
    int  paddingLeft()   const { return mPadL; }
    int  paddingTop()    const { return mPadT; }
    int  paddingRight()  const { return mPadR; }
    int  paddingBottom() const { return mPadB; }

    // CSS border-box: width/height include border + padding; margin stays outside.
    void setBorder(int16_t width, RGB565 color);
    void setBorder(int16_t l, int16_t t, int16_t r, int16_t b, RGB565 color);
    void clearBorder();
    int  borderLeft()   const { return mBorderL; }
    int  borderTop()    const { return mBorderT; }
    int  borderRight()  const { return mBorderR; }
    int  borderBottom() const { return mBorderB; }
    RGB565 borderColor() const { return mBorderColor; }

    // Distance from bounds edge to content box (border + padding).
    int insetLeft()   const { return (int)mBorderL + (int)mPadL; }
    int insetTop()    const { return (int)mBorderT + (int)mPadT; }
    int insetRight()  const { return (int)mBorderR + (int)mPadR; }
    int insetBottom() const { return (int)mBorderB + (int)mPadB; }
    int insetHorizontal() const { return insetLeft() + insetRight(); }
    int insetVertical()   const { return insetTop() + insetBottom(); }

    // Takes ownership of lp (may be null = absolute / no layout hints).
    void setLayoutParams(LayoutParams* lp);
    LayoutParams* layoutParams() const { return mLayoutParams; }

    // ---- measure / layout ----

    void requestLayout();
    bool isLayoutRequested() const { return mLayoutRequested; }

    void measure(int32_t widthMeasureSpec, int32_t heightMeasureSpec);
    void layout(int l, int t, int r, int b);

    virtual void onDraw(Painter& p) {
        drawBackground(p);
        drawBorder(p);
    }

    // Local-space bounds including all transforms (translation, scale).
    // Default: mBounds shifted by translation; scale expands about center.
    // Used by screenBounds() and ViewGroup clip.
    virtual Region transformedBounds() const {
        int16_t x = static_cast<int16_t>(mBounds.x + mTranslationX);
        int16_t y = static_cast<int16_t>(mBounds.y + mTranslationY);
        int16_t w = mBounds.width;
        int16_t h = mBounds.height;
        if (mScale != kScaleOne && w > 0 && h > 0) {
            int sw = applyScale(w, mScale);
            int sh = applyScale(h, mScale);
            if (sw < 1) sw = 1;
            if (sh < 1) sh = 1;
            x = (int16_t)(x + (w - sw) / 2);
            y = (int16_t)(y + (h - sh) / 2);
            w = (int16_t)sw;
            h = (int16_t)sh;
        }
        return {x, y, w, h};
    }

    // Compute screen-space rectangle for this view (bounds + translation,
    // accumulated up the parent chain). Used by invalidate() and setters.
    Region screenBounds() const;

    // Push the screen-space bounds (see screenBounds()) to the DirtyList.
    void invalidate();

    // Propagate dirty list to view tree (replaces dynamic_cast on ViewGroup).
    virtual void propagateDirtyList(DirtyList* dl) { mDirtyList = dl; }

    // Tile mask: bit i set = this view covers tile row i. 0 = unset (draw always).
    void     setTileMask(uint16_t m) { mTileMask = m; }
    uint16_t tileMask()        const { return mTileMask; }

    // Touch dispatch. Default: no children, just call onTouchEvent.
    virtual bool dispatchTouchEvent(TouchEvent& ev, int screenX, int screenY);

    // Touch event. Return true if handled.
    virtual bool onTouchEvent(TouchEvent& e) { (void)e; return false; }

    // Updated each frame by WindowManager — for press-delay, animations, etc.
    static void setFrameTimeMs(uint32_t ms) { sFrameTimeMs = ms; }
    static uint32_t frameTimeMs() { return sFrameTimeMs; }

protected:
    friend class ViewGroup;

    virtual void onMeasure(int32_t widthMeasureSpec, int32_t heightMeasureSpec);
    virtual void onLayout(bool changed, int left, int top, int right, int bottom) {
        (void)changed; (void)left; (void)top; (void)right; (void)bottom;
    }

    void setMeasuredDimension(int measuredWidth, int measuredHeight) {
        mMeasuredWidth  = measuredWidth;
        mMeasuredHeight = measuredHeight;
    }

    virtual int getSuggestedMinimumWidth()  const { return 0; }
    virtual int getSuggestedMinimumHeight() const { return 0; }

    // Resolve child MeasureSpec from parent spec + LP dimension + parent insets.
    static int32_t getChildMeasureSpec(int32_t spec, int padding, int childDimension);

    void drawBackground(Painter& p) {
        if (mHasBg && mBounds.width > 0 && mBounds.height > 0)
            p.fillRect(0, 0, mBounds.width, mBounds.height, mBgColor);
    }
    void drawBorder(Painter& p);

    Region       mBounds;
    bool         bVisible       = true;
    bool         mHasBg         = false;
    bool         mLayoutRequested = true; // first frame lays out
    RGB565       mBgColor       = {};
    RGB565       mBorderColor   = {};
    int16_t      mTranslationX  = 0;
    int16_t      mTranslationY  = 0;
    int16_t      mPadL = 0, mPadT = 0, mPadR = 0, mPadB = 0;
    int16_t      mBorderL = 0, mBorderT = 0, mBorderR = 0, mBorderB = 0;
    uint8_t      mAlpha         = 255;
    uint32_t     mScale         = kScaleOne;
    int          mMeasuredWidth  = 0;
    int          mMeasuredHeight = 0;
    ViewGroup*   mParent        = nullptr;
    DirtyList*   mDirtyList     = nullptr;
    LayoutParams* mLayoutParams = nullptr;
    uint16_t     mTileMask      = 0;  // 0=draw always, bit i=covered

private:
    ViewPropertyAnimator* mAnimator = nullptr;
    static uint32_t sFrameTimeMs;
};

// View property setters for ObjectAnimator
inline void viewSetTranslationX(void* target, float val) {
    ((View*)target)->setTranslationX((int16_t)val);
}
inline void viewSetTranslationY(void* target, float val) {
    ((View*)target)->setTranslationY((int16_t)val);
}
inline void viewSetAlpha(void* target, float val) {
    ((View*)target)->setAlpha((uint8_t)val);
}
inline void viewSetScale(void* target, float val) {
    if (val < 1.f) val = 1.f;
    if (val > 4294967040.f) val = 4294967040.f; // leave headroom below UINT32_MAX
    ((View*)target)->setScale((uint32_t)(val + 0.5f));
}

} // namespace litho
