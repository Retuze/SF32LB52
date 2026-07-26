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

    // Dump the current 390×450 backbuffer to a 24-bit BMP (headless capture).
    // Bypasses the window / DPI / StretchBlt path entirely. Returns false on
    // I/O error or if the backbuffer is not allocated.
    bool saveBmp(const char* path) const;

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
    HDC        mMemDc     = nullptr;   // memory DC (DIB section)
    HBITMAP    mDib       = nullptr;   // DIB section (390x450)
    HBITMAP    mOldBmp    = nullptr;   // saved original bitmap
    uint32_t*  mBackbuf   = nullptr;   // pointer into DIB pixels (390x450)
    int        mWidth     = 0;         // framebuffer size (always 390)
    int        mHeight    = 0;         // framebuffer size (always 450)
    int        mScale     = 1;         // DPI scale factor (1=96dpi, 2=192dpi)
    int        mWinW      = 0;         // window client width (mWidth * mScale)
    int        mWinH      = 0;         // window client height (mHeight * mScale)
    bool       mButtonDown = false;
};

} // namespace litho
