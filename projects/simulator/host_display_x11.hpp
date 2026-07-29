/**
 * @file host_display_x11.hpp
 * @brief Linux X11 display backend — window + RGB565→XRGB8888 blit.
 */

#pragma once

#include "port/display_adapter.hpp"
#include "framework/event/event_types.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace litho {

class X11Display : public DisplayAdapter {
public:
    ~X11Display() override;

    // ── DisplayAdapter ──────────────────────────────────────────
    bool init(int width, int height) override;
    void bitblt(const uint16_t* data, int x, int y, int w, int h) override;
    void flush() override;
    int  width()  const override { return mWidth; }
    int  height() const override { return mHeight; }

    // ── Event queue (called by HostInput) ──────────────────────
    void pumpEvents();
    bool pollEvent(Event& out);

    ::Display* nativeDisplay() const { return mDisplay; }
    ::Window   nativeWindow()  const { return mWindow; }

private:
    void pushEvent(const Event& e);

    static constexpr int kEventCap = 64;
    Event  mEvents[kEventCap];
    int    mEventHead = 0;
    int    mEventTail = 0;

    // Map X11 keysym to LithoUI KeyCode
    static KeyCode mapKeySym(KeySym ks);

    // ── X11 state ───────────────────────────────────────────────
    ::Display* mDisplay = nullptr;
    ::Window   mWindow  = 0;
    ::GC       mGC      = nullptr;
    ::XImage*  mImage   = nullptr;
    uint32_t*  mBackbuf = nullptr;
    int        mWidth   = 0;
    int        mHeight  = 0;
    Atom       mWmDeleteMsg = 0;
    bool       mButtonDown   = false;
};

} // namespace litho
