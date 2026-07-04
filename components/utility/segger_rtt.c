/**
 * @file segger_rtt.c
 * @brief SEGGER RTT — shared-memory ring buffers for debugger I/O.
 *
 * SEGGER J-Link / Ozone scan memory for the _SEGGER_RTT control block
 * and read/write the attached ring buffers.  This is a polled (non-IRQ)
 * implementation — the debugger reads at its own pace.
 */

#include "segger_rtt.h"
#include <stdint.h>

/* ── SEGGER RTT control-block layout ──────────────────────────────────── */

typedef struct {
    const char *name;
    char       *buf;
    uint32_t    size;
    uint32_t    wr;
    uint32_t    rd;
    uint32_t    flags;
} segger_rtt_buf_t;

typedef struct {
    char     id[16];
    uint32_t num_up;
    uint32_t num_down;
    segger_rtt_buf_t up[1];
    segger_rtt_buf_t down[1];
} segger_rtt_cb_t;

#define UP_BUF_SIZE   1024U
#define DOWN_BUF_SIZE   64U
#define MODE_NO_BLOCK    0U

static char up_buf[UP_BUF_SIZE];
static char down_buf[DOWN_BUF_SIZE];

/* Placed in .data (not .bss) so the debugger can find the signature even
 * after the loader has touched the section. */
__attribute__((section(".data")))
volatile segger_rtt_cb_t _SEGGER_RTT = {
    .id       = "SEGGER RTT",
    .num_up   = 1,
    .num_down = 1,
    .up       = {{ .name = "Terminal", .buf = up_buf,
                   .size = UP_BUF_SIZE, .flags = MODE_NO_BLOCK }},
    .down     = {{ .name = "Terminal", .buf = down_buf,
                   .size = DOWN_BUF_SIZE }},
};

/* ── Up-buffer: target → host ─────────────────────────────────────────── */

void segger_rtt_send(const char *data, uint32_t len)
{
    segger_rtt_buf_t *up = (segger_rtt_buf_t *)&_SEGGER_RTT.up[0];
    uint32_t wr = up->wr;
    uint32_t rd = up->rd;
    uint32_t end = up->size;

    for (uint32_t i = 0U; i < len; i++) {
        uint32_t next = (wr + 1U < end) ? wr + 1U : 0U;
        if (next == rd) break;           /* buffer full */
        up->buf[wr] = data[i];
        wr = next;
    }
    up->wr = wr;
}

/* ── Down-buffer: host → target ───────────────────────────────────────── */

uint32_t segger_rtt_recv(char *data, uint32_t len)
{
    segger_rtt_buf_t *down = (segger_rtt_buf_t *)&_SEGGER_RTT.down[0];
    uint32_t wr = down->wr;
    uint32_t rd = down->rd;
    uint32_t end = down->size;
    uint32_t n = 0U;

    while (n < len && rd != wr) {
        data[n++] = down->buf[rd];
        rd = (rd + 1U < end) ? rd + 1U : 0U;
    }
    down->rd = rd;
    return n;
}
