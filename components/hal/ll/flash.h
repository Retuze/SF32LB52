#pragma once
#ifndef FLASH_H
#define FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file flash.h
 * @brief SPI NOR Flash — switch from ROM single-line to Quad QSPI 48 MHz.
 *
 * Must run from RAM (bootloader executes entirely from RAM) because it
 * reconfigures the MPI2 Flash controller while XIP is active.
 */
void flash_quad_init(void);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_H */
