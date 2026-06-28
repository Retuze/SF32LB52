#pragma once
#include "port/input_adapter.hpp"

extern "C" {
#include "tp_ft6146.h"
#include "board.h"
#include "lcd.h"
}

namespace litho {

class SF32Input : public InputAdapter {
public:
    SF32Input() {
        tp_.i2c.pin_sda = CTP_SDA;
        tp_.i2c.pin_scl = CTP_SCL;
        tp_.i2c.half_period = nullptr;
        tp_.pin_int = CTP_INT;
        tp_.pin_rst = CTP_RST;
        tp_.max_x   = LCD_WIDTH;
        tp_.max_y   = LCD_HEIGHT;
        tp_ft6146_init(&tp_);
    }

    bool pollEvent(Event& out) override {
        if (!g_tp_irq_fired) {
            return false;
        }
        g_tp_irq_fired = 0;

        int x, y, ev;
        if (tp_ft6146_read(&tp_, &x, &y, &ev) != 0) {
            return false;
        }

        out.type = EventType::TOUCH;
        out.touch.x      = x;
        out.touch.y      = y;
        out.touch.handler   = nullptr;
        out.touch.handlerSX = 0;
        out.touch.handlerSY = 0;

        switch (ev) {
        case 0: out.touch.action = TouchAction::DOWN; break;
        case 2: out.touch.action = TouchAction::UP;   break;
        case 1:
        default: out.touch.action = TouchAction::MOVE;  break;
        }

        return true;
    }

private:
    tp_ft6146_t tp_;
};

} // namespace litho
