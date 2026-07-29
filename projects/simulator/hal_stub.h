/**
 * @file hal_stub.h
 * @brief Host compatibility layer — must be included BEFORE any LithoUI headers.
 *
 * Provides:
 *   - DWT_CYCCNT via host CPU cycle counter (__rdtsc / __builtin_readcyclecounter)
 *   - HAL function stubs required by LithoUI framework
 *   - LCD/BSP function stubs
 */

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── DWT_CYCCNT — Cortex-M33 cycle counter → host CPU TSC ──────────
// LithoUI framework uses DWT_CYCCNT for performance stats (pfb.hpp,
// window_manager.hpp, painter.hpp). All three guard with #ifndef so
// defining it before inclusion overrides the ARM default.
//
// On x86 we use the CPU timestamp counter (rdtsc). It ticks at the
// CPU base clock (~3-4 GHz), giving higher resolution than the ARM
// 240 MHz DWT. The unsigned 32-bit wrap semantics match DWT exactly:
// differences d = t1 - t0 are correct even across 32-bit overflow.

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(__rdtsc)
#define DWT_CYCCNT ((uint32_t)__rdtsc())
#else
#define DWT_CYCCNT ((uint32_t)__builtin_readcyclecounter())
#endif

// ── HAL function stubs ─────────────────────────────────────────────

#ifdef __cplusplus
extern "C" {
#endif

// GPIO
void    pinMode(uint32_t pin, uint8_t mode);
void    digitalWrite(uint32_t pin, uint8_t value);
uint8_t digitalRead(uint32_t pin);
void    digitalToggle(uint32_t pin);

// Clock
void    rcc_set_system_hz(uint32_t hz);
void    enable_flash_cache_prefetch(void);

// Time
void     SystemInit(void);
uint32_t millis(void);
void     delay(uint32_t ms);
void     SysTick_Handler(void);

// LCD
void lcd_init(void *dev);
void lcd_set_brightness(void *dev, uint8_t pct);

// Framebuffer helper — called by window_manager.hpp for BgView fill.
// Not needed on simulator (PFB tile memset replaces it), but the
// declaration must exist to satisfy the extern.
void lcd_ref_fill_buf(uint16_t* buf, int stride,
                      int w, int h, uint16_t color);

#ifdef __cplusplus
}
#endif
