#pragma once
#ifndef CACHE_H
#define CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file cache.h
 * @brief Enable I-Cache, D-Cache, and MPI2 Flash prefetch.
 *
 * Must run from RAM (.ramfunc) — it modifies the Flash MPU attributes.
 * Call once during early boot (bootloader), before jumping to firmware.
 */
void cache_enable(void);

#ifdef __cplusplus
}
#endif

#endif /* CACHE_H */
