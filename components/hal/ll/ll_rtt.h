#ifndef RTT_H
#define RTT_H

#include <stdint.h>

void rtt_init(void);
void rtt_write(const char *data, uint32_t len);
uint32_t rtt_read(char *data, uint32_t len);

#endif
