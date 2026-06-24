#pragma once
#include "port/input_adapter.hpp"

namespace litho {

class SF32Input : public InputAdapter {
public:
    bool pollEvent(Event& out) override {
        out.type = EventType::NONE;
        return false;  // stub â€?no touch yet
    }
};

} // namespace litho
