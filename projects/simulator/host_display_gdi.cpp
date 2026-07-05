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

namespace litho {

// GET_X_LPARAM / GET_Y_LPARAM are in <windowsx.h>
#include <windowsx.h>

// ═══════════════════════════════════════════════════════════════════
//  RGB565 → XRGB8888 conversion
// ═══════════════════════════════════════════════════════════════════

static inline uint32_t rgb565_to_xrgb(uint16_t c)
{
    // Extract 5/6/5 bits
    uint32_t r5 = (c >> 11) & 0x1F;
    uint32_t g6 = (c >>  5) & 0x3F;
    uint32_t b5 = (c      ) & 0x1F;
    // Expand to 8-bit (0..31 → 0..255, 0..63 → 0..255)
    uint32_t r8 = (r5 << 3) | (r5 >> 2);   // 5→8
    uint32_t g8 = (g6 << 2) | (g6 >> 4);   // 6→8
    uint32_t b8 = (b5 << 3) | (b5 >> 2);   // 5→8
    // Pack XRGB8888
    return (r8 << 16) | (g8 << 8) | b8;
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
        ev.touch.x = GET_X_LPARAM(lp);
        ev.touch.y = GET_Y_LPARAM(lp);
        pushEvent(ev);
        return 0;

    case WM_LBUTTONUP:
        mButtonDown = false;
        ev.type = EventType::TOUCH;
        ev.touch.action = TouchAction::UP;
        ev.touch.x = GET_X_LPARAM(lp);
        ev.touch.y = GET_Y_LPARAM(lp);
        pushEvent(ev);
        return 0;

    case WM_MOUSEMOVE:
        if (mButtonDown) {
            ev.type = EventType::TOUCH;
            ev.touch.action = TouchAction::MOVE;
            ev.touch.x = GET_X_LPARAM(lp);
            ev.touch.y = GET_Y_LPARAM(lp);
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
        // Validate the dirty rect and repaint from our DIB
        PAINTSTRUCT ps;
        HDC paintDc = BeginPaint(mHwnd, &ps);
        if (mMemDc) {
            BitBlt(paintDc, ps.rcPaint.left, ps.rcPaint.top,
                   ps.rcPaint.right - ps.rcPaint.left,
                   ps.rcPaint.bottom - ps.rcPaint.top,
                   mMemDc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
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

    // ── Adjust window size for client area ──────────────────────
    RECT rect = {0, 0, (LONG)w, (LONG)h};
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

    // ── Create DIB section for the backing buffer ───────────────
    mHdc = GetDC(mHwnd);
    mMemDc = CreateCompatibleDC(mHdc);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize     = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth    = w;
    bmi.bmiHeader.biHeight   = -h;  // top-down DIB (negative height)
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
    memset(mBackbuf, 0, (size_t)w * h * 4);

    // ── Show the window ─────────────────────────────────────────
    ShowWindow(mHwnd, SW_SHOW);
    UpdateWindow(mHwnd);

    printf("[sim] GDI window %dx%d created\n", w, h);
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
            dst[col] = rgb565_to_xrgb(src[col]);
        }
    }
}

void GdiDisplay::flush()
{
    if (!mMemDc || !mHdc) return;
    BitBlt(mHdc, 0, 0, mWidth, mHeight, mMemDc, 0, 0, SRCCOPY);
}

} // namespace litho
