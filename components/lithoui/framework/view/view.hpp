#pragma once
#include "core/region.hpp"
#include "core/painter.hpp"
#include "core/dirty_list.hpp"
#include "framework/base/object.hpp"
#include "framework/event/event_types.hpp"

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
    void       setVisible(bool v) { bVisible = v; }

    // Solid fill drawn before subclass content (ViewGroup draws this, then children).
    virtual void setBackgroundColor(RGB565 c) { mHasBg = true; mBgColor = c; invalidate(); }
    virtual void clearBackgroundColor()       { mHasBg = false; invalidate(); }
    bool   hasBackgroundColor() const   { return mHasBg; }
    RGB565 backgroundColor()    const   { return mBgColor; }

    virtual void onDraw(Painter& p) {
        if (mHasBg && mBounds.width > 0 && mBounds.height > 0)
            p.fillRect(0, 0, mBounds.width, mBounds.height, mBgColor);
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

protected:
    friend class ViewGroup;

    Region       mBounds;
    bool         bVisible       = true;
    bool         mHasBg         = false;
    RGB565       mBgColor       = {};
    int16_t      mTranslationX  = 0;
    int16_t      mTranslationY  = 0;
    uint8_t      mAlpha         = 255;
    uint32_t     mScale         = kScaleOne;
    ViewGroup*   mParent        = nullptr;
    DirtyList*   mDirtyList     = nullptr;
    uint16_t     mTileMask      = 0;  // 0=draw always, bit i=covered

private:
    ViewPropertyAnimator* mAnimator = nullptr;
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
