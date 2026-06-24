#pragma once
#include "framework/window/window.hpp"
#include "framework/intent/intent.hpp"

namespace litho {

class ActivityManager;

class Activity {
    friend class ActivityManager;
public:
    virtual ~Activity() = default;

    // Lifecycle
    virtual void onCreate(Bundle& state)    { (void)state; }
    virtual void onStart()                  {}
    virtual void onResume()                 {}
    virtual void onPause()                  {}
    virtual void onStop()                   {}
    virtual void onDestroy()                {}

    // Called by ActivityManager
    void setManager(ActivityManager* mgr) { mManager = mgr; }
    void setWindow(Window* win)           { mWindow = win; }

    ActivityManager& manager() { return *mManager; }

    void setContentView(ViewGroup* root) { mWindow->setContentView(root); }

    // Navigate
    void startActivity(Intent& intent);
    void finish();

protected:
    Window*          mWindow  = nullptr;
    ActivityManager* mManager = nullptr;
};

} // namespace litho
