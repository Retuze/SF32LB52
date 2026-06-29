/**
 * @file main.cpp
 * @brief LithoUI SF32LB52 demo — icon grid.
 *
 * Flash speed on this chip: ~81 KiB/s (no cache/prefetch).
 * All critical rendering code runs from RAM (.ramfunc).
 * Image pixel data stays in Flash (.rodata) — acceptable for static UI.
 * For smooth animation, use hardware LCDC peripheral.
 */

extern "C" {
#include "hal.h"
#include "board.h"
#include "lcd.h"
}

#include "core/litho_core.h"
#include "framework/view/view.hpp"
#include "framework/view/view_group.hpp"
#include "framework/window/window.hpp"
#include "framework/window/window_manager.hpp"
#include "framework/activity/activity.hpp"
#include "framework/activity/activity_manager.hpp"
#include "framework/intent/intent.hpp"
#include "framework/widget/image_view.hpp"
#include "res_images.h"

#include "port/sf32lb52/sf32_display.hpp"
#include "port/sf32lb52/sf32_input.hpp"
#include "port/sf32lb52/sf32_tick.hpp"

using namespace litho;

static const int kScreenW = LCD_WIDTH;
static const int kScreenH = LCD_HEIGHT;

class GalleryActivity : public Activity {
public:
    void onCreate(Bundle&) override {
        auto* root = new ViewGroup();
        root->bounds() = {0, 0, (int16_t)kScreenW, (int16_t)kScreenH};
        setContentView(root);

        static const int kCols = 3, kIconW = 100, kIconH = 100;
        static const int kGapX = (kScreenW - kCols * kIconW) / (kCols + 1);
        static const int kGapY = 15, kStartY = 40;

        ImageId icons[] = {
            IMG_DIAL, IMG_MESSAGES, IMG_MUSIC,
            IMG_SETTINGS, IMG_CAMERA, IMG_WEATHER,
            IMG_CALENDAR, IMG_COMPASS, IMG_SPORTS,
            IMG_SLEEP, IMG_ALARM, IMG_STOPWATCH,
        };
        for (int i = 0; i < (int)(sizeof(icons)/sizeof(icons[0])); i++) {
            int cx = kGapX + (i % kCols) * (kIconW + kGapX);
            int cy = kStartY + (i / kCols) * (kIconH + kGapY);
            auto* iv = new ImageView(icons[i]);
            iv->bounds().x = (int16_t)cx;
            iv->bounds().y = (int16_t)cy;
            root->addView(iv);
        }

    }

    void onResume() override {
        Activity::onResume();
        mWindow->rootView()->invalidate();
    }
};

/* VSYNC via TE rising edge (TE signal from LCD = display refresh) */
volatile int g_vsync;

/* ── TE interval measurement ──────────────────────────────────────────── */
/* Uses DWT cycle counter @ HCLK (240 MHz → 1 tick = 4.167 ns) */

#define TE_HISTORY   256           /* rolling window for average */
#define TE_PRINT_N   128           /* print stats every N TE interrupts */

static volatile uint32_t g_te_last_cyc;   /* DWT_CYCCNT at last TE */
static volatile uint32_t g_te_delta;      /* most recent interval (cycles) */
static volatile uint32_t g_te_sum;        /* sum of deltas in window */
static volatile uint32_t g_te_min;        /* min delta in window */
static volatile uint32_t g_te_max;        /* max delta in window */
static volatile uint32_t g_te_count;      /* total TE count (wraps) */
static volatile uint32_t g_te_window_n;   /* samples in current window */

static void te_isr(uint32_t pin, void *arg)
{
    (void)pin; (void)arg;
    g_vsync = 1;

    uint32_t now = DWT_CYCCNT;
    uint32_t last = g_te_last_cyc;
    g_te_last_cyc = now;

    if (last != 0U) {
        uint32_t delta = now - last;
        g_te_delta = delta;

        if (g_te_window_n == 0U) {
            g_te_sum = delta;
            g_te_min = delta;
            g_te_max = delta;
            g_te_window_n = 1U;
        } else if (g_te_window_n < TE_HISTORY) {
            g_te_sum += delta;
            if (delta < g_te_min) g_te_min = delta;
            if (delta > g_te_max) g_te_max = delta;
            g_te_window_n++;
        } else {
            /* rolling: subtract oldest approximation and add new */
            g_te_sum = g_te_sum - (g_te_sum / TE_HISTORY) + delta;
            if (delta < g_te_min) g_te_min = delta;
            if (delta > g_te_max) g_te_max = delta;
        }
    }
    g_te_count++;
}

