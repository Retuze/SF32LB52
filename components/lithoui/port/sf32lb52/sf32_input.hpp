#pragma once
#include "port/input_adapter.hpp"

extern "C" {
#include "tp.h"
#include "board.h"
}

namespace litho {

class SF32Input : public InputAdapter {
public:
    SF32Input() {
        board_tp_init();
    }

    bool pollEvent(Event& out) override {
        if (!g_tp_irq_fired) return false;
        g_tp_irq_fired = 0;

        int x, y, ev;
        if (tp_read(&x, &y, &ev) != 0) return false;

        out.type = EventType::TOUCH;
        out.touch.x      = x;
        out.touch.y      = y;
        out.touch.handler   = nullptr;
        out.touch.handlerSX = 0;
        out.touch.handlerSY = 0;

        switch (ev) {
        case TP_EVENT_DOWN: out.touch.action = TouchAction::DOWN; break;
        case TP_EVENT_UP:   out.touch.action = TouchAction::UP;   break;
        default:            out.touch.action = TouchAction::MOVE; break;
        }
        return true;
    }
};

} // namespace litho
