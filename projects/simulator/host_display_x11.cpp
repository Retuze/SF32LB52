/**
 * @file host_display_x11.cpp
 * @brief Linux X11 display backend implementation.
 *
 * Creates an X11 window, maintains an XImage + 32-bit pixel buffer as
 * the backing store, converts RGB565 tiles to XRGB8888 on bitblt(),
 * and XPutImage's the surface on flush().
 *
 * Input events (mouse→simulated touch, keyboard, window close) are
 * collected in a ring buffer and drained by HostInput.
 */

#include "host_display_x11.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace litho {

// ═══════════════════════════════════════════════════════════════════
//  RGB565 → XRGB8888
// ═══════════════════════════════════════════════════════════════════

static inline uint32_t rgb565_to_xrgb(uint16_t c)
{
    // Extract 5/6/5 bits
    uint32_t r5 = (c >> 11) & 0x1F;
    uint32_t g6 = (c >>  5) & 0x3F;
    uint32_t b5 = (c      ) & 0x1F;
    // Expand to 8-bit
    uint32_t r8 = (r5 << 3) | (r5 >> 2);   // 5→8
    uint32_t g8 = (g6 << 2) | (g6 >> 4);   // 6→8
    uint32_t b8 = (b5 << 3) | (b5 >> 2);   // 5→8
    // Pack XRGB8888
    return (r8 << 16) | (g8 << 8) | b8;
}

// ═══════════════════════════════════════════════════════════════════
//  Event ring buffer
// ═══════════════════════════════════════════════════════════════════

void X11Display::pushEvent(const Event& e)
{
    int next = (mEventHead + 1) % kEventCap;
    if (next == mEventTail) return;
    mEvents[mEventHead] = e;
    mEventHead = next;
}