static void vsync_wait(void)
{
    while (!g_vsync) { __asm volatile("wfi"); }
    g_vsync = 0;
}

/* Print TE frame-rate stats. Call periodically from main loop. */
static void te_heartbeat(void)
{
    static uint32_t last_print_count;

    if (g_te_count - last_print_count < TE_PRINT_N) return;
    last_print_count = g_te_count;

    uint32_t n    = g_te_window_n;
    uint32_t sum  = g_te_sum;
    uint32_t min  = g_te_min;
    uint32_t max  = g_te_max;
    uint32_t clk  = clk_get_hz();

    if (n == 0U) return;

    uint32_t avg_cyc = sum / n;
    /* Convert cycles → µs (× 10 for 1 decimal) */
    uint32_t avg_us_x10 = (uint32_t)((uint64_t)avg_cyc * 10000000ULL / clk);
    uint32_t min_us_x10 = (uint32_t)((uint64_t)min     * 10000000ULL / clk);
    uint32_t max_us_x10 = (uint32_t)((uint64_t)max     * 10000000ULL / clk);
    /* FPS × 10 via integer math — picolibc has no %f support */
    uint32_t fps_x10 = (uint32_t)((uint64_t)clk * 10ULL / avg_cyc);

    printf("[TE] %lu frames | avg %lu.%lu ms | min %lu.%lu max %lu.%lu | %lu.%lu fps\r\n",
           (unsigned long)g_te_count,
           (unsigned long)(avg_us_x10 / 10000U), (unsigned long)((avg_us_x10 / 1000U) % 10U),
           (unsigned long)(min_us_x10 / 10000U), (unsigned long)((min_us_x10 / 1000U) % 10U),
           (unsigned long)(max_us_x10 / 10000U), (unsigned long)((max_us_x10 / 1000U) % 10U),
           (unsigned long)(fps_x10 / 10U), (unsigned long)(fps_x10 % 10U));

    /* reset min/max for next window */
    g_te_min = ~0U;
    g_te_max = 0U;
}

extern "C" int main()
{
    printf("\r\n[litho] Gallery\r\n");
    clk_set_hz(HCLK_240MHZ);

    cache_enable();
    printf("[litho] I+D Cache + MPI2 prefetch ON\r\n");

    /* Select bus driver:
     *   lcd_bus_qspi       — bit-bang QSPI, no IRQ needed
     *   lcd_bus_qspi_lcdc  — LCDC hardware QSPI, async DMA, needs LCDC1_IRQ
     * To switch, also change bsp/CMakeLists.txt to compile the matching .c file.
     */
    lcd_set_bus(&lcd_bus_qspi);
    lcd_set_ic(&lcd_ic_co5300);
    lcd_set_geometry(LCD_WIDTH, LCD_HEIGHT);
    lcd_set_pins(LCD_RST, LCD_BL);
    lcd_init();

    /* Init TE stats (DWT_CYCCNT already enabled by SystemInit) */
    g_te_min = ~0U;
    g_te_last_cyc = 0U;

    /* TE VSYNC */
    pinMode(LCD_TE, INPUT);
    attachInterrupt(LCD_TE, te_isr, RISING, NULL);
    printf("[litho] VSYNC enabled on TE pin %d\r\n", LCD_TE);

    SF32Display display;
    display.init(kScreenW, kScreenH);
    SF32Input input;
    SF32Tick  tick;

    WindowManager wm(display, input, tick);
    wm.initPFB(390, 50, 2);

    ActivityManager am(wm);
    am.registerActivity<GalleryActivity>("Gallery");

    Intent i;
    i.target = "Gallery";
    am.startActivity(i);

    printf("[litho] running (vsync mode)\r\n");
    while (true) {
        vsync_wait();
        te_heartbeat();
        if (!wm.runOnce()) break;
    }
    return 0;
}
