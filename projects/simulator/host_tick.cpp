/**
 * @file host_tick.cpp
 * @brief Host TickAdapter implementation.
 */

#include "host_tick.hpp"
#include "hal_stub.h"

namespace litho {

uint32_t HostTick::tickMs()
{
    return millis();
}

} // namespace litho
