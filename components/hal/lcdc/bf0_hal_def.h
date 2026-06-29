#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef __IO
#define __IO volatile
#endif

#ifndef __weak
#define __weak __attribute__((weak))
#endif

typedef enum {
    HAL_OK      = 0x00,
    HAL_ERROR   = 0x01,
    HAL_BUSY    = 0x02,
    HAL_TIMEOUT = 0x03,
} HAL_StatusTypeDef;

typedef enum {
    HAL_UNLOCKED = 0x00,
    HAL_LOCKED   = 0x01,
} HAL_LockTypeDef;

#define UNUSED(x) ((void)(x))
#include "register.h"
#define HAL_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define HAL_MIN(a, b) (((a) < (b)) ? (a) : (b))
