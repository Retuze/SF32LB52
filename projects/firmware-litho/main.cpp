/**
 * @file main.cpp
 * @brief LithoUI SF32LB52 demo - icon grid over hardware LCDC.
 */

extern "C" {
#include "hal.h"
#include "board.h"
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

        static const int kCols = 3, kIconW = 100, kIconH = 100;
        static const int kGapX = (kScreenW - kCols * kIconW) / (kCols + 1);
        static const int kGapY = 15, kStartY = 40;

        ImageId icons[] = {
            IMG_DIAL, IMG_MESSAGES, IMG_MUSIC,
            IMG_SETTINGS, IMG_CAMERA, IMG_WEATHER,
            IMG_CALENDAR, IMG_COMPASS, IMG_SPORTS,
            IMG_SLEEP, IMG_ALARM, IMG_STOPWATCH,
        };
        for (int i = 0; i < (int)(sizeof(icons) / sizeof(icons[0])); i++) {
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

extern "C" {
void enable_flash_cache_prefetch(void);
}

extern "C" int main()
{
    printf("\r\n[litho] Gallery LCDC\r\n");
    rcc_set_system_hz(240000000UL);

    enable_flash_cache_prefetch();
    printf("[litho] I+D Cache + MPI2 prefetch ON\r\n");

    lcd_lcdc_co5300_init();

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

    printf("[litho] running\r\n");
    wm.run();
    return 0;
}
