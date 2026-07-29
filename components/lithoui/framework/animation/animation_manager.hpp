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
        // 1) Advance all
        for (int i = 0; i < mCount; i++) {
            mAnimators[i]->onFrame(frameTimeMs);
        }

        // 2) Detach finished animators, then fire end callbacks.
        //    End callbacks may destroy views that own the animators — so we
        //    must not touch the list after firePendingEnd.
        ValueAnimator* finished[kMaxAnimators];
        int nFinished = 0;
        for (int i = 0; i < mCount; ) {
            if (!mAnimators[i]->isRunning()) {
                finished[nFinished++] = mAnimators[i];
                for (int j = i; j < mCount - 1; j++)
                    mAnimators[j] = mAnimators[j + 1];
                mCount--;
            } else {
                i++;
            }
        }
        for (int i = 0; i < nFinished; i++) {
            finished[i]->firePendingEnd();
        }
    }

private:
    ValueAnimator* mAnimators[kMaxAnimators] = {};
    int            mCount = 0;
};

} // namespace litho
