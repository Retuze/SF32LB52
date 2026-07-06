/**
 * @file host_display_gdi.cpp
 * @brief Windows GDI display backend implementation.
 *
 * Creates a native Win32 window, maintains a 32-bit DIB section as the
 * backing store, converts LithoUI RGB565 tiles → XRGB8888 on bitblt(),
 * and BitBlt's the full surface on flush().
 *
 * Input events (mouse → simulated touch, keyboard, window close) are
 * collected in a lock-free ring buffer and drained by HostInput.
 */

#include "host_display_gdi.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>   // atoi
#include <shellscalingapi.h>  // SetProcessDPIAware, GetDpiForMonitor

namespace litho {

// ═══════════════════════════════════════════════════════════════════
//  DPI awareness — prevent Windows from bitmap-stretching our window
// ═══════════════════════════════════════════════════════════════════

namespace {

int get_dpi_scale(HWND hwnd)
{
    // Try per-monitor DPI (Windows 8.1+)
    HMONITOR hm = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (hm) {
        UINT dpiX = 0, dpiY = 0;
        HRESULT hr = GetDpiForMonitor(hm, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
        if (SUCCEEDED(hr) && dpiX > 0) {
            // Round to nearest integer scale: 96→1, 120→1, 144→2, 192→2, 288→3
            int scale = (dpiX + 48) / 96;
            if (scale < 1) scale = 1;
            return scale;
        }
    }
    // Fallback: system DPI
    HDC screen = GetDC(nullptr);
    int dpi = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(nullptr, screen);
    int scale = (dpi + 48) / 96;
    if (scale < 1) scale = 1;
    return scale;
}

} // anonymous namespace

// GET_X_LPARAM / GET_Y_LPARAM are in <windowsx.h>
#include <windowsx.h>

// ═══════════════════════════════════════════════════════════════════
//  RGB565 → XRGB8888 conversion
// ═══════════════════════════════════════════════════════════════════

static inline uint32_t rgb565_to_bgra(uint16_t c)
{
    // Extract 5/6/5 bits
    uint32_t r5 = (c >> 11) & 0x1F;
    uint32_t g6 = (c >>  5) & 0x3F;
    uint32_t b5 = (c      ) & 0x1F;
    // Expand to 8-bit (0..31 → 0..255, 0..63 → 0..255)
    uint32_t r8 = (r5 << 3) | (r5 >> 2);   // 5→8
    uint32_t g8 = (g6 << 2) | (g6 >> 4);   // 6→8
    uint32_t b8 = (b5 << 3) | (b5 >> 2);   // 5→8
    // GDI 32-bit DIB: little-endian uint32 = 0xAA_RR_GG_BB in memory.
    // bits[ 0.. 7] = Blue  → byte[0] on LE = Blue
    // bits[ 8..15] = Green → byte[1] on LE = Green
    // bits[16..23] = Red   → byte[2] on LE = Red
    // bits[24..31] = Alpha → byte[3] on LE = Alpha (0xFF = fully opaque)
    return 0xFF000000u | (r8 << 16) | (g8 << 8) | b8;
}

// ═══════════════════════════════════════════════════════════════════
//  Event ring buffer
// ═══════════════════════════════════════════════════════════════════

void GdiDisplay::pushEvent(const Event& e)
{
    int next = (mEventHead + 1) % kEventCap;
    if (next == mEventTail) return;          // queue full — drop
    mEvents[mEventHead] = e;
    mEventHead = next;
}

bool GdiDisplay::pollEvent(Event& out)
{
    if (mEventTail == mEventHead) return false;
    out = mEvents[mEventTail];
    mEventTail = (mEventTail + 1) % kEventCap;
    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  Window proc
// ═══════════════════════════════════════════════════════════════════

KeyCode GdiDisplay::mapVKey(WPARAM vk)
{
    switch (vk) {
    case VK_ESCAPE: return KeyCode::ESC;
    case VK_RETURN: return KeyCode::ENTER;
    case VK_SPACE:  return KeyCode::SPACE;
    case VK_LEFT:   return KeyCode::LEFT;
    case VK_RIGHT:  return KeyCode::RIGHT;
    case VK_UP:     return KeyCode::UP;
    case VK_DOWN:   return KeyCode::DOWN;
    default:        return KeyCode::NONE;
    }
}

LRESULT CALLBACK GdiDisplay::sWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    // Retrieve the GdiDisplay* stored during WM_CREATE
    GdiDisplay* self = nullptr;
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = static_cast<GdiDisplay*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<GdiDisplay*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (self) return self->wndProc(msg, wp, lp);
    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT GdiDisplay::wndProc(UINT msg, WPARAM wp, LPARAM lp)
{
    Event ev;
    ev.type = EventType::NONE;

    switch (msg) {

    case WM_LBUTTONDOWN:
        mButtonDown = true;
        ev.type = EventType::TOUCH;
        ev.touch.action = TouchAction::DOWN;
        ev.touch.x = GET_X_LPARAM(lp) / mScale;
        ev.touch.y = GET_Y_LPARAM(lp) / mScale;
        pushEvent(ev);
        return 0;

    case WM_LBUTTONUP:
        mButtonDown = false;
        ev.type = EventType::TOUCH;
        ev.touch.action = TouchAction::UP;
        ev.touch.x = GET_X_LPARAM(lp) / mScale;
        ev.touch.y = GET_Y_LPARAM(lp) / mScale;
        pushEvent(ev);
        return 0;

    case WM_MOUSEMOVE:
        if (mButtonDown) {
            ev.type = EventType::TOUCH;
            ev.touch.action = TouchAction::MOVE;
            ev.touch.x = GET_X_LPARAM(lp) / mScale;
            ev.touch.y = GET_Y_LPARAM(lp) / mScale;
            pushEvent(ev);
        }
        return 0;

    case WM_KEYDOWN: {
        KeyCode kc = mapVKey(wp);
        if (kc == KeyCode::ESC) {
            ev.type = EventType::QUIT;
            pushEvent(ev);
        } else if (kc != KeyCode::NONE) {
            ev.type = EventType::KEY;
            ev.key.code = kc;
            ev.key.action = KeyAction::DOWN;
            pushEvent(ev);
        }
        return 0;
    }

    case WM_CLOSE:
        ev.type = EventType::QUIT;
        pushEvent(ev);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_ERASEBKGND:
        // Prevent flicker — we always paint the full bitmap
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC paintDc = BeginPaint(mHwnd, &ps);
        if (mMemDc) {
            if (mScale == 1) {
                BitBlt(paintDc, ps.rcPaint.left, ps.rcPaint.top,
                       ps.rcPaint.right - ps.rcPaint.left,
                       ps.rcPaint.bottom - ps.rcPaint.top,
                       mMemDc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
            } else {
                SetStretchBltMode(paintDc, COLORONCOLOR);
                StretchBlt(paintDc, 0, 0, mWinW, mWinH,
                           mMemDc, 0, 0, mWidth, mHeight, SRCCOPY);
            }
        }
        EndPaint(mHwnd, &ps);
        return 0;
    }
    }

    return DefWindowProc(mHwnd, msg, wp, lp);
}

// ═══════════════════════════════════════════════════════════════════
//  Event pump — drain Windows message queue into our ring buffer
// ═══════════════════════════════════════════════════════════════════

void GdiDisplay::pumpEvents()
{
    MSG msg;
    while (PeekMessage(&msg, mHwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  DisplayAdapter implementation
// ═══════════════════════════════════════════════════════════════════

GdiDisplay::~GdiDisplay()
{
    if (mOldBmp) SelectObject(mMemDc, mOldBmp);
    if (mDib)    DeleteObject(mDib);
    if (mMemDc)  DeleteDC(mMemDc);
    if (mHdc)    ReleaseDC(mHwnd, mHdc);
    if (mHwnd)   DestroyWindow(mHwnd);
    // Note: window class is never unregistered (process lifetime)
}

bool GdiDisplay::init(int w, int h)
{
    mWidth  = w;
    mHeight = h;

    // ── DPI awareness (prevent system bitmap scaling) ────────────
    // Declare this process as DPI-aware so Windows doesn't
    // stretch our window content. We do our own integer scaling.
    SetProcessDPIAware();

    // Check env var LITHO_SCALE for manual override
    const char* envScale = getenv("LITHO_SCALE");
    if (envScale) {
        mScale = atoi(envScale);
        if (mScale < 1) mScale = 1;
    }

    // ── Register window class ──────────────────────────────────
    HINSTANCE hInst = GetModuleHandle(nullptr);

    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = sWndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = TEXT("LithoSimWnd");

    ATOM atom = RegisterClassEx(&wc);
    if (!atom) {
        fprintf(stderr, "[sim] RegisterClassEx failed (err=%lu)\n", GetLastError());
        return false;
    }

    // ── Create a temporary window to detect DPI scale ────────────
    // We create a hidden window first, get the monitor DPI, then
    // recreate the window at the correct size.
    HWND tmpHwnd = CreateWindowEx(
        0, TEXT("LithoSimWnd"), TEXT(""), WS_POPUP,
        0, 0, 1, 1, nullptr, nullptr, hInst, this);

    if (!mScale) {  // auto-detect if not overridden by env var
        mScale = get_dpi_scale(tmpHwnd);
    }
    DestroyWindow(tmpHwnd);

    // Scaled window client area
    mWinW = mWidth  * mScale;
    mWinH = mHeight * mScale;

    // ── Adjust window size for client area ──────────────────────
    RECT rect = {0, 0, (LONG)mWinW, (LONG)mWinH};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&rect, style, FALSE);

    mHwnd = CreateWindowEx(
        0,
        TEXT("LithoSimWnd"),
        TEXT("Watch Simulator"),
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, hInst, this);   // lpParam = this

    if (!mHwnd) {
        fprintf(stderr, "[sim] CreateWindowEx failed (err=%lu)\n", GetLastError());
        return false;
    }

    // ── Create DIB section at native resolution (390×450) ──────
    // We render at native res, then integer-scale via StretchBlt.
    mHdc = GetDC(mHwnd);
    mMemDc = CreateCompatibleDC(mHdc);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize     = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth    = mWidth;
    bmi.bmiHeader.biHeight   = -mHeight;  // top-down DIB
    bmi.bmiHeader.biPlanes   = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    mDib = CreateDIBSection(mMemDc, &bmi, DIB_RGB_COLORS,
                            (void**)&mBackbuf, nullptr, 0);
    if (!mDib) {
        fprintf(stderr, "[sim] CreateDIBSection failed (err=%lu)\n", GetLastError());
        return false;
    }

    mOldBmp = (HBITMAP)SelectObject(mMemDc, mDib);

    // Fill with black
    memset(mBackbuf, 0, (size_t)mWidth * mHeight * 4);

    // ── Show the window ─────────────────────────────────────────
    ShowWindow(mHwnd, SW_SHOW);
    UpdateWindow(mHwnd);

    printf("[sim] GDI window %dx%d (framebuffer %dx%d, scale %dx)\n",
           mWinW, mWinH, mWidth, mHeight, mScale);
    return true;
}

void GdiDisplay::bitblt(const uint16_t* data, int x, int y, int w, int h)
{
    if (!data || !mBackbuf || w <= 0 || h <= 0) return;

    // Clip to screen bounds
    if (x < 0) { w += x; data -= x; x = 0; }
    if (y < 0) { h += y; data -= y * w; y = 0; }  // careful: careful with shift
    if (x + w > mWidth)  w = mWidth  - x;
    if (y + h > mHeight) h = mHeight - y;
    if (w <= 0 || h <= 0) return;

    for (int row = 0; row < h; row++) {
        uint32_t*       dst = mBackbuf + (y + row) * mWidth + x;
        const uint16_t* src = data + row * w;
        for (int col = 0; col < w; col++) {
            dst[col] = rgb565_to_bgra(src[col]);
        }
    }
}

void GdiDisplay::flush()
{
    if (!mMemDc || !mHdc) return;
    // Integer-scale the 390×450 DIB to the window client area.
    // Use nearest-neighbour (COLORONCOLOR / no smoothing) for
    // crisp pixel art — important for watch UI debugging.
    if (mScale == 1) {
        BitBlt(mHdc, 0, 0, mWidth, mHeight, mMemDc, 0, 0, SRCCOPY);
    } else {
        SetStretchBltMode(mHdc, COLORONCOLOR);
        StretchBlt(mHdc, 0, 0, mWinW, mWinH,
                   mMemDc, 0, 0, mWidth, mHeight, SRCCOPY);
    }
}

} // namespace litho
