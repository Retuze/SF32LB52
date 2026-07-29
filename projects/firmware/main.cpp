/**
 * @file main.cpp
 * @brief LithoUI SF32LB52 demo — gallery + transition effect lab.
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
#include "framework/activity/transition.hpp"
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
static constexpr uint16_t kTransMs = 450;

static TransitionSpec makeTrans(int id) {
    switch (id) {
    case 0:  return TransitionSpec::fade().setDuration(kTransMs);
    case 1:  return TransitionSpec::slideFromRight().setDuration(kTransMs);
    case 2:  return TransitionSpec::slideFromLeft().setDuration(kTransMs);
    case 3:  return TransitionSpec::slideFromTop().setDuration(kTransMs);
    case 4:  return TransitionSpec::slideFromBottom().setDuration(kTransMs);
    case 5:  return TransitionSpec::pushFromRight().setDuration(kTransMs);
    case 6:  return TransitionSpec::pushFromLeft().setDuration(kTransMs);
    case 7:  return TransitionSpec::pushFromTop().setDuration(kTransMs);
    case 8:  return TransitionSpec::pushFromBottom().setDuration(kTransMs);
    case 9:  return TransitionSpec::slideFromRight().withFade().setDuration(kTransMs);
    case 10: return TransitionSpec::slideFromBottom().withFade().setDuration(kTransMs);
    case 11: return TransitionSpec::pushFromRight().withFade().setDuration(kTransMs);
    case 12: return TransitionSpec::pushFromBottom().withFade().setDuration(kTransMs);
    default: return TransitionSpec::fade().setDuration(kTransMs);
    }
}

static const char* const kTransLabels[] = {
    "Fade",
    "Slide Right",
    "Slide Left",
    "Slide Top",
    "Slide Bottom",
    "Push Right",
    "Push Left",
    "Push Top",
    "Push Bottom",
    "SlideR + Fade",
    "SlideB + Fade",
    "PushR + Fade",
    "PushB + Fade",
};
static constexpr int kTransCount = (int)(sizeof(kTransLabels) / sizeof(kTransLabels[0]));

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

class TransDemoActivity : public Activity {
public:
    void onCreate(Bundle& state) override {
        mId = state.getInt("ti", 0);
        if (mId < 0 || mId >= kTransCount) mId = 0;

        auto* root = new ViewGroup();
        root->bounds() = {0, 0, (int16_t)kScreenW, (int16_t)kScreenH};
        setContentView(root);

        root->addView(new ColorBg(RGB565::fromRGB(20, 90, 70)));

        auto* title = new TextView(kTransLabels[mId]);
        title->setTextColor(RGB565::fromRGB(255, 230, 120));
        title->bounds().x = 16;
        title->bounds().y = 16;
        root->addView(title);

        auto* hint = new TextView("Demo page");
        hint->setTextColor(RGB565::fromRGB(180, 220, 200));
        hint->bounds().x = 16;
        hint->bounds().y = 56;
        root->addView(hint);

        ImageId icons[] = { IMG_A_MUSIC, IMG_SETTINGS, IMG_CAMERA, IMG_WEATHER };
        for (int i = 0; i < 4; i++) {
            auto* iv = new ImageView(icons[i]);
            iv->bounds().x = (int16_t)(20 + (i % 2) * 120);
            iv->bounds().y = (int16_t)(110 + (i / 2) * 120);
            root->addView(iv);
        }

        auto* back = new Button(RGB565::fromRGB(80, 80, 100), 120, 44);
        back->setText("Back");
        back->setTextColor(RGB565::White());
        back->bounds().x = 16;
        back->bounds().y = (int16_t)(kScreenH - 60);
        back->setOnClick([](void* u) {
            auto* self = (TransDemoActivity*)u;
            self->finish(makeTrans(self->mId));
        }, this);
        root->addView(back);
    }

private:
    int mId = 0;
};

class TransLabActivity : public Activity {
public:
    void onCreate(Bundle&) override {
        auto* root = new ViewGroup();
        root->bounds() = {0, 0, (int16_t)kScreenW, (int16_t)kScreenH};
        setContentView(root);

        root->addView(new ColorBg(RGB565::fromRGB(35, 40, 55)));

        auto* title = new TextView("Transitions");
        title->setTextColor(RGB565::White());
        title->bounds().x = 16;
        title->bounds().y = 12;
        root->addView(title);

        auto* back = new Button(RGB565::fromRGB(80, 80, 100), 80, 36);
        back->setText("Back");
        back->setTextColor(RGB565::White());
        back->bounds().x = (int16_t)(kScreenW - 96);
        back->bounds().y = 8;
        back->setOnClick([](void* u) {
            ((TransLabActivity*)u)->finish(TransitionSpec::slideFromRight().setDuration(kTransMs));
        }, this);
        root->addView(back);

        auto* scroll = new ScrollView();
        scroll->bounds() = {0, 56, (int16_t)kScreenW, (int16_t)(kScreenH - 56)};
        root->addView(scroll);

        static const int kBtnH = 44;
        static const int kGap  = 8;
        for (int i = 0; i < kTransCount; i++) {
            mClicks[i].self = this;
            mClicks[i].id   = i;

            auto* btn = new Button(RGB565::fromRGB(55, 95, 160), kScreenW - 32, kBtnH);
            btn->setText(kTransLabels[i]);
            btn->setTextColor(RGB565::White());
            btn->bounds().x = 16;
            btn->bounds().y = (int16_t)(8 + i * (kBtnH + kGap));
            btn->setOnClick([](void* u) {
                auto* ctx = (ClickCtx*)u;
                Intent intent;
                intent.target = "TransDemo";
                intent.putInt("ti", ctx->id);
                intent.putString("name", kTransLabels[ctx->id]);
                ctx->self->startActivity(intent, makeTrans(ctx->id));
            }, &mClicks[i]);
            scroll->addView(btn);
        }
    }

private:
    struct ClickCtx { TransLabActivity* self; int id; };
    ClickCtx mClicks[kTransCount] = {};
};

class GalleryActivity : public Activity {
public:
    void onCreate(Bundle&) override {
        auto* root = new ViewGroup();
        root->bounds() = {0, 0, (int16_t)kScreenW, (int16_t)kScreenH};
        setContentView(root);

        root->addView(new ColorBg(RGB565::fromRGB(40, 40, 80)));

        auto* title = new TextView("Hello 你好");
        title->setTextColor(RGB565::fromRGB(255, 220, 80));
        title->bounds().x = 16;
        title->bounds().y = 8;
        root->addView(title);

        auto* btn = new Button(RGB565::fromRGB(60, 120, 200), 120, 40);
        btn->setText("Trans");
        btn->setTextColor(RGB565::White());
        btn->bounds().x = 250;
        btn->bounds().y = 6;
        btn->setOnClick([](void* u) {
            auto* self = (GalleryActivity*)u;
            Intent intent;
            intent.target = "TransLab";
            self->startActivity(intent, TransitionSpec::slideFromRight().setDuration(kTransMs));
        }, this);
        root->addView(btn);

        static const int kScrollTop = 60;
        auto* scroll = new ScrollView();
        scroll->bounds() = {0, (int16_t)kScrollTop,
                            (int16_t)kScreenW, (int16_t)(kScreenH - kScrollTop)};
        root->addView(scroll);

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
        if (mWindow && mWindow->rootView()) mWindow->rootView()->invalidate();
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
    am.registerActivity<TransLabActivity>("TransLab");
    am.registerActivity<TransDemoActivity>("TransDemo");
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
