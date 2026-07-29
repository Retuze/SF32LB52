#pragma once
#include "framework/activity/activity.hpp"
#include "framework/activity/transition.hpp"
#include "framework/window/window_manager.hpp"
#include "framework/animation/view_property_animator.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>

namespace litho {

using ActivityFactory = Activity* (*)();

class ActivityManager {
public:
    static constexpr int kMaxActivities = 8;
    static constexpr int kMaxStack      = 8;

    ActivityManager(WindowManager& wm) : mWindowManager(wm) {}

    void registerActivity(const char* name, ActivityFactory factory) {
        if (mRegistryCount >= kMaxActivities) {
            fprintf(stderr, "ActivityManager: registry full\n");
            return;
        }
        mRegistry[mRegistryCount].name    = name;
        mRegistry[mRegistryCount].factory = factory;
        mRegistryCount++;
    }

    template<typename T>
    void registerActivity(const char* name) {
        registerActivity(name, []() -> Activity* { return new T(); });
    }

    void startActivity(Intent& intent) {
        startActivity(intent, TransitionSpec::none());
    }

    void startActivity(Intent& intent, const TransitionSpec& spec) {
        if (mTransitioning) {
            fprintf(stderr, "ActivityManager: transition in progress\n");
            return;
        }

        const char* name = intent.target;
        if (!name) {
            fprintf(stderr, "ActivityManager: Intent has no target\n");
            return;
        }

        ActivityFactory factory = findFactory(name);
        if (!factory) {
            fprintf(stderr, "ActivityManager: unknown activity '%s'\n", name);
            return;
        }

        Activity* prev = (mCount > 0) ? mStack[mCount - 1] : nullptr;
        if (prev) prev->onPause();

        assert(mCount < kMaxStack && "activity stack overflow");

        Activity* a = factory();
        a->setManager(this);

        Window* win = mWindowManager.createWindow();
        a->setWindow(win);

        a->onCreate(intent.extras);
        a->onStart();
        a->onResume();

        mStack[mCount++] = a;

        View* newRoot = win->rootView();
        View* oldRoot = (prev && prev->mWindow) ? prev->mWindow->rootView() : nullptr;

        if (spec.isNone() || !newRoot) {
            if (prev) {
                prev->onStop();
                hideWindow(prev); // keep in stack, but do not composite
            }
            fullInvalidate(win);
            return;
        }

        mTransitioning = true;
        mPendingStop   = prev; // defer onStop until enter anim ends

        // Block input for the whole transition (avoids pressed-state flicker).
        if (win) win->setInputEnabled(false);
        if (prev && prev->mWindow) prev->mWindow->setInputEnabled(false);
        // Only prev + new are visible; deeper stopped windows stay hidden.

        applyEnterStart(newRoot, spec);
        if (oldRoot) applyExitStart(oldRoot, spec);

        // Run enter anim on new root (drives end callback). Exit runs in parallel.
        if (oldRoot && spec.hasExit()) {
            startLayerAnim(oldRoot, spec, /*enter=*/false, /*endCb=*/nullptr);
        }
        startLayerAnim(newRoot, spec, /*enter=*/true, &ActivityManager::onEnterAnimEnd);

        fullInvalidate(win);
        if (prev && prev->mWindow) fullInvalidate(prev->mWindow);
    }

    void finishActivity(Activity* a) {
        finishActivity(a, TransitionSpec::none());
    }

