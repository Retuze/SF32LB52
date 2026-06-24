/**
 * @file system.c
 * @brief System init, delay, and picolibc syscalls via UART.
 */

#include <stdint.h>
#include <sys/types.h>
#include "hal.h"
#include "hal_uart.h"

extern const uintptr_t g_pfnVectors[];

/* ---- System clock & tick ---------------------------------------------- */
static volatile uint32_t _tick_ms = 0;

void SystemInit(void)
{
    uint32_t systick_hz;

    SCB_VTOR = (uint32_t)(uintptr_t)g_pfnVectors;

    /* DWT cycle counter enable */
    SCB_DEMCR |= SCB_DEMCR_TRCENA;
    DWT_CYCCNT = 0U;
    DWT_CTRL  |= DWT_CTRL_CYCCNTENA;

    /* SysTick: 1 ms tick from the current HCLK. */
    _tick_ms = 0U;
    systick_hz = rcc_get_system_hz();
    if (systick_hz == 0U) {
        systick_hz = 240000000UL;
    }

    SYST_CSR = 0U;
    SYST_RVR = (systick_hz / 1000UL) - 1U;
    SYST_CVR = 0U;
    SYST_CSR = 0x7U;
}

uint32_t millis(void)
{
    return _tick_ms;
}

void delay(uint32_t ms)
{
    uint32_t start = _tick_ms;

    while ((uint32_t)(_tick_ms - start) < ms) {
    }
}

void SysTick_Handler(void)
{
    _tick_ms++;
}

/* ---- Picolibc syscalls (UART, posix-console) ------------------------- */

ssize_t write(int fd, const void *buf, size_t count)
{
    (void)fd;
    const char *p = (const char *)buf;
    for (size_t i = 0U; i < count; i++) {
        sf32lb52_uart1_write_byte((uint8_t)p[i]);
    }
    return (ssize_t)count;
}

ssize_t read(int fd, void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return 0;
}

off_t lseek(int fd, off_t offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    return 0;
}

int close(int fd)
{
    (void)fd;
    return 0;
}
