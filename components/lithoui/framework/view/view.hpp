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
    int16_t translationX() const { return mTranslationX; }
    int16_t translationY() const { return mTranslationY; }
    uint8_t alpha()        const { return mAlpha; }

    // Setters invalidate both old and new screen rects to prevent ghosting
    // during animation. Alpha changes only need single invalidate (no motion).
    void setTranslationX(int16_t tx);
    void setTranslationY(int16_t ty);
    void setAlpha(uint8_t a)         { mAlpha = a; invalidate(); }

    ViewPropertyAnimator& animate();

    ViewGroup* parent()    const { return mParent; }
    bool       visible()   const { return bVisible; }
    void       setVisible(bool v) { bVisible = v; }

    virtual void onDraw(Painter& p) { (void)p; }

    // Local-space bounds including all transforms (translation, rotation).
    // Default: mBounds shifted by translation.  Subclasses override to
    // add rotation, scale, etc.  Used by screenBounds() and ViewGroup clip.
    virtual Region transformedBounds() const {
        return {static_cast<int16_t>(mBounds.x + mTranslationX),
                static_cast<int16_t>(mBounds.y + mTranslationY),
                mBounds.width, mBounds.height};
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
    int16_t      mTranslationX  = 0;
    int16_t      mTranslationY  = 0;
    uint8_t      mAlpha         = 255;
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

} // namespace litho
