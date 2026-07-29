/**
 * @file main.cpp
 * @brief PC Simulator — LithoUI Gallery demo on native windowing.
 *
 * Runs the same GalleryActivity as firmware-litho (12 icons, 390×450),
 * but on the host PC using X11 (Linux) or GDI (Windows).
 *
 * Usage:
 *   simulator [path/to/res_images.bin]
 *
 * Build:
 *   cmake --preset simulator
 *   cmake --build build/simulator
 */

// ── 1. Host compatibility layer (MUST be #1) ────────────────────
// Defines DWT_CYCCNT before any LithoUI header includes it.
#include "hal_stub.h"

// ── 2. Resource loader ──────────────────────────────────────────
#include "res_loader.h"

// ── 3. LithoUI framework ────────────────────────────────────────
#include "core/litho_core.h"
#include "framework/view/view_group.hpp"
#include "framework/window/window_manager.hpp"
#include "framework/activity/activity_manager.hpp"
#include "framework/intent/intent.hpp"
#include "framework/widget/image.hpp"
#include "framework/widget/text.hpp"
#include "framework/widget/button.hpp"
#include "framework/widget/scroll.hpp"
#include "res_images.h"

// ── 4. Platform-specific port adapters ──────────────────────────
#include "host_input.hpp"
#include "host_tick.hpp"

// ── 5. Standard libs ────────────────────────────────────────────
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace litho;

// ═══════════════════════════════════════════════════════════════════
//  Gallery Activity — same as firmware-litho/main.cpp
// ═══════════════════════════════════════════════════════════════════

static constexpr int kScreenW = 390;
static constexpr int kScreenH = 450;

// Simple solid-color background view — makes transparency artifacts obvious
class ColorBg : public View {
public:
    explicit ColorBg(RGB565 c) : mColor(c) {
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

        // Background (renders first) — makes alpha transparency visible
        root->addView(new ColorBg(RGB565::fromRGB(40, 40, 80)));

        auto* title = new TextView("Hello 你好");
        title->setTextColor(RGB565::fromRGB(255, 220, 80));
        title->bounds().x = 16;
        title->bounds().y = 8;
        root->addView(title);

        auto* btn = new Button(RGB565::fromRGB(60, 120, 200), 100, 40);
        btn->setText("设置");
        btn->setTextColor(RGB565::White());
        btn->bounds().x = 270;
        btn->bounds().y = 6;
        btn->setOnClick([](void*) {
            printf("[btn] 设置 clicked\n");
        }, nullptr);
        root->addView(btn);

        auto* btnImg = new Button();
        btnImg->setBackgroundImage(IMG_A_MUSIC);
        btnImg->setText("音乐");
        btnImg->setTextColor(RGB565::White());
        btnImg->bounds().x = 16;
        btnImg->bounds().y = 48;
        btnImg->setOnClick([](void*) {
            printf("[btn] 音乐 clicked\n");
        }, nullptr);
        root->addView(btnImg);

        // ScrollView below header — fullscreen scroll would steal button hits.
        static constexpr int kScrollTop = 160;
        auto* scroll = new ScrollView();
        scroll->bounds() = {0, (int16_t)kScrollTop,
                            (int16_t)kScreenW, (int16_t)(kScreenH - kScrollTop)};
        root->addView(scroll);

        // Gallery: 12 icons, rows 1+4 are alpha (PAL_ALPHA_RLE), rows 2+3 opaque
        static constexpr int kCols  = 3, kIconW = 100, kIconH = 100;
        static constexpr int kGapX  = (kScreenW - kCols * kIconW) / (kCols + 1);
        static constexpr int kGapY  = 15, kStartY = 10;

        ImageId icons[] = {
            IMG_A_DIAL,     IMG_A_MESSAGES, IMG_A_MUSIC,
            IMG_SETTINGS,   IMG_CAMERA,     IMG_WEATHER,
            IMG_CALENDAR,   IMG_COMPASS,    IMG_SPORTS,
            IMG_A_CAMERA,   IMG_A_CALENDAR, IMG_A_COMPASS,
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

// ═══════════════════════════════════════════════════════════════════
//  entry point
// ═══════════════════════════════════════════════════════════════════

int main(int argc, char** argv)
{
    printf("[sim] Watch Simulator — LithoUI Gallery\n");

    // ── Parse args: [--dump <file.bmp>] [path/to/res_images.bin] ──
    // --dump renders a few frames headlessly, writes the 390×450
    // framebuffer to a BMP, and exits — no window interaction needed.
    const char* dumpPath = nullptr;
    const char* resPath  = "res_images.bin";
    bool resSet = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
            dumpPath = argv[++i];
        } else if (!resSet) {
            resPath = argv[i];
            resSet = true;
        }
    }

    // 1. Load resource bundle
    if (!ResLoader::init(resPath)) {
        fprintf(stderr, "[sim] FATAL: cannot load resource bundle.\n");
        fprintf(stderr, "[sim] Usage: %s [--dump out.bmp] [path/to/res_images.bin]\n",
                (argc > 0) ? argv[0] : "simulator");
        return 1;
    }

    // 2. Create platform display
#ifdef _WIN32
    GdiDisplay display;
#else
    X11Display display;
#endif
    if (!display.init(kScreenW, kScreenH)) {
        fprintf(stderr, "[sim] FATAL: failed to create display\n");
        ResLoader::shutdown();
        return 1;
    }

    // 3. Create adapters
    HostInput input(display);
    HostTick  tick;

    // 4. Run LithoUI
    WindowManager wm(display, input, tick);

    // PFB: 390px-wide tiles × 50px high, pool of 2
    // (screen is only 390px wide, so each "row" is 1 tile)
    wm.initPFB(390, 50, 2);

    ActivityManager am(wm);
    am.registerActivity<GalleryActivity>("Gallery");

    Intent intent;
    intent.target = "Gallery";
    am.startActivity(intent);

    // ── Headless dump mode ───────────────────────────────────────
    if (dumpPath) {
        // Render enough frames for every PFB tile to flush (9 tiles at
        // 390×50 cover the 450px height; a few extra frames for safety).
        for (int i = 0; i < 30; i++) {
            wm.invalidateAll();
            wm.runOnce();
        }
#ifdef _WIN32
        bool ok = display.saveBmp(dumpPath);
        printf("[sim] dump %s -> %s\n", ok ? "OK" : "FAILED", dumpPath);
        ResLoader::shutdown();
        return ok ? 0 : 1;
#else
        fprintf(stderr, "[sim] --dump not implemented on this backend\n");
        ResLoader::shutdown();
        return 1;
#endif
    }

    printf("[sim] Running... (ESC or close window to quit)\n");
    // Continuous render loop — match firmware pattern with invalidateAll + runOnce
    while (true) {
        wm.invalidateAll();
        if (!wm.runOnce()) break;
    }

    printf("[sim] Quit. Goodbye.\n");
    ResLoader::shutdown();
    return 0;
}
