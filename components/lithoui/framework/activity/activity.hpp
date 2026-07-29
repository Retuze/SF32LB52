#pragma once
#include "framework/window/window.hpp"
#include "framework/intent/intent.hpp"
#include "framework/activity/transition.hpp"

namespace litho {

class ActivityManager;

class Activity {
    friend class ActivityManager;
public:
    virtual ~Activity() = default;

    virtual void onCreate(Bundle& state)    { (void)state; }
    virtual void onStart()                  {}
    virtual void onResume()                 {}
    virtual void onPause()                  {}
    virtual void onStop()                   {}
    virtual void onDestroy()                {}

    void setManager(ActivityManager* mgr) { mManager = mgr; }
    void setWindow(Window* win)           { mWindow = win; }

    ActivityManager& manager() { return *mManager; }
    Window* window() const { return mWindow; }

    void setContentView(ViewGroup* root) { mWindow->setContentView(root); }

    void startActivity(Intent& intent);
    void startActivity(Intent& intent, const TransitionSpec& spec);
    void finish();
    void finish(const TransitionSpec& spec);

protected:
    Window*          mWindow  = nullptr;
    ActivityManager* mManager = nullptr;
};

} // namespace litho
