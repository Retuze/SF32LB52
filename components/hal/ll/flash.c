/**
 * @file flash.c
 * @brief MPI2 NOR Flash — ROM single-line → Quad QSPI 48 MHz STR.
 *
 * Must run from RAM (bootloader .link.ld places everything in RAM).
 */

#include "flash.h"
#include "hal.h"

/* ── MPI2 register map ────────────────────────────────────────────────── */

#define MPI2_BASE     0x50042000UL
#define MPI2_CR       (*(volatile uint32_t*)(MPI2_BASE + 0x00))
#define MPI2_DR       (*(volatile uint32_t*)(MPI2_BASE + 0x04))
#define MPI2_PSCLR    (*(volatile uint32_t*)(MPI2_BASE + 0x0C))
#define MPI2_SR       (*(volatile uint32_t*)(MPI2_BASE + 0x10))
#define MPI2_SCR      (*(volatile uint32_t*)(MPI2_BASE + 0x14))
#define MPI2_CMDR1    (*(volatile uint32_t*)(MPI2_BASE + 0x18))
#define MPI2_AR1      (*(volatile uint32_t*)(MPI2_BASE + 0x1C))
#define MPI2_ABR1     (*(volatile uint32_t*)(MPI2_BASE + 0x20))
#define MPI2_DLR1     (*(volatile uint32_t*)(MPI2_BASE + 0x24))
#define MPI2_CCR1     (*(volatile uint32_t*)(MPI2_BASE + 0x28))
#define MPI2_HCMDR    (*(volatile uint32_t*)(MPI2_BASE + 0x40))
#define MPI2_HRABR    (*(volatile uint32_t*)(MPI2_BASE + 0x44))
#define MPI2_HRCCR    (*(volatile uint32_t*)(MPI2_BASE + 0x48))
#define MPI2_MISCR    (*(volatile uint32_t*)(MPI2_BASE + 0x58))
#define MPI2_TIMR     (*(volatile uint32_t*)(MPI2_BASE + 0x70))

#define CCR1_IMODE_Pos  0U
#define CCR1_DMODE_Pos  18U
#define CCR1_FMODE_Pos  21U

#define CSR_SEL_MPI2_Pos  6U
#define CSR_SEL_MPI2_Msk  (3UL << CSR_SEL_MPI2_Pos)
#define CLK_FLASH_DLL2    2UL

#define MPI_LINE_SINGLE  1UL
#define MPI_LINE_QUAD    3UL

#define HRCCR(im, adm, ads, abm, abs, dcyc, dm) \
    (((im)<<0) | ((adm)<<3) | ((ads)<<6) | ((abm)<<8) | ((abs)<<11) | ((dcyc)<<13) | ((dm)<<18))

/* ── Debug helpers ────────────────────────────────────────────────────── */

static void dbg_puts(const char *s) { while (*s) uart_putc(*s++); uart_flush(); }

static void dbg_hex32(uint32_t v)
{
    static const char h[] = "0123456789ABCDEF";
    dbg_puts("0x");
    for (int i = 28; i >= 0; i -= 4) uart_putc(h[(v >> i) & 0xFU]);
}

static void dbg_words(const char *tag, const volatile uint32_t *p, int n)
{
    dbg_puts(tag);
    for (int i = 0; i < n; i++) { dbg_puts(" "); dbg_hex32(p[i]); }
    dbg_puts("\r\n");
}

static void dbg_mpi2(const char *tag)
{
    dbg_puts(tag);
    dbg_puts(" CSR=");   dbg_hex32(HPSYS_RCC->CSR.R);
    dbg_puts(" PSCLR="); dbg_hex32(MPI2_PSCLR);
    dbg_puts(" SR=");    dbg_hex32(MPI2_SR);
    dbg_puts("\r\n");
}

/* ── MPI2 helpers ─────────────────────────────────────────────────────── */

static void mpi2_wait(void)  { while (!(MPI2_SR & 1U)) {} MPI2_SCR = 1U; }
static void mpi2_cmd(uint8_t cmd) { MPI2_CCR1=1UL<<CCR1_IMODE_Pos; MPI2_AR1=0; MPI2_CMDR1=cmd; mpi2_wait(); }

