#pragma once
#ifndef _SF32_DWT_H_
#define _SF32_DWT_H_

#include <stdint.h>

void dwt_init(void);
void dwt_set_us(uint32_t us);
uint32_t dwt_get_us(void);

#endif
