#pragma once
#include "port/input_adapter.hpp"

extern "C" {
#include "tp_ft6146.h"
#include "board.h"
#include "lcd.h"
}

/* ~1.25µs delay for 400kHz I2C at 240MHz */
static void i2c_delay(void)
{
    for (volatile uint32_t i = 0U; i < 80U; ++i) {
        __asm volatile("nop");
    }
}

namespace litho {

class SF32Input : public InputAdapter {
public:
    SF32Input() {
        tp_.i2c.pin_sda = CTP_SDA;
        tp_.i2c.pin_scl = CTP_SCL;
        tp_.i2c.half_period = i2c_delay;
        tp_.pin_int = CTP_INT;
        tp_.pin_rst = CTP_RST;
        tp_.max_x   = LCD_WIDTH;
        tp_.max_y   = LCD_HEIGHT;
        tp_ft6146_init(&tp_);
    }

    bool pollEvent(Event& out) override {
        if (!g_tp_irq_fired) return false;
        g_tp_irq_fired = 0;

        int x, y, ev;
        if (tp_ft6146_read(&tp_, &x, &y, &ev) != 0) return false;

        const char *act;
        switch (ev) {
        case TP_EVENT_DOWN: act = "DN"; break;
        case TP_EVENT_UP:   act = "UP"; break;
        case TP_EVENT_MOVE: act = "MV"; break;
        default:            act = "??"; break;
        }
        printf("[TP] #%lu %s (%d,%d)\r\n",
               (unsigned long)g_tp_irq_cnt, act, x, y);

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

private:
    tp_ft6146_t tp_;
};

} // namespace litho
