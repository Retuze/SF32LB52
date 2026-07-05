/**
 * @file host_display_gdi.hpp
 * @brief Windows GDI display backend — window + RGB565→XRGB8888 blit.
 */

#pragma once

#include "port/display_adapter.hpp"
#include "framework/event/event_types.hpp"

#ifndef _WIN32
#  error "host_display_gdi.hpp is Windows-only"
#endif

#include <windows.h>

namespace litho {

class GdiDisplay : public DisplayAdapter {
public:
    ~GdiDisplay() override;

    // ── DisplayAdapter ──────────────────────────────────────────
    bool init(int width, int height) override;
    void bitblt(const uint16_t* data, int x, int y, int w, int h) override;
    void flush() override;
    int  width()  const override { return mWidth; }
    int  height() const override { return mHeight; }

    // ── Event queue (called by HostInput) ──────────────────────
    void pumpEvents();
    bool pollEvent(Event& out);

    HWND hwnd() const { return mHwnd; }

private:
    // Ring buffer for LithoUI events
    static constexpr int kEventCap = 64;
    Event  mEvents[kEventCap];
    int    mEventHead = 0;  // write pos
    int    mEventTail = 0;  // read pos

    void pushEvent(const Event& e);

    // ── Window proc ────────────────────────────────────────────
    static LRESULT CALLBACK sWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT wndProc(UINT msg, WPARAM wp, LPARAM lp);

    // Map Win32 VK_* to LithoUI KeyCode
    static KeyCode mapVKey(WPARAM vk);

    // ── Window / GDI state ─────────────────────────────────────
    HWND       mHwnd      = nullptr;
    HDC        mHdc       = nullptr;   // window DC
    HDC        mMemDc     = nullptr;   // memory DC (double-buffer)
    HBITMAP    mDib       = nullptr;   // DIB section
    HBITMAP    mOldBmp    = nullptr;   // saved original bitmap
    uint32_t*  mBackbuf   = nullptr;   // pointer into DIB pixels
    int        mWidth     = 0;
    int        mHeight    = 0;
    bool       mButtonDown = false;
};

} // namespace litho
