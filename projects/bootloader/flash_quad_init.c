/**
 * @file flash_quad_init.c
 * @brief MPI2 NOR Flash: ROM single/24MHz -> Quad QSPI 48MHz STR.
 *
 * Bootloader runs entirely from RAM (link.ld), so modifying the Flash
 * controller while executing is safe.
 */
#include "hal.h"

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

/* MPI_CCR1 field positions */
#define CCR1_IMODE_Pos   0U    /* instruction line mode */
#define CCR1_DCYC_Pos    13U   /* dummy cycles */
#define CCR1_DMODE_Pos   18U   /* data line mode */
#define CCR1_FMODE_Pos   21U   /* 0 = read direction, 1 = write direction */

#define HPSYS_RCC_CSR_SEL_MPI2_Pos 6U
#define HPSYS_RCC_CSR_SEL_MPI2_Msk (3UL << HPSYS_RCC_CSR_SEL_MPI2_Pos)
#define RCC_CLK_FLASH_DLL2         2UL

#define MPI_LINE_NONE   0UL
#define MPI_LINE_SINGLE 1UL
#define MPI_LINE_QUAD   3UL

#define MPI_HRCCR(imode, admode, adsize, abmode, absize, dcyc, dmode) \
    (((imode) << 0) | ((admode) << 3) | ((adsize) << 6) | \
     ((abmode) << 8) | ((absize) << 11) | ((dcyc) << 13) | ((dmode) << 18))

#define USART_ISR_TXE  (1UL << 7)
#define USART_ISR_TC   (1UL << 6)

static void uart_putc(char c)
{
    while ((USART1->ISR & USART_ISR_TXE) == 0U) {}
    USART1->TDR = (uint8_t)c;
}

static void uart_puts(const char *s)
{
    while (*s) { uart_putc(*s++); }
    while ((USART1->ISR & USART_ISR_TC) == 0U) {}
}

static void uart_put_hex32(uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uart_putc(hex[(v >> i) & 0xFU]);
    }
}

static void dump_app_words(const char *tag)
{
    const volatile uint32_t *p = (const volatile uint32_t *)0x12020000UL;
    uart_puts(tag);
    for (int i = 0; i < 4; i++) {
        uart_puts(" ");
        uart_put_hex32(p[i]);
    }
    uart_puts("\r\n");
}

static void dump_mpi2_regs(const char *tag)
{
    uart_puts(tag);
    uart_puts(" CSR=");
    uart_put_hex32(HPSYS_RCC->CSR.R);
    uart_puts(" PSCLR=");
    uart_put_hex32(MPI2_PSCLR);
    uart_puts(" HCMDR=");
    uart_put_hex32(MPI2_HCMDR);
    uart_puts(" HRCCR=");
    uart_put_hex32(MPI2_HRCCR);
    uart_puts(" SR=");
    uart_put_hex32(MPI2_SR);
    uart_puts("\r\n");
}

/* Wait for SPI transfer complete, then clear the flag */
static void mpi2_wait_tcf(void) {
    while (!(MPI2_SR & 1U)) {}
    MPI2_SCR = 1U;
}

/* Issue a command with no address and no data (e.g. WREN 0x06) */
static void mpi2_cmd(uint8_t cmd) {
    MPI2_CCR1 = (1UL << CCR1_IMODE_Pos);  /* IMODE=1, no addr, no data */
    MPI2_AR1 = 0;
    MPI2_CMDR1 = cmd;
    mpi2_wait_tcf();
}

/*
 * Read up to 4 bytes after a single-line command. The data length register
 * (DLR1) MUST be programmed or the controller clocks out zero data bytes and
 * the read returns stale/zero — which is exactly why QE/ID reads failed.
 */
static uint32_t mpi2_cmd_read(uint8_t cmd, uint32_t nbytes) {
    MPI2_CCR1 = (1UL << CCR1_IMODE_Pos) | (1UL << CCR1_DMODE_Pos);  /* IMODE=1, DMODE=1 */
    MPI2_DLR1 = (nbytes - 1U);
    MPI2_AR1 = 0;
    MPI2_CMDR1 = cmd;
    mpi2_wait_tcf();
    return MPI2_DR;
}

static uint8_t mpi2_cmd_read8(uint8_t cmd) {
    return (uint8_t)(mpi2_cmd_read(cmd, 1) & 0xFFU);
}

