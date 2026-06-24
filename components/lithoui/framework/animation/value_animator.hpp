#pragma once
#include <stdint.h>

namespace litho {

enum class Interpolator : uint8_t {
    LINEAR                = 0,
    ACCELERATE            = 1,
    DECELERATE            = 2,
    ACCELERATE_DECELERATE = 3,
};

inline float applyInterpolator(Interpolator type, float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    switch (type) {
    case Interpolator::ACCELERATE:
        return t * t;
    case Interpolator::DECELERATE:
        return 1.0f - (1.0f - t) * (1.0f - t);
    case Interpolator::ACCELERATE_DECELERATE:
        return t * t * (3.0f - 2.0f * t);
    default:
        return t;
    }
}

class ValueAnimator {
public:
    using UpdateCallback = void (*)(float fraction, void* user);

    ValueAnimator& setDuration(uint32_t ms) {
        mDurationMs = ms;
        return *this;
    }

    ValueAnimator& setInterpolator(Interpolator type) {
        mInterpolator = type;
        return *this;
    }

    ValueAnimator& setUpdateCallback(UpdateCallback cb, void* user) {
        mCallback = cb;
        mUser     = user;
        return *this;
    }

    ValueAnimator& setRepeatCount(int count) {
        mRepeatCount = count;
        return *this;
    }

    void start() {
        mWaitingStart = true;
        mRunning      = true;
        mRepeatRemaining = mRepeatCount;
    }

    void cancel() {
        mRunning = false;
    }

    bool isRunning() const { return mRunning; }

    void onFrame(uint32_t frameTimeMs) {
        if (!mRunning) return;

        if (mWaitingStart) {
            mStartTimeMs  = frameTimeMs;
            mWaitingStart = false;
        }

        float elapsed = (float)(frameTimeMs - mStartTimeMs);
        float raw     = elapsed / (float)mDurationMs;
        float t       = applyInterpolator(mInterpolator, raw);

        if (mCallback) mCallback(t, mUser);

        if (raw >= 1.0f) {
            if (mCallback) mCallback(1.0f, mUser);

            if (mRepeatRemaining > 0) {
                mRepeatRemaining--;
                mStartTimeMs  = frameTimeMs;
                mWaitingStart = false;
            } else if (mRepeatRemaining < 0) {
                // infinite repeat
                mStartTimeMs  = frameTimeMs;
                mWaitingStart = false;
            } else {
                mRunning = false;
            }
        }
    }

private:
    uint32_t      mStartTimeMs    = 0;
    uint32_t      mDurationMs     = 300;
    Interpolator  mInterpolator   = Interpolator::LINEAR;
    bool          mWaitingStart   = false;
    bool          mRunning        = false;
    int           mRepeatCount    = 0;
    int           mRepeatRemaining = 0;
    UpdateCallback mCallback      = nullptr;
    void*         mUser           = nullptr;
};

} // namespace litho
