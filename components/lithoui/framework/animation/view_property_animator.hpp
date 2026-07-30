#pragma once
#include "framework/animation/object_animator.hpp"
#include "framework/animation/animation_manager.hpp"
#include "framework/animation/property.hpp"
#include "framework/view/view.hpp"

namespace litho {

class ViewPropertyAnimator {
public:
    static constexpr int kMaxProps = 5;
    using EndCallback = void (*)(void* user);

    explicit ViewPropertyAnimator(View* view) : mView(view) {}

    ViewPropertyAnimator& translationX(float to) {
        return addProp((float)mView->translationXQ16() / (float)View::kTransOne,
                       to, viewSetTranslationX);
    }
    ViewPropertyAnimator& translationY(float to) {
        return addProp((float)mView->translationYQ16() / (float)View::kTransOne,
                       to, viewSetTranslationY);
    }
    ViewPropertyAnimator& alpha(float to) {
        return addProp((float)mView->alpha(), to, viewSetAlpha);
    }
    ViewPropertyAnimator& scale(float to) {
        return addProp((float)mView->scale(), to, viewSetScale);
    }

    ViewPropertyAnimator& setDuration(uint32_t ms) { mDuration = ms; return *this; }

    ViewPropertyAnimator& setInterpolator(Interpolator type) {
        mInterpolator = type; return *this;
    }

    ViewPropertyAnimator& withEndAction(EndCallback cb, void* user = nullptr) {
        mEndCallback = cb;
        mEndUser     = user;
        return *this;
    }

    void start(AnimationManager& mgr) {
        cancel(&mgr);

        if (mPropCount == 0) {
            if (mEndCallback) mEndCallback(mEndUser);
            return;
        }

        for (int i = 0; i < mPropCount; i++) {
            auto& oa = mObjAnims[i];
            oa.setTarget(mView)
              .setFloatValues(mProps[i].from, mProps[i].to)
              .setSetter(mProps[i].setter)
              .setDuration(mDuration)
              .setInterpolator(mInterpolator);
            oa.start();
            // End action on the last property (all share the same duration).
            if (i == mPropCount - 1 && mEndCallback) {
                oa.animator().setEndCallback(mEndCallback, mEndUser);
            } else {
                oa.animator().setEndCallback(nullptr, nullptr);
            }
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

    View*             mView         = nullptr;
    AnimationManager* mManager      = nullptr;
    PropRequest       mProps[kMaxProps];
    ObjectAnimator    mObjAnims[kMaxProps];
    int               mPropCount    = 0;
    int               mActiveCount  = 0;
    uint32_t          mDuration     = 300;
    Interpolator      mInterpolator = Interpolator::LINEAR;
    EndCallback       mEndCallback  = nullptr;
    void*             mEndUser      = nullptr;
};

} // namespace litho