bool X11Display::pollEvent(Event& out)
{
    if (mEventTail == mEventHead) return false;
    out = mEvents[mEventTail];
    mEventTail = (mEventTail + 1) % kEventCap;
    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  Keysym mapping
// ═══════════════════════════════════════════════════════════════════

KeyCode X11Display::mapKeySym(KeySym ks)
{
    switch (ks) {
    case XK_Escape:    return KeyCode::ESC;
    case XK_Return:    return KeyCode::ENTER;
    case XK_space:     return KeyCode::SPACE;
    case XK_Left:      return KeyCode::LEFT;
    case XK_Right:     return KeyCode::RIGHT;
    case XK_Up:        return KeyCode::UP;
    case XK_Down:      return KeyCode::DOWN;
    default:           return KeyCode::NONE;
    }
}

// ═══════════════════════════════════════════════════════════════════
//  Event pump
// ═══════════════════════════════════════════════════════════════════

void X11Display::pumpEvents()
{
    if (!mDisplay) return;

    XEvent xev;
    while (XPending(mDisplay)) {
        XNextEvent(mDisplay, &xev);

        Event ev;
        ev.type = EventType::NONE;

        switch (xev.type) {

        case Expose:
            if (xev.xexpose.count == 0) {
                flush();
            }
            break;

        case ButtonPress:
            if (xev.xbutton.button == Button1) {
                mButtonDown = true;
                ev.type = EventType::TOUCH;
                ev.touch.action = TouchAction::DOWN;
                ev.touch.x = xev.xbutton.x;
                ev.touch.y = xev.xbutton.y;
                pushEvent(ev);
            }
            break;

        case ButtonRelease:
            if (xev.xbutton.button == Button1) {
                mButtonDown = false;
                ev.type = EventType::TOUCH;
                ev.touch.action = TouchAction::UP;
                ev.touch.x = xev.xbutton.x;
                ev.touch.y = xev.xbutton.y;
                pushEvent(ev);
            }
            break;

        case MotionNotify:
            if (mButtonDown) {
                ev.type = EventType::TOUCH;
                ev.touch.action = TouchAction::MOVE;
                ev.touch.x = xev.xmotion.x;
                ev.touch.y = xev.xmotion.y;
                pushEvent(ev);
            }
            break;

        case KeyPress: {
            KeySym ks = XLookupKeysym(&xev.xkey, 0);
            KeyCode kc = mapKeySym(ks);
            if (kc == KeyCode::ESC) {
                ev.type = EventType::QUIT;
                pushEvent(ev);
            } else if (kc != KeyCode::NONE) {
                ev.type = EventType::KEY;
                ev.key.code = kc;
                ev.key.action = KeyAction::DOWN;
                pushEvent(ev);
            }
            break;
        }

        case ClientMessage:
            if ((Atom)xev.xclient.data.l[0] == mWmDeleteMsg) {
                ev.type = EventType::QUIT;
                pushEvent(ev);
            }
            break;

        case DestroyNotify:
            ev.type = EventType::QUIT;
            pushEvent(ev);
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
//  DisplayAdapter implementation
// ═══════════════════════════════════════════════════════════════════

X11Display::~X11Display()
{
    if (mImage) {
        // Detach the pixel buffer from the XImage before destroying
        mImage->data = nullptr;
        XDestroyImage(mImage);
    }
    free(mBackbuf);
    if (mGC)      XFreeGC(mDisplay, mGC);
    if (mWindow)  XDestroyWindow(mDisplay, mWindow);
    if (mDisplay) XCloseDisplay(mDisplay);
}

bool X11Display::init(int w, int h)
{
    mWidth  = w;
    mHeight = h;

    mDisplay = XOpenDisplay(nullptr);
    if (!mDisplay) {
        fprintf(stderr, "[sim] XOpenDisplay failed\n");
        return false;
    }

    int screen = DefaultScreen(mDisplay);
    int depth  = DefaultDepth(mDisplay, screen);
    Visual* visual = DefaultVisual(mDisplay, screen);

    // Create window
    mWindow = XCreateSimpleWindow(
        mDisplay,
        RootWindow(mDisplay, screen),
        0, 0, (unsigned)w, (unsigned)h,
        1,                              // border
        BlackPixel(mDisplay, screen),
        BlackPixel(mDisplay, screen));

    if (!mWindow) {
        fprintf(stderr, "[sim] XCreateSimpleWindow failed\n");
        return false;
    }

    // Window close protocol
    mWmDeleteMsg = XInternAtom(mDisplay, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(mDisplay, mWindow, &mWmDeleteMsg, 1);

    // Select input events
    XSelectInput(mDisplay, mWindow,
                 ExposureMask | StructureNotifyMask |
                 ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask |
                 KeyPressMask | KeyReleaseMask);

    XMapWindow(mDisplay, mWindow);
    XFlush(mDisplay);

    // Create GC
    mGC = XCreateGC(mDisplay, mWindow, 0, nullptr);

    // Allocate pixel buffer (XRGB8888)
    size_t bufSize = (size_t)w * h * sizeof(uint32_t);
    mBackbuf = (uint32_t*)malloc(bufSize);
    if (!mBackbuf) {
        fprintf(stderr, "[sim] malloc(%zu) failed\n", bufSize);
        return false;
    }
    memset(mBackbuf, 0, bufSize);

    // Create XImage backed by our buffer
    mImage = XCreateImage(mDisplay, visual, (unsigned)depth,
                          ZPixmap, 0, (char*)mBackbuf,
                          (unsigned)w, (unsigned)h, 32, 0);
    if (!mImage) {
        fprintf(stderr, "[sim] XCreateImage failed\n");
        return false;
    }

    printf("[sim] X11 window %dx%d created\n", w, h);
    return true;
}

void X11Display::bitblt(const uint16_t* data, int x, int y, int w, int h)
{
    if (!data || !mBackbuf || w <= 0 || h <= 0) return;

    // Clip
    if (x < 0) { const int skip = -x; w -= skip; data += skip; x = 0; }
    if (y < 0) { h += y; data += (-y) * w; y = 0; }
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

void X11Display::flush()
{
    if (!mDisplay || !mImage) return;
    XPutImage(mDisplay, mWindow, mGC, mImage,
              0, 0, 0, 0, (unsigned)mWidth, (unsigned)mHeight);
    XFlush(mDisplay);
}

} // namespace litho
