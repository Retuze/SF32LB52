/**
 * @file hal_stub.c
 * @brief Stub implementations of HAL/BSP APIs for the PC simulator.
 *
 * These allow the same LithoUI framework code to compile on the host
 * without real hardware. Each function is a no-op or returns a
 * sensible default. Time functions use OS monotonic clocks.
 */

#include "hal_stub.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

/* ===================================================================
 *  GPIO stubs
 * =================================================================== */

void    pinMode(uint32_t pin, uint8_t mode)        { (void)pin; (void)mode; }
void    digitalWrite(uint32_t pin, uint8_t value)  { (void)pin; (void)value; }
uint8_t digitalRead(uint32_t pin)                  { (void)pin; return 0; }
void    digitalToggle(uint32_t pin)                { (void)pin; }

/* ===================================================================
 *  Clock stubs
 * =================================================================== */

void rcc_set_system_hz(uint32_t hz)         { (void)hz; }
void enable_flash_cache_prefetch(void)      {}

/* ===================================================================
 *  Time stubs
 * =================================================================== */

static uint32_t _start_ms = 0;

#ifdef _WIN32

static uint32_t host_millis(void)
{
    return (uint32_t)GetTickCount64();
}

void delay(uint32_t ms)
{
    Sleep(ms);
}

#else

static uint32_t host_millis(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
}

void delay(uint32_t ms)
{
    struct timespec ts = {
        .tv_sec  = ms / 1000U,
        .tv_nsec = (long)(ms % 1000U) * 1000000L,
    };
    nanosleep(&ts, NULL);
}

#endif

void SystemInit(void)
{
    _start_ms = host_millis();
}

uint32_t millis(void)
{
    return host_millis() - _start_ms;
}

void SysTick_Handler(void) {}

/* ===================================================================
 *  DWT cycle counter stub — called by hal_stub/hal.h inline wrapper
 * =================================================================== */

unsigned _sim_dwt_cycles(void)
{
#if defined(__clang__) || defined(__GNUC__)
    unsigned lo;
    __asm__ volatile("rdtsc" : "=a"(lo) : : "edx", "memory");
    return lo;
#elif defined(_MSC_VER)
    return (unsigned)__rdtsc();
#else
    return 0;
#endif
}

/* ===================================================================
 *  LCD stubs
 * =================================================================== */

void lcd_init(void *dev)                        { (void)dev; }
void lcd_set_brightness(void *dev, uint8_t pct) { (void)dev; (void)pct; }

// lcd_ref_fill_buf — called from window_manager.hpp extern declaration.
// Simulator doesn't need this (PFB tile memset handles background).
void lcd_ref_fill_buf(uint16_t* buf, int stride,
                      int w, int h, uint16_t color)
{
    (void)buf; (void)stride; (void)w; (void)h; (void)color;
}

// lcd_te_late_count — called from window_manager.hpp extern declaration.
// Returns count of late TE (tearing effect) signals. Always 0 on simulator.
int lcd_te_late_count(void)
{
    return 0;
}
