/**
 * @file main.cpp
 * @brief LithoUI SF32LB52 demo — icon grid, rendered via hardware LCDC QSPI.
 *
 * Panel is brought up over the bit-bang bus (proven init path), then the
 * pixel transport is handed to the LCDC peripheral (async DMA).  The PFB
 * tile pipeline (pool=2) draws tile N+1 while LCDC transmits tile N.
 */

extern "C" {
#include "hal.h"
#include "board.h"
#include "lcd.h"
#include "lcd_lcdc_co5300.h"
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

        // Background: 360x360 cartoon (PAL8 raw, opaque)
        auto* bg = new ImageView(IMG_03);
        bg->bounds().x = (int16_t)((kScreenW - 360) / 2);
        bg->bounds().y = (int16_t)((kScreenH - 360) / 2);
        root->addView(bg);

        // 12 alpha icons overlaid in 3×4 grid
        static const int kCols = 3, kIconW = 100, kIconH = 100;
        static const int kGapX = (kScreenW - kCols * kIconW) / (kCols + 1);
        static const int kGapY = 15, kStartY = 40;

        ImageId alphaIcons[] = {
            IMG_A_DIAL, IMG_A_MESSAGES, IMG_A_MUSIC,
            IMG_A_SETTINGS, IMG_A_CAMERA, IMG_A_WEATHER,
            IMG_A_CALENDAR, IMG_A_COMPASS, IMG_A_SPORTS,
            IMG_A_SLEEP, IMG_A_ALARM, IMG_A_STOPWATCH,
        };
        for (int i = 0; i < (int)(sizeof(alphaIcons)/sizeof(alphaIcons[0])); i++) {
            int cx = kGapX + (i % kCols) * (kIconW + kGapX);
            int cy = kStartY + (i / kCols) * (kIconH + kGapY);
            auto* iv = new ImageView(alphaIcons[i]);
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

extern "C" int main()
{
    printf("\r\n[litho] Gallery (LCDC)\r\n");
    clk_set_hz(HCLK_240MHZ);

    cache_enable();
    printf("[litho] I+D Cache + MPI2 prefetch ON\r\n");

    /* Panel init via bit-bang (proven), then hand the pixel path to LCDC. */
    lcd_set_bus(&lcd_bus_qspi);
    lcd_set_ic(&lcd_ic_co5300);
    lcd_set_geometry(LCD_WIDTH, LCD_HEIGHT);
    lcd_set_pins(LCD_RST, LCD_BL);
    lcd_init();
    lcd_fill_color(0x0000);          /* clear via bit-bang before handoff */
    lcdc_activate_pixel();           /* pins → LCDC, peripheral up (async DMA) */
    printf("[litho] LCDC pixel path active\r\n");

    /* Gallery via LithoUI PFB pipeline — tiles transferred by LCDC DMA. */
    SF32Display display;
    display.init(kScreenW, kScreenH);
    SF32Input input;
    SF32Tick  tick;
    WindowManager wm(display, input, tick);
    wm.initPFB(390, 50, 2);
    ActivityManager am(wm);
    am.registerActivity<GalleryActivity>("Gallery");
    Intent intent;
    intent.target = "Gallery";
    am.startActivity(intent);

    /* Guaranteed first frame, independent of the benchmark loop. */
    wm.invalidateAll();
    wm.runOnce();
    printf("[litho] first frame via LCDC\r\n");

    /* Free-running full-redraw loop; report whole-frame FPS via DWT. */
    uint32_t frames = 0;
    uint32_t t0 = DWT_CYCCNT;
    while (true) {
        wm.invalidateAll();
        if (!wm.runOnce()) break;

        if (++frames >= 100U) {
            uint32_t dt  = DWT_CYCCNT - t0;
            uint32_t clk = clk_get_hz();
            /* FPS × 10 via integer math — picolibc has no %f support */
            uint32_t fps_x10 = (uint32_t)((uint64_t)frames * clk * 10ULL / dt);
            printf("[litho] %lu frames | %lu.%lu fps (LCDC full redraw)\r\n",
                   (unsigned long)frames,
                   (unsigned long)(fps_x10 / 10U),
                   (unsigned long)(fps_x10 % 10U));
            frames = 0;
            t0 = DWT_CYCCNT;
        }
    }

    while (1) { __asm volatile("wfi"); }
    return 0;
}
