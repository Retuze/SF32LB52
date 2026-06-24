#pragma once
#include "value_animator.hpp"

namespace litho {

class AnimationManager {
public:
    static constexpr int kMaxAnimators = 16;

    void addAnimator(ValueAnimator* animator) {
        if (mCount >= kMaxAnimators) return;
        mAnimators[mCount++] = animator;
    }

    void removeAnimator(ValueAnimator* animator) {
        for (int i = 0; i < mCount; i++) {
            if (mAnimators[i] == animator) {
                for (int j = i; j < mCount - 1; j++)
                    mAnimators[j] = mAnimators[j + 1];
                mCount--;
                return;
            }
        }
    }

    void tick(uint32_t frameTimeMs) {
        for (int i = 0; i < mCount; i++) {
            mAnimators[i]->onFrame(frameTimeMs);
        }
    }

private:
    ValueAnimator* mAnimators[kMaxAnimators] = {};
    int            mCount = 0;
};

} // namespace litho
