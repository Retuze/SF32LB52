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
#include "framework/widget/image.hpp"
#include "framework/widget/text.hpp"
#include "framework/widget/button.hpp"
#include "framework/widget/scroll.hpp"
#include "res_images.h"

#include "port/sf32lb52/sf32_display.hpp"
#include "port/sf32lb52/sf32_input.hpp"
#include "port/sf32lb52/sf32_tick.hpp"

using namespace litho;

static const int kScreenW = LCD_WIDTH;
static const int kScreenH = LCD_HEIGHT;

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

        // Text demo — charset-packed 32px sans (Hello 你好)
        auto* title = new TextView("Hello 你好");
        title->setTextColor(RGB565::fromRGB(255, 220, 80));
        title->bounds().x = 16;
        title->bounds().y = 8;
        root->addView(title);

        // Button demo: solid color + centered label
        auto* btn = new Button(RGB565::fromRGB(60, 120, 200), 100, 40);
        btn->setText("设置");
        btn->setTextColor(RGB565::White());
        btn->bounds().x = 270;
        btn->bounds().y = 6;
        btn->setOnClick([](void*) {
            printf("[btn] 设置 clicked\r\n");
        }, nullptr);
        root->addView(btn);

        // Image + label button (icon-sized)
        auto* btnImg = new Button();
        btnImg->setBackgroundImage(IMG_A_MUSIC);
        btnImg->setText("音乐");
        btnImg->setTextColor(RGB565::White());
        btnImg->bounds().x = 16;
        btnImg->bounds().y = 48;
        btnImg->setOnClick([](void*) {
            printf("[btn] 音乐 clicked\r\n");
        }, nullptr);
        root->addView(btnImg);

        // ScrollView below the header buttons — must NOT cover them or it
        // steals all touches (hit-test is topmost-first).
        static const int kScrollTop = 160;
        auto* scroll = new ScrollView();
        scroll->bounds() = {0, (int16_t)kScrollTop,
                            (int16_t)kScreenW, (int16_t)(kScreenH - kScrollTop)};
        root->addView(scroll);

        // Gallery: 12 icons. Rows 1 & 4 are alpha (PAL_ALPHA_RLE / RGB565A_RLE),
        // rows 2 & 3 opaque. A_MUSIC/A_WEATHER exercise the fixed alpha encoder.
        static const int kCols = 3, kIconW = 100, kIconH = 100;
        static const int kGapX = (kScreenW - kCols * kIconW) / (kCols + 1);
        static const int kGapY = 15, kStartY = 10;

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
            scroll->addView(iv);
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
