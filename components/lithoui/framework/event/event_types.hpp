#pragma once
#include <stdint.h>

namespace litho {

enum class TouchAction : uint8_t { DOWN = 0, MOVE = 1, UP = 2 };

struct TouchEvent {
    int         x;
    int         y;
    TouchAction action;
    void*       handler;
    int         handlerSX;
    int         handlerSY;
};

enum class KeyAction : uint8_t { DOWN = 0, UP = 1 };

enum class KeyCode : uint32_t {
    NONE   = 0,
    ESC    = 9,
    ENTER  = 36,
    SPACE  = 65,
    LEFT   = 113,
    RIGHT  = 114,
    UP     = 111,
    DOWN   = 116,
};

struct KeyEvent {
    KeyCode   code;
    KeyAction action;
};

enum class EventType : uint8_t {
    NONE  = 0,
    TOUCH = 1,
    KEY   = 2,
    QUIT  = 3,
};

struct Event {
    EventType type;

    Event() : type(EventType::NONE) {
        touch.handler   = nullptr;
        touch.handlerSX = 0;
        touch.handlerSY = 0;
    }

    union {
        TouchEvent touch;
        KeyEvent   key;
    };
};

} // namespace litho
