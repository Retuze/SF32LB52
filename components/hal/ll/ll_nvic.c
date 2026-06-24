#include "ll_nvic.h"
#include <stdint.h>

#define SCS_BASE        (0xE000E000UL)
#define NVIC_BASE       (SCS_BASE + 0x0100UL)
#define SCB_BASE        (SCS_BASE + 0x0D00UL)
#define SCNSCB_BASE     (SCS_BASE + 0x0D04UL)

typedef struct {
    volatile uint32_t ICTR;
} SF32_SCnSCB_TypeDef;

typedef struct {
    volatile uint32_t CPUID;
    volatile uint32_t ICSR;
    volatile uint32_t VTOR;
} SF32_SCB_TypeDef;

typedef struct {
    volatile uint32_t ISER[8];
    uint32_t RESERVED0[24];
    volatile uint32_t ICER[8];
    uint32_t RESERVED1[24];
    volatile uint32_t ISPR[8];
    uint32_t RESERVED2[24];
    volatile uint32_t ICPR[8];
} SF32_NVIC_TypeDef;

#define SCnSCB  ((SF32_SCnSCB_TypeDef *)SCNSCB_BASE)
#define SCB     ((SF32_SCB_TypeDef *)SCB_BASE)
#define NVIC    ((SF32_NVIC_TypeDef *)NVIC_BASE)

static inline void sf32_dsb(void)
{
    __asm volatile ("dsb 0xF" ::: "memory");
}

static inline void sf32_isb(void)
{
    __asm volatile ("isb 0xF" ::: "memory");
}

static inline void sf32_disable_irq(void)
{
    __asm volatile ("cpsid i" ::: "memory");
}

static inline void sf32_enable_irq(void)
{
    __asm volatile ("cpsie i" ::: "memory");
}

static uint32_t nvic_group_count(void)
{
    return (SCnSCB->ICTR & 0xFUL) + 1U;
}

void nvic_disable_all(void)
{
    uint32_t groups = nvic_group_count();

    for (uint32_t i = 0; i < groups; ++i) {
        NVIC->ICER[i] = 0xFFFFFFFFUL;
    }
}

void nvic_clear_all_pending(void)
{
    uint32_t groups = nvic_group_count();

    for (uint32_t i = 0; i < groups; ++i) {
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }
}

void nvic_set_vector_table(uint32_t vector_base)
{
    SCB->VTOR = vector_base;
    sf32_dsb();
    sf32_isb();
}

void irq_disable_global(void)
{
    sf32_disable_irq();
}

void irq_enable_global(void)
{
    sf32_enable_irq();
}
