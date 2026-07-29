/**
 * @file res_loader.h
 * @brief Runtime resource bundle loader for PC simulator.
 *
 * On ARM firmware, res_images.bin is embedded via .incbin (res_embed.S).
 * On the PC simulator, we load it from a file at startup.
 *
 * This header defines RES_IMAGE_BUNDLE to point to the loaded buffer.
 * res_images.h (auto-generated) guards its own default definition with
 * #ifndef RES_IMAGE_BUNDLE, so our override takes precedence.
 *
 * Include this BEFORE res_images.h.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

// Override the resource bundle pointer BEFORE res_images.h is included.
// res_images.h has:  #ifndef RES_IMAGE_BUNDLE
//                      #define RES_IMAGE_BUNDLE  _binary_res_images_bin_start
//                    #endif
// Our definition is used instead — no .incbin symbols needed.
#define RES_IMAGE_BUNDLE  g_res_bundle_data

#ifdef __cplusplus
extern "C" {
#endif

// Points to the loaded res_images.bin buffer.
// Set by ResLoader::init(), freed by ResLoader::shutdown().
extern const uint8_t* g_res_bundle_data;
extern size_t         g_res_bundle_size;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace litho {

class ResLoader {
public:
    /// Load the resource bundle from a file path.
    /// Returns true on success, false on failure (prints error to stderr).
    static bool init(const char* path);

    /// Free the loaded bundle. Safe to call multiple times.
    static void shutdown();
};

} // namespace litho
#endif
