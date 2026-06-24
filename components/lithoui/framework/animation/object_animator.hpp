#pragma once
#include "value_animator.hpp"

namespace litho {

class ObjectAnimator {
public:
    ObjectAnimator& setTarget(void* target) { mTarget = target; return *this; }

    ObjectAnimator& setFloatValues(float from, float to) {
        mFrom = from; mTo = to; return *this;
    }

    ObjectAnimator& setSetter(void (*setter)(void*, float)) {
        mSetter = setter; return *this;
    }

    ObjectAnimator& setDuration(uint32_t ms) {
        mAnim.setDuration(ms); return *this;
    }

    ObjectAnimator& setInterpolator(Interpolator type) {
        mAnim.setInterpolator(type); return *this;
    }

    void start() {
        mAnim.setUpdateCallback([](float t, void* user) {
            auto* self = (ObjectAnimator*)user;
            float val = self->mFrom + t * (self->mTo - self->mFrom);
            self->mSetter(self->mTarget, val);
        }, this);
        mAnim.start();
    }

    bool isRunning() const { return mAnim.isRunning(); }
    ValueAnimator& animator() { return mAnim; }

private:
    ValueAnimator mAnim;
    void*  mTarget = nullptr;
    void (*mSetter)(void*, float) = nullptr;
    float  mFrom = 0;
    float  mTo   = 0;
};

} // namespace litho
