#pragma once
#include "framework/animation/object_animator.hpp"
#include "framework/animation/animation_manager.hpp"
#include "framework/animation/property.hpp"
#include "framework/view/view.hpp"

namespace litho {

class ViewPropertyAnimator {
public:
    static constexpr int kMaxProps = 4;

    explicit ViewPropertyAnimator(View* view) : mView(view) {}

    ViewPropertyAnimator& translationX(float to) {
        return addProp((float)mView->translationX(), to, viewSetTranslationX);
    }
    ViewPropertyAnimator& translationY(float to) {
        return addProp((float)mView->translationY(), to, viewSetTranslationY);
    }
    ViewPropertyAnimator& alpha(float to) {
        return addProp((float)mView->alpha(), to, viewSetAlpha);
    }

    ViewPropertyAnimator& setDuration(uint32_t ms) { mDuration = ms; return *this; }

    ViewPropertyAnimator& setInterpolator(Interpolator type) {
        mInterpolator = type; return *this;
    }

    void start(AnimationManager& mgr) {
        // Cancel previously running animators
        cancel(&mgr);

        for (int i = 0; i < mPropCount; i++) {
            auto& oa = mObjAnims[i];
            oa.setTarget(mView)
              .setFloatValues(mProps[i].from, mProps[i].to)
              .setSetter(mProps[i].setter)
              .setDuration(mDuration)
              .setInterpolator(mInterpolator);
            oa.start();
            mgr.addAnimator(&oa.animator());
        }
        mActiveCount = mPropCount;
        mManager = &mgr;
    }

    void cancel(AnimationManager* mgr = nullptr) {
        auto* m = mgr ? mgr : mManager;
        if (!m || mActiveCount == 0) return;
        for (int i = 0; i < mActiveCount; i++) {
            mObjAnims[i].animator().cancel();
            m->removeAnimator(&mObjAnims[i].animator());
        }
        mActiveCount = 0;
    }

private:
    struct PropRequest {
        float from;
        float to;
        void (*setter)(void*, float);
    };

    ViewPropertyAnimator& addProp(float from, float to, void (*setter)(void*, float)) {
        if (mPropCount < kMaxProps) {
            mProps[mPropCount++] = {from, to, setter};
        }
        return *this;
    }

    View*          mView        = nullptr;
    AnimationManager* mManager  = nullptr;
    PropRequest    mProps[kMaxProps];
    ObjectAnimator mObjAnims[kMaxProps];
    int            mPropCount   = 0;
    int            mActiveCount = 0;
    uint32_t       mDuration    = 300;
    Interpolator   mInterpolator = Interpolator::LINEAR;
};

} // namespace litho
