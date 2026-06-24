/**
 * @file main.cpp
 * @brief LithoUI SF32LB52 demo — icon grid with real images.
 */

extern "C" {
#include "hal.h"
#include "board.h"
#include "lcd_ref.h"
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

static const int kScreenW = LCD_WIDTH_REF;
static const int kScreenH = LCD_HEIGHT_REF;

// ---- UART helpers ----
static void uart_putc(char c) {
    while ((USART1->ISR & (1UL << 7)) == 0U) {}
    USART1->TDR = (uint8_t)c;
}
static void uart_puts(const char* s) {
    while (*s) uart_putc(*s++);
    while ((USART1->ISR & (1UL << 6)) == 0U) {}
}

// ---- Background view ----
class BgView : public View {
public:
    void onDraw(Painter& p) override {
        p.fillRect(0, 0, mBounds.width, mBounds.height, RGB565::fromRGB(18, 20, 26));
    }
};

// ---- Icon grid activity ----
class GalleryActivity : public Activity {
public:
    void onCreate(Bundle&) override {
        auto* root = new ViewGroup();
        root->bounds() = {0, 0, (int16_t)kScreenW, (int16_t)kScreenH};
        setContentView(root);

        auto* bg = new BgView();
        bg->bounds() = {0, 0, (int16_t)kScreenW, (int16_t)kScreenH};
        root->addView(bg);

        // Title bar
        root->addView(makeTitle());

        // Icon grid: 3 columns, row height 120
        static const int kCols = 3;
        static const int kIconW = 90, kIconH = 90;
        static const int kGapX = (kScreenW - kCols * kIconW) / (kCols + 1);
        static const int kGapY = 20;
        static const int kStartY = 40;

        ImageId icons[] = {
            IMG_A_DIAL,      IMG_A_MESSAGES,   IMG_A_MUSIC,
            IMG_A_SETTINGS,  IMG_A_CAMERA,     IMG_A_WEATHER,
            IMG_A_CALENDAR,  IMG_A_COMPASS,    IMG_A_SPORTS,
            IMG_A_SLEEP,     IMG_A_ALARM,      IMG_A_STOPWATCH,
        };
        int count = sizeof(icons) / sizeof(icons[0]);

        for (int i = 0; i < count; i++) {
            int col = i % kCols;
            int row = i / kCols;
            int cx = kGapX + col * (kIconW + kGapX);
            int cy = kStartY + row * (kIconH + kGapY);

            auto* iv = new ImageView(kIconW, kIconH);
            iv->bounds() = {(int16_t)cx, (int16_t)cy, (int16_t)kIconW, (int16_t)kIconH};
            iv->setImageId(icons[i]);
            root->addView(iv);
        }
    }

    void onResume() override {
        Activity::onResume();
        mWindow->rootView()->invalidate();
    }

private:
    View* makeTitle() {
        auto* v = new View();
        v->bounds() = {0, 0, (int16_t)kScreenW, 40};
        // title drawn in onDraw
        return v;  // placeholder, background handles the look
    }
};

// ---- Entry point ----
extern "C" int main()
{
    uart_puts("\r\n[litho] Gallery demo\r\n");
    rcc_set_system_hz(240000000UL);
    lcd_ref_init();

    SF32Display display;
    display.init(kScreenW, kScreenH);
    SF32Input input;
    SF32Tick  tick;

    WindowManager wm(display, input, tick);
    wm.initPFB(128, 128, 2);

    ActivityManager am(wm);
    am.registerActivity<GalleryActivity>("Gallery");

    Intent i;
    i.target = "Gallery";
    am.startActivity(i);

    uart_puts("[litho] running\r\n");
    wm.run();
    return 0;
}