static uint32_t mpi2_read(uint8_t cmd, uint32_t n)
{
    MPI2_CCR1 = (1UL<<CCR1_IMODE_Pos) | (1UL<<CCR1_DMODE_Pos);
    MPI2_DLR1 = n - 1U; MPI2_AR1 = 0; MPI2_CMDR1 = cmd; mpi2_wait();
    return MPI2_DR;
}

static uint8_t mpi2_read8(uint8_t cmd) { return (uint8_t)(mpi2_read(cmd, 1) & 0xFFU); }

static void mpi2_write8(uint8_t cmd, uint8_t data)
{
    MPI2_DR = data; MPI2_DLR1 = 0;
    MPI2_CCR1 = (1UL<<CCR1_IMODE_Pos) | (1UL<<CCR1_DMODE_Pos) | (1UL<<CCR1_FMODE_Pos);
    MPI2_AR1 = 0; MPI2_CMDR1 = cmd; mpi2_wait();
}

static void mpi2_wait_wip(void) {
    for (int i = 0; i < 100000; i++) if (!(mpi2_read8(0x05) & 1U)) return;
}

static int flash_read_ok(void) {
    uint32_t sp = *(volatile uint32_t*)0x12020000UL;
    return sp >= 0x20000000UL && sp <= 0x20080000UL;
}

/* ── Read mode switching ──────────────────────────────────────────────── */

static void set_single_read(void) {
    MPI2_HRCCR = HRCCR(1UL, 1UL, 2UL, 0UL, 0UL, 8UL, 1UL);
    MPI2_HCMDR = (MPI2_HCMDR & ~0xFFUL) | 0x0BUL;
}

static void set_quad_read(void) {
    MPI2_HRABR = 0xFF;
    MPI2_HRCCR = HRCCR(1UL, 1UL, 2UL, 0UL, 0UL, 8UL, 3UL);
    MPI2_HCMDR = (MPI2_HCMDR & ~0xFFUL) | 0x6BUL;
}

/* ── Public API ───────────────────────────────────────────────────────── */

void flash_quad_init(void)
{
    dbg_words("[flash] entry", (const volatile uint32_t*)0x12020000UL, 4);
    dbg_mpi2("[flash] regs entry");

    /* 1. DLL2 @ 288 MHz → MPI2 clock = 72 MHz */
    HPSYS_RCC->DLL2CR.R = 0U;
    HPSYS_RCC->DLL2CR.R = (1UL << 6) | (1UL << 12) | (12UL << 2);
    HPSYS_RCC->DLL2CR.R |= 1UL;
    for (volatile uint32_t to = 1000000UL; to; --to) {
        if (HPSYS_RCC->DLL2CR.R & (1UL << 31)) break;
    }

    MPI2_PSCLR = 4;
    HPSYS_RCC->CSR.R = (HPSYS_RCC->CSR.R & ~CSR_SEL_MPI2_Msk) | (CLK_FLASH_DLL2 << CSR_SEL_MPI2_Pos);

    /* 2. Identify flash */
    uint32_t id = mpi2_read(0x9F, 3);
    uint8_t sr1 = mpi2_read8(0x05), sr2 = mpi2_read8(0x35);
    dbg_puts("[flash] JEDEC="); dbg_hex32(id & 0xFFFFFFU);
    dbg_puts(" SR1="); dbg_hex32(sr1);
    dbg_puts(" SR2="); dbg_hex32(sr2);
    dbg_puts("\r\n");

    /* 3. Enable QE (Puya SR2 bit1) */
    if (!(sr2 & 2U)) {
        mpi2_cmd(0x06);
        mpi2_write8(0x31, 0x02);
        mpi2_wait_wip();
        sr2 = mpi2_read8(0x35);
        dbg_puts("[flash] QE set, SR2="); dbg_hex32(sr2); dbg_puts("\r\n");
    }

    /* 4. Switch to quad read, verify, fallback if needed */
    if (sr2 & 2U) {
        set_quad_read();
        if (flash_read_ok()) {
            dbg_puts("[flash] quad read OK (0x6B)\r\n");
        } else {
            set_single_read();
            dbg_puts("[flash] quad read invalid, fell back to single 0x0B\r\n");
        }
    } else {
        dbg_puts("[flash] QE not set, staying single-line\r\n");
    }

    dbg_mpi2("[flash] regs final");
    dbg_words("[flash] final words", (const volatile uint32_t*)0x12020000UL, 4);
}
