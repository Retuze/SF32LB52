/**
 * @file tp.h
 * @brief Touch panel framework — three-layer architecture.
 *
 *   Panel (tp.c)         — INT/RST pins, init orchestration, touch API
 *   Bus   (tp_bus_*.c)   — transport protocol (I2C, SPI)
 *   IC    (tp_ic_*.c)    — chip-specific commands (FT6146, CST816, GT911, ...)
 *
 * Similar to LCD framework: decouples hardware config (board.c) from drivers (bsp).
 */

#pragma once
#ifndef TP_H
#define TP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Bus ──────────────────────────────────────────────────────────────── */

/**
 * Transport layer — I2C or SPI communication.
 *
 * For I2C: uses bit-bang I2C (bb_i2c_t) internally.
 * For SPI: uses hardware or bit-bang SPI.
 */
typedef struct tp_bus {
    void (*init)(const void *config);  /* config = bb_i2c_t* for I2C bus */
    int  (*read)(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len);
    int  (*write)(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len);
} tp_bus_t;

/* ── IC ───────────────────────────────────────────────────────────────── */

/**
 * Touch IC driver — chip-specific initialization and touch reading.
 */
typedef struct tp_ic {
    const char *name;
    uint8_t dev_addr;  /* I2C 7-bit address (or SPI CS index) */

    int  (*init)(const tp_bus_t *bus, uint8_t dev_addr);
    int  (*read_touch)(const tp_bus_t *bus, uint8_t dev_addr,
                       int *x, int *y, int *event);
    uint32_t (*read_id)(const tp_bus_t *bus, uint8_t dev_addr);
} tp_ic_t;

/* ── Touch event encoding ────────────────────────────────────────────── */

/* Matches litho::TouchAction and FT6146 hardware encoding */
enum {
    TP_EVENT_DOWN = 0,
    TP_EVENT_UP   = 1,
    TP_EVENT_MOVE = 2,
};

/* ── Registration ────────────────────────────────────────────────────── */

void tp_set_bus(const tp_bus_t *bus, const void *bus_config);
void tp_set_ic(const tp_ic_t *ic);
void tp_set_ctrl_pins(uint32_t rst, uint32_t irq);
void tp_set_resolution(uint16_t w, uint16_t h);

/* ── Public API ──────────────────────────────────────────────────────── */

int  tp_init(void);
int  tp_read(int *x, int *y, int *event);
bool tp_touched(void);
uint32_t tp_read_id(void);

/* ── IRQ signaling (shared with IC driver) ───────────────────────────── */

extern volatile int g_tp_irq_fired;
extern volatile int g_tp_irq_cnt;

/* ── Available bus drivers ──────────────────────────────────────────────── */

extern const tp_bus_t tp_bus_i2c;   /* Bit-bang I2C */

/* ── Available IC drivers ───────────────────────────────────────────────── */

extern const tp_ic_t tp_ic_ft6146;  /* FocalTech FT6146 */

#ifdef __cplusplus
}
#endif

#endif /* TP_H */
