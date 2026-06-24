/**
 * @file system.c — Bootloader system init & picolibc syscalls via UART.
 */

#include <stdint.h>
#include <sys/types.h>
#include "hal.h"
#include "hal_uart.h"

static volatile uint32_t _tick_ms = 0;

extern const uintptr_t g_pfnVectors[];

void SystemInit(void)
{
    SCB_VTOR = (uint32_t)(uintptr_t)g_pfnVectors;

    SYST_RVR = 239999U;
    SYST_CVR = 0U;
    SYST_CSR = 0x7U;
}

uint32_t millis(void) { return _tick_ms; }

void delay(uint32_t ms)
{
    uint32_t end = _tick_ms + ms;
    while (_tick_ms < end) {
        __asm volatile("wfi");
    }
}

void SysTick_Handler(void) { _tick_ms++; }

/* ---- picolibc syscalls (UART) ---- */
ssize_t write(int fd, const void *buf, size_t count)
{
    (void)fd;
    const char *p = (const char *)buf;
    for (size_t i = 0U; i < count; i++) {
        uart_write_byte((uint8_t)p[i]);
    }
    return (ssize_t)count;
}

ssize_t read(int fd, void *buf, size_t count)    { (void)fd; (void)buf; return (ssize_t)count; }
off_t   lseek(int fd, off_t offset, int whence)  { (void)fd; (void)offset; (void)whence; return 0; }
int     close(int fd)                             { (void)fd; return 0; }
