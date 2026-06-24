#pragma once
#ifndef BB_GPIO_H
#define BB_GPIO_H

#include <stdint.h>

#define INPUT            0
#define OUTPUT           1
#define INPUT_PULLUP     2
#define INPUT_PULLDOWN   3
#define OUTPUT_OPENDRAIN 4


#define LOW  0
#define HIGH 1

void    pinMode(uint32_t pin, uint8_t mode);
void    digitalWrite(uint32_t pin, uint8_t value);
uint8_t digitalRead(uint32_t pin);
void    digitalToggle(uint32_t pin);

#endif
