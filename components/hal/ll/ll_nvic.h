#pragma once

#include <stdint.h>

#define SYST_CSR_ENABLE     (1UL << 0)
#define SYST_CSR_TICKINT    (1UL << 1)
#define SYST_CSR_CLKSOURCE  (1UL << 2)

void nvic_disable_all(void);
void nvic_clear_all_pending(void);
void nvic_set_vector_table(uint32_t vector_base);
void irq_disable_global(void);
void irq_enable_global(void);