    void finishActivity(Activity* a, const TransitionSpec& spec) {
        if (!a) return;
        if (mTransitioning) {
            fprintf(stderr, "ActivityManager: transition in progress\n");
            return;
        }

        int idx = -1;
        for (int i = 0; i < mCount; i++) {
            if (mStack[i] == a) { idx = i; break; }
        }
        if (idx < 0) return;

        bool isTop = (idx == mCount - 1);
        Activity* below = (isTop && mCount > 1) ? mStack[mCount - 2] : nullptr;

        a->onPause();

        // Pop = reverse of the open pair (Android-style):
        //   open.enter → finish.exit  (top leaves the way it came in)
        //   open.exit  → finish.enter (below returns the way it left)
        // Pure slideFromRight: only enter was set → top exits Right, below stays.
        // Push: both set → top exits Right, below enters from Left.
        TransitionSpec finishSpec = spec;
        {
            LayerAnim openEnter = spec.enter;
            LayerAnim openExit  = spec.exit;
            finishSpec.exit  = openEnter;
            finishSpec.enter = openExit;
        }

        View* root = (a->mWindow) ? a->mWindow->rootView() : nullptr;
        View* belowRoot = (below && below->mWindow) ? below->mWindow->rootView() : nullptr;

        if (below) {
            below->onStart();
            below->onResume();
            showWindow(below);
        }

        if (finishSpec.isNone() || !root || !isTop) {
            if (belowRoot) resetRoot(belowRoot);
            destroyActivityAt(idx);
            return;
        }

        mTransitioning  = true;
        mPendingDestroy = a;
        mPendingDestroyIdx = idx;

        // Drop capture + ignore further touches while the page is exiting.
        if (a->mWindow) a->mWindow->setInputEnabled(false);
        if (below && below->mWindow) below->mWindow->setInputEnabled(false);

        // Top: identity → exit (fade 255→0 / slide off).
        applyExitStart(root, finishSpec);
        startLayerAnim(root, finishSpec, /*enter=*/false, &ActivityManager::onExitAnimEnd);

        // Below: enter start → identity (fade 0→255 / slide in), Android pair.
        if (belowRoot && finishSpec.hasEnter()) {
            applyEnterStart(belowRoot, finishSpec);
            startLayerAnim(belowRoot, finishSpec, /*enter=*/true, /*endCb=*/nullptr);
        } else if (belowRoot) {
            resetRoot(belowRoot);
        }

        fullInvalidate(a->mWindow);
        if (below && below->mWindow) fullInvalidate(below->mWindow);
    }

    Activity* currentActivity() {
        return (mCount > 0) ? mStack[mCount - 1] : nullptr;
    }

    WindowManager& windowManager() { return mWindowManager; }
    bool isTransitioning() const { return mTransitioning; }

private:
    void fullInvalidate(Window* win) {
        if (!win) return;
        win->invalidateRect({0, 0,
            (int16_t)mWindowManager.displayWidth(),
            (int16_t)mWindowManager.displayHeight()});
    }

    static void slideOffset(SlideEdge edge, int W, int H, int16_t& outX, int16_t& outY) {
        outX = 0; outY = 0;
        switch (edge) {
        case SlideEdge::Right:  outX = (int16_t)W;  break;
        case SlideEdge::Left:   outX = (int16_t)-W; break;
        case SlideEdge::Bottom: outY = (int16_t)H;  break;
        case SlideEdge::Top:    outY = (int16_t)-H; break;
        default: break;
        }
    }

    // Enter scale starts small; exit shrinks toward this (0.1×).
    static constexpr uint32_t kScaleFrom = View::kScaleOne / 10; // 0.1 in 16.16

    void applyEnterStart(View* root, const TransitionSpec& spec) {
        int W = mWindowManager.displayWidth();
        int H = mWindowManager.displayHeight();
        int16_t tx = 0, ty = 0;
        slideOffset(spec.enter.slide, W, H, tx, ty);
        root->setTranslationX(tx);
        root->setTranslationY(ty);
        if (spec.enter.fade) root->setAlpha(0);
        else root->setAlpha(255);
        if (spec.enter.scale) root->setScale(kScaleFrom);
        else root->setScale(View::kScaleOne);
    }

    void applyExitStart(View* root, const TransitionSpec& spec) {
        // Exit starts at identity; animation targets off-screen / fade / shrink.
        (void)spec;
        root->setTranslationX(0);
        root->setTranslationY(0);
        root->setAlpha(255);
        root->setScale(View::kScaleOne);
    }

    using EndFn = void (*)(void*);

    void startLayerAnim(View* root, const TransitionSpec& spec, bool enter, EndFn endCb) {
        const LayerAnim& layer = enter ? spec.enter : spec.exit;
        int W = mWindowManager.displayWidth();
        int H = mWindowManager.displayHeight();

        auto& anim = root->animate()
            .setDuration(spec.durationMs)
            .setInterpolator(spec.ease);

        if (enter) {
            if (layer.slide != SlideEdge::None) {
                if (layer.slide == SlideEdge::Left || layer.slide == SlideEdge::Right)
                    anim.translationX(0);
                else
                    anim.translationY(0);
            }
            if (layer.fade) anim.alpha(255);
            if (layer.scale) anim.scale((float)View::kScaleOne);
        } else {
            if (layer.slide != SlideEdge::None) {
                int16_t tx = 0, ty = 0;
                // Exit toward the same edge name: Right → slide to +W
                slideOffset(layer.slide, W, H, tx, ty);
                if (tx != 0) anim.translationX((float)tx);
                if (ty != 0) anim.translationY((float)ty);
            }
            if (layer.fade) anim.alpha(0);
            if (layer.scale) anim.scale((float)kScaleFrom);
        }

        if (endCb) anim.withEndAction(endCb, this);
        anim.start(mWindowManager.animationManager());
    }

