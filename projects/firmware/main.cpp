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

/* ColorBg — full-screen solid background. A non-black fill makes alpha
 * transparency and any semi-transparent artifacts obvious. */
class ColorBg : public View {
public:
    explicit ColorBg(RGB565 color) : mColor(color) {
        mBounds = {0, 0, (int16_t)kScreenW, (int16_t)kScreenH};
    }
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

        // Background renders first — makes alpha transparency visible so any
        // semi-transparent horizontal banding stands out against solid color.
        root->addView(new ColorBg(RGB565::fromRGB(40, 40, 80)));

        // Gallery: 12 icons. Rows 1 & 4 are alpha (PAL_ALPHA_RLE / RGB565A_RLE),
        // rows 2 & 3 opaque. A_MUSIC/A_WEATHER exercise the fixed alpha encoder.
        static const int kCols = 3, kIconW = 100, kIconH = 100;
        static const int kGapX = (kScreenW - kCols * kIconW) / (kCols + 1);
        static const int kGapY = 15, kStartY = 40;

        ImageId icons[] = {
            IMG_A_DIAL,     IMG_A_MESSAGES, IMG_A_MUSIC,
            IMG_SETTINGS,   IMG_CAMERA,     IMG_WEATHER,
            IMG_CALENDAR,   IMG_COMPASS,    IMG_SPORTS,
            IMG_A_CAMERA,   IMG_A_CALENDAR, IMG_A_WEATHER,
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
