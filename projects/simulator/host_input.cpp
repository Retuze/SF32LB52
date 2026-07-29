/**
 * @file host_input.cpp
 * @brief Host InputAdapter implementation.
 *
 * Both platforms follow the same pattern: call the display's
 * pumpEvents() to drain the native event queue, then drain our
 * LithoUI event ring buffer via pollEvent() on the display.
 */

#include "host_input.hpp"

namespace litho {

#ifdef _WIN32
HostInput::HostInput(GdiDisplay& display) : mDisplay(display) {}
#else
HostInput::HostInput(X11Display& display) : mDisplay(display) {}
#endif

bool HostInput::pollEvent(Event& out)
{
    mDisplay.pumpEvents();
    return mDisplay.pollEvent(out);
}

} // namespace litho
