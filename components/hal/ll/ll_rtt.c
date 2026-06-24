#include "ll_rtt.h"

#include <stdint.h>

typedef struct {
    const char *name;
    char *buffer;
    uint32_t size;
    volatile uint32_t wr_off;
    volatile uint32_t rd_off;
    uint32_t flags;
} rtt_buffer_up_t;

typedef struct {
    const char *name;
    char *buffer;
    uint32_t size;
    volatile uint32_t wr_off;
    volatile uint32_t rd_off;
    uint32_t flags;
} rtt_buffer_down_t;

typedef struct {
    char id[16];
    int32_t max_num_up_buffers;
    int32_t max_num_down_buffers;
    rtt_buffer_up_t up[1];
    rtt_buffer_down_t down[1];
} rtt_cb_t;

#define RTT_UP_BUF_SIZE 1024u
#define RTT_DOWN_BUF_SIZE 64u
#define RTT_MODE_NO_BLOCK_SKIP 0u

static char g_rtt_up_buf[RTT_UP_BUF_SIZE];
static char g_rtt_down_buf[RTT_DOWN_BUF_SIZE];

__attribute__((section(".data")))
volatile rtt_cb_t _SEGGER_RTT = {
    "SEGGER RTT",
    1,
    1,
    {
        {
            "Terminal",
            g_rtt_up_buf,
            RTT_UP_BUF_SIZE,
            0u,
            0u,
            RTT_MODE_NO_BLOCK_SKIP
        }
    },
    {
        {
            "Terminal",
            g_rtt_down_buf,
            RTT_DOWN_BUF_SIZE,
            0u,
            0u,
            0u
        }
    }
};

void rtt_init(void)
{
    _SEGGER_RTT.up[0].wr_off = 0u;
    _SEGGER_RTT.up[0].rd_off = 0u;
    _SEGGER_RTT.down[0].wr_off = 0u;
    _SEGGER_RTT.down[0].rd_off = 0u;
}

void rtt_write(const char *data, uint32_t len)
{
    uint32_t i;
    rtt_buffer_up_t *up = (rtt_buffer_up_t *)&_SEGGER_RTT.up[0];
    uint32_t wr = up->wr_off;
    uint32_t rd = up->rd_off;
    uint32_t size = up->size;

    for (i = 0u; i < len; i++) {
        uint32_t next = wr + 1u;
        if (next >= size) {
            next = 0u;
        }
        if (next == rd) {
            break;
        }
        up->buffer[wr] = data[i];
        wr = next;
    }
    up->wr_off = wr;
}

uint32_t rtt_read(char *data, uint32_t len)
{
    uint32_t i = 0u;
    rtt_buffer_down_t *down = (rtt_buffer_down_t *)&_SEGGER_RTT.down[0];
    uint32_t wr = down->wr_off;
    uint32_t rd = down->rd_off;
    uint32_t size = down->size;

    while (i < len && rd != wr) {
        data[i++] = down->buffer[rd];
        rd++;
        if (rd >= size) {
            rd = 0u;
        }
    }
    down->rd_off = rd;
    return i;
}
