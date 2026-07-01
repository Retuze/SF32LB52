/**
 * @file main.cpp
 * @brief LithoUI SF32LB52 demo — icon grid, rendered via hardware LCDC QSPI.
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

/* ScrollableRoot — intercept touch to scroll icon container vertically. */
class ScrollableRoot : public ViewGroup {
public:
    void setScrollTarget(ViewGroup* t) { mTarget = t; }

    bool dispatchTouchEvent(TouchEvent& ev, int sx, int sy) override {
        if (!mTarget) return false;

        if (ev.action == TouchAction::DOWN) {
            mLastY = ev.y;
            mTracking = true;
            ev.handler   = this;
            ev.handlerSX = sx;
            ev.handlerSY = sy;
            return true;
        }
        if (ev.action == TouchAction::MOVE && mTracking) {
            int dy = ev.y - mLastY;
            mLastY  = ev.y;
            mScroll += dy;
            mTarget->setTranslationY((int16_t)mScroll);
            return true;
        }
        if (ev.action == TouchAction::UP) {
            mTracking = false;
            return true;
        }
        return false;
    }

private:
    ViewGroup* mTarget = nullptr;
    int mLastY  = 0;
    int mScroll = 0;
    bool mTracking = false;
};

class GalleryActivity : public Activity {
public:
    void onCreate(Bundle&) override {
        auto* root = new ScrollableRoot();
        root->bounds() = {0, 0, (int16_t)kScreenW, (int16_t)kScreenH};
        setContentView(root);

        static const int kCols = 3, kIconW = 100, kIconH = 100;
        static const int kGapX = (kScreenW - kCols * kIconW) / (kCols + 1);
        static const int kGapY = 15, kStartY = 40;
        static const int kBoxH = kStartY + 4 * (kIconH + kGapY) + 100;

        auto* iconBox = new ViewGroup();
        iconBox->bounds() = {0, 0, (int16_t)kScreenW, (int16_t)kBoxH};
        root->addView(iconBox);
        root->setScrollTarget(iconBox);

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
            iconBox->addView(iv);
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

    SF32Input input;  /* touch init before LCDC — avoids pinmux conflict */

    lcd_set_bus(&lcd_bus_qspi);
    lcd_set_ic(&lcd_ic_co5300);
    lcd_set_geometry(LCD_WIDTH, LCD_HEIGHT);
    lcd_set_pins(LCD_RST, LCD_BL);
    lcd_init();
    lcd_fill_color(0x0000);
    lcdc_activate_pixel();
    printf("[litho] LCDC pixel path active\r\n");

    SF32Display display;
    display.init(kScreenW, kScreenH);
    SF32Tick  tick;
    WindowManager wm(display, input, tick);
    wm.initPFB(390, 50, 2);

    ActivityManager am(wm);
    am.registerActivity<GalleryActivity>("Gallery");
    Intent intent;
    intent.target = "Gallery";
    am.startActivity(intent);

    while (true) {
        wm.invalidateAll();
        wm.runOnce();
    }

    return 0;
}
