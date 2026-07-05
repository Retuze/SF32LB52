/**
 * @file host_tick.hpp
 * @brief Host TickAdapter — millisecond tick source via OS monotonic clock.
 */

#pragma once

#include <stdint.h>
#include "port/tick_adapter.hpp"

namespace litho {

class HostTick : public TickAdapter {
public:
    uint32_t tickMs() override;
};

} // namespace litho
