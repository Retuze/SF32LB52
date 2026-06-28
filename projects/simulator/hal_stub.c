/**
 * @file hal_stub.c
 * @brief Stub implementations of HAL/BSP APIs for the PC simulator.
 *
 * These allow the same UI/application code to compile on the host
 * without real hardware. Each function is a no-op or returns a
 * sensible default.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* ---- GPIO stubs ---- */
void pinMode(uint32_t pin, uint8_t mode)      { (void)pin; (void)mode; }
void digitalWrite(uint32_t pin, uint8_t value) { (void)pin; (void)value; }
uint8_t digitalRead(uint32_t pin)              { (void)pin; return 0; }
void digitalToggle(uint32_t pin)               { (void)pin; }

/* ---- Clock stub ---- */
void clk_set_hz(uint32_t hz) { (void)hz; }

/* ---- Time stubs (host clock_gettime) ---- */
static uint32_t _start_ms = 0;

static uint32_t host_millis(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
}

void SystemInit(void)
{
    _start_ms = host_millis();
}

uint32_t millis(void)
{
    return host_millis() - _start_ms;
}

void delay(uint32_t ms)
{
    struct timespec ts = {
        .tv_sec  = ms / 1000U,
        .tv_nsec = (long)(ms % 1000U) * 1000000L,
    };
    nanosleep(&ts, NULL);
}

void SysTick_Handler(void) {}

/* ---- LCD stubs ---- */
void lcd_init(void *dev)              { (void)dev; }
void lcd_set_brightness(void *dev, uint8_t pct) { (void)dev; (void)pct; }