/* Write 1 byte, then issue command (e.g. WRSR2 0x31) */
static void mpi2_cmd_write8(uint8_t cmd, uint8_t data) {
    MPI2_DR = data;
    MPI2_DLR1 = 0;  /* 1 byte */
    MPI2_CCR1 = (1UL << CCR1_IMODE_Pos) | (1UL << CCR1_DMODE_Pos) | (1UL << CCR1_FMODE_Pos);
    MPI2_AR1 = 0;
    MPI2_CMDR1 = cmd;
    mpi2_wait_tcf();
}

/* Poll WIP (Write In Progress) bit in Status Register 1 (0x05) */
static void mpi2_wait_wip(void) {
    for (int i = 0; i < 100000; i++) {
        if (!(mpi2_cmd_read8(0x05) & 1U)) return;
    }
}

/* Restore the ROM-default single-line Fast Read (0x0B) AHB path. */
static void set_single_fast_read(void) {
    MPI2_HRCCR = MPI_HRCCR(1UL, 1UL, 2UL, 0UL, 0UL, 8UL, 1UL);
    MPI2_HCMDR = (MPI2_HCMDR & ~0xFFUL) | 0x0BUL;
}

/* Quad Output Fast Read (0x6B): cmd+addr single, data quad, 8 dummy cycles
 * (matches the Puya entry in the SDK flash_table). */
static void set_quad_output_read(void) {
    MPI2_HRABR = 0xFF;
    MPI2_HRCCR = MPI_HRCCR(1UL, 1UL, 2UL, 0UL, 0UL, 8UL, 3UL);
    MPI2_HCMDR = (MPI2_HCMDR & ~0xFFUL) | 0x6BUL;
}

/* A valid Cortex-M vector table starts with an initial SP inside SRAM. */
static int flash_read_looks_valid(void) {
    uint32_t sp = *(volatile uint32_t*)0x12020000UL;
    return (sp >= 0x20000000UL) && (sp <= 0x20080000UL);
}

void boot_flash_quad_init(void)
{
    dump_app_words("[FLASH] entry");
    dump_mpi2_regs("[FLASH] regs entry");

    /* 1. Enable DLL2 @ 288 MHz */
    HPSYS_RCC->DLL2CR.R = 0U;
    HPSYS_RCC->DLL2CR.R = (1UL << 6) | (1UL << 12) | (12UL << 2);
    HPSYS_RCC->DLL2CR.R |= 1UL;
    for (volatile uint32_t to = 1000000UL; to; --to) {
        if (HPSYS_RCC->DLL2CR.R & (1UL << 31)) {
            break;
        }
    }

    /* 2. Switch MPI2 to DLL2 / 4 = 72 MHz. ROM single-line Fast Read stays
     * valid through the clock change. */
    MPI2_PSCLR = 4;
    HPSYS_RCC->CSR.R = (HPSYS_RCC->CSR.R & ~HPSYS_RCC_CSR_SEL_MPI2_Msk) |
                       (RCC_CLK_FLASH_DLL2 << HPSYS_RCC_CSR_SEL_MPI2_Pos);

    /* 3. Identify flash and read status (manual reads now program DLR1). */
    uint32_t id = mpi2_cmd_read(0x9F, 3);
    uint8_t sr1 = mpi2_cmd_read8(0x05);
    uint8_t sr2 = mpi2_cmd_read8(0x35);
    uart_puts("[FLASH] JEDEC=");
    uart_put_hex32(id & 0xFFFFFFU);
    uart_puts(" SR1=");
    uart_put_hex32(sr1);
    uart_puts(" SR2=");
    uart_put_hex32(sr2);
    uart_puts("\r\n");

    /* 4. Enable QE (Puya SR2 bit1) via volatile/non-volatile WRSR2. */
    if (!(sr2 & 2U)) {
        mpi2_cmd(0x06);                  /* WREN */
        mpi2_cmd_write8(0x31, 0x02);     /* WRSR2: QE=1 */
        mpi2_wait_wip();
        sr2 = mpi2_cmd_read8(0x35);
        uart_puts("[FLASH] QE set, SR2=");
        uart_put_hex32(sr2);
        uart_puts("\r\n");
    }

    /* 5. Switch to quad read only if QE is confirmed, then verify the AHB
     * window still returns a sane vector table; fall back otherwise. */
    if (sr2 & 2U) {
        set_quad_output_read();
        if (flash_read_looks_valid()) {
            uart_puts("[FLASH] quad read OK (0x6B)\r\n");
        } else {
            set_single_fast_read();
            uart_puts("[FLASH] quad read invalid, fell back to single 0x0B\r\n");
        }
    } else {
        uart_puts("[FLASH] QE not set, staying single-line\r\n");
    }

    dump_mpi2_regs("[FLASH] regs final");
    dump_app_words("[FLASH] final words");
}
