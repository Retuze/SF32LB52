#pragma once
#ifndef SEGGER_RTT_H
#define SEGGER_RTT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file segger_rtt.h
 * @brief SEGGER RTT (Real-Time Transfer) — shared-memory ring buffers
 *        for debugger communication (J-Link / Ozone).
 *
 *     segger_rtt_send(data, len)   — up-buffer:   target → host
 *     segger_rtt_recv(buf, len)    — down-buffer: host → target
 *
 * No init needed: the control block is statically initialized in .data.
 */

void     segger_rtt_send(const char *data, uint32_t len);
uint32_t segger_rtt_recv(char *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* SEGGER_RTT_H */
