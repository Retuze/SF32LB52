/**
 * @file main.cpp
 * @brief LithoUI SF32LB52 demo — icon grid, rendered via hardware LCDC QSPI.
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

/* ColorBox — simple solid color rectangle for testing */
class ColorBox : public View {
public:
    explicit ColorBox(RGB565 color) : mColor(color) {}

    void onDraw(Painter& p) override {
        p.fillRect(0, 0, mBounds.width, mBounds.height, mColor);
    }

private:
    RGB565 mColor;
};

class GalleryActivity : public Activity {
public:
    void onCreate(Bundle&) override {
        auto* root = new ViewGroup();
        root->bounds() = {0, 0, (int16_t)kScreenW, (int16_t)kScreenH};
        setContentView(root);

        // Simple color blocks instead of images - test rendering pipeline
        static const int kCols = 3, kBoxW = 100, kBoxH = 100;
        static const int kGapX = (kScreenW - kCols * kBoxW) / (kCols + 1);
        static const int kGapY = 15, kStartY = 40;

        RGB565 colors[] = {
            RGB565::fromRGB(255, 0, 0),    // red
            RGB565::fromRGB(0, 255, 0),    // green
            RGB565::fromRGB(0, 0, 255),    // blue
            RGB565::fromRGB(255, 255, 0),  // yellow
            RGB565::fromRGB(255, 0, 255),  // magenta
            RGB565::fromRGB(0, 255, 255),  // cyan
        };

        for (int i = 0; i < 6; i++) {
            int cx = kGapX + (i % kCols) * (kBoxW + kGapX);
            int cy = kStartY + (i / kCols) * (kBoxH + kGapY);

            auto* colorBox = new ColorBox(colors[i]);
            colorBox->bounds() = {(int16_t)cx, (int16_t)cy, (int16_t)kBoxW, (int16_t)kBoxH};
            root->addView(colorBox);
        }
    }

    void onResume() override {
        Activity::onResume();
        mWindow->rootView()->invalidate();
    }
};

extern "C" int main()
{
    printf("[litho] start\r\n");
    clk_set_hz(HCLK_240MHZ);
    cache_enable();

    SF32Input input;
    board_lcd_init();

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

    printf("[litho] loop start\r\n");
    while (true) {
        wm.invalidateAll();
        wm.runOnce();
    }

    return 0;
}
