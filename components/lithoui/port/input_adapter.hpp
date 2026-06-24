#pragma once
#include "framework/event/event_types.hpp"

namespace litho {

class InputAdapter {
public:
    virtual ~InputAdapter() = default;
    virtual bool pollEvent(Event& out) = 0;
};

} // namespace litho
