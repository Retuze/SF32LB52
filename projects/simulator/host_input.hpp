/**
 * @file host_input.hpp
 * @brief Host InputAdapter — translates native events to LithoUI Events.
 *
 * Concrete display type is selected via platform #ifdef:
 *   Windows → GdiDisplay
 *   Linux   → X11Display
 */

#pragma once

#include "port/input_adapter.hpp"

#ifdef _WIN32
#include "host_display_gdi.hpp"
#else
#include "host_display_x11.hpp"
#endif

namespace litho {

class HostInput : public InputAdapter {
public:
#ifdef _WIN32
    explicit HostInput(GdiDisplay& display);
#else
    explicit HostInput(X11Display& display);
#endif

    bool pollEvent(Event& out) override;

private:
#ifdef _WIN32
    GdiDisplay& mDisplay;
#else
    X11Display& mDisplay;
#endif
};

} // namespace litho
