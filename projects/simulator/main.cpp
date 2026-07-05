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
#include "framework/widget/image_view.hpp"
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

class GalleryActivity : public Activity {
public:
    void onCreate(Bundle&) override {
        auto* root = new ViewGroup();
        root->bounds() = {0, 0, (int16_t)kScreenW, (int16_t)kScreenH};
        setContentView(root);

        static constexpr int kCols  = 3;
        static constexpr int kIconW = 100;
        static constexpr int kIconH = 100;
        static constexpr int kGapX  = (kScreenW - kCols * kIconW) / (kCols + 1);
        static constexpr int kGapY  = 15;
        static constexpr int kStartY = 40;

        ImageId icons[] = {
            IMG_DIAL,     IMG_MESSAGES, IMG_MUSIC,
            IMG_SETTINGS, IMG_CAMERA,   IMG_WEATHER,
            IMG_CALENDAR, IMG_COMPASS,  IMG_SPORTS,
            IMG_SLEEP,    IMG_ALARM,    IMG_STOPWATCH,
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

// ═══════════════════════════════════════════════════════════════════
//  entry point
// ═══════════════════════════════════════════════════════════════════

int main(int argc, char** argv)
{
    printf("[sim] Watch Simulator — LithoUI Gallery\n");

    // 1. Load resource bundle
    const char* resPath = (argc > 1) ? argv[1] : "res_images.bin";
    if (!ResLoader::init(resPath)) {
        fprintf(stderr, "[sim] FATAL: cannot load resource bundle.\n");
        fprintf(stderr, "[sim] Usage: %s [path/to/res_images.bin]\n",
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

    printf("[sim] Running... (ESC or close window to quit)\n");
    wm.run();

    printf("[sim] Quit. Goodbye.\n");
    ResLoader::shutdown();
    return 0;
}
