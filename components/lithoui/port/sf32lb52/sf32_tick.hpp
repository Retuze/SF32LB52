#pragma once
#include <stdint.h>
extern "C" {
uint32_t millis(void);
}
#include "port/tick_adapter.hpp"

namespace litho {

class SF32Tick : public TickAdapter {
public:
    uint32_t tickMs() override { return millis(); }
};

} // namespace litho