    static void resetRoot(View* root) {
        if (!root) return;
        root->setTranslationX(0);
        root->setTranslationY(0);
        root->setAlpha(255);
        root->setScale(View::kScaleOne);
    }

    static void hideWindow(Activity* a) {
        if (a && a->mWindow) a->mWindow->setVisible(false);
    }
    static void showWindow(Activity* a) {
        if (a && a->mWindow) a->mWindow->setVisible(true);
    }

    static void onEnterAnimEnd(void* user) {
        auto* self = (ActivityManager*)user;
        if (self->mPendingStop) {
            // Push/fade exit leaves the underlying page off-screen / transparent —
            // restore identity before it becomes visible again on back.
            if (self->mPendingStop->mWindow) {
                View* oldRoot = self->mPendingStop->mWindow->rootView();
                if (oldRoot) {
                    oldRoot->animate().cancel(&self->mWindowManager.animationManager());
                    resetRoot(oldRoot);
                }
            }
            self->mPendingStop->onStop();
            // Stopped ≠ destroyed: stay on back stack, but never draw under fades.
            hideWindow(self->mPendingStop);
            self->mPendingStop = nullptr;
        }
        if (self->mCount > 0 && self->mStack[self->mCount - 1]->mWindow) {
            resetRoot(self->mStack[self->mCount - 1]->mWindow->rootView());
            self->mStack[self->mCount - 1]->mWindow->setInputEnabled(true);
        }
        self->mTransitioning = false;
        if (self->mCount > 0)
            self->fullInvalidate(self->mStack[self->mCount - 1]->mWindow);
    }

    static void onExitAnimEnd(void* user) {
        auto* self = (ActivityManager*)user;
        Activity* below = nullptr;
        if (self->mPendingDestroyIdx > 0 && self->mPendingDestroyIdx < self->mCount)
            below = self->mStack[self->mPendingDestroyIdx - 1];

        if (self->mPendingDestroy) {
            self->destroyActivityAt(self->mPendingDestroyIdx);
            self->mPendingDestroy = nullptr;
            self->mPendingDestroyIdx = -1;
        }
        // Ensure the revealed activity is fully visible and interactive.
        if (below && below->mWindow) {
            resetRoot(below->mWindow->rootView());
            below->mWindow->setInputEnabled(true);
            self->fullInvalidate(below->mWindow);
        } else if (self->mCount > 0 && self->mStack[self->mCount - 1]->mWindow) {
            resetRoot(self->mStack[self->mCount - 1]->mWindow->rootView());
            self->mStack[self->mCount - 1]->mWindow->setInputEnabled(true);
            self->fullInvalidate(self->mStack[self->mCount - 1]->mWindow);
        }
        self->mTransitioning = false;
    }

    void destroyActivityAt(int idx) {
        if (idx < 0 || idx >= mCount) return;
        Activity* a = mStack[idx];
        a->onStop();
        a->onDestroy();
        Window* w = a->mWindow;
        mWindowManager.destroyWindow(w);
        for (int i = idx; i < mCount - 1; i++) mStack[i] = mStack[i + 1];
        mCount--;
        delete a;
    }

    ActivityFactory findFactory(const char* name) {
        for (int i = 0; i < mRegistryCount; i++) {
            if (strcmp(mRegistry[i].name, name) == 0)
                return mRegistry[i].factory;
        }
        return nullptr;
    }

    struct Entry { const char* name; ActivityFactory factory; };

    WindowManager& mWindowManager;
    Entry          mRegistry[kMaxActivities];
    int            mRegistryCount = 0;
    Activity*      mStack[kMaxStack] = {};
    int            mCount = 0;

    bool      mTransitioning      = false;
    Activity* mPendingStop        = nullptr;
    Activity* mPendingDestroy     = nullptr;
    int       mPendingDestroyIdx  = -1;
};

} // namespace litho
