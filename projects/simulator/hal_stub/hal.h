/**
 * @file hal.h (stub)
 * @brief Minimal stub intercepting `#include "hal.h"` from LithoUI framework
 *        headers during SIMULATOR builds. Provides only dwt_cycles().
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

unsigned _sim_dwt_cycles(void);

/** Host equivalent of Cortex-M DWT cycle counter. Calls into hal_stub.c. */
static inline unsigned dwt_cycles(void)
{
    return _sim_dwt_cycles();
}

#ifdef __cplusplus
}
#endif
