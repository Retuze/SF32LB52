/**
 * @file res_loader.cpp
 * @brief Runtime resource bundle loader implementation.
 */

#include "res_loader.h"

#include <cstdio>
#include <cstdlib>

const uint8_t* g_res_bundle_data = nullptr;
size_t         g_res_bundle_size = 0;

namespace litho {

bool ResLoader::init(const char* path)
{
    // Already loaded?
    if (g_res_bundle_data) return true;

    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[sim] ResLoader: cannot open '%s'\n", path);
        return false;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) {
        fprintf(stderr, "[sim] ResLoader: empty or invalid file '%s'\n", path);
        fclose(f);
        return false;
    }

    uint8_t* buf = (uint8_t*)malloc((size_t)sz);
    if (!buf) {
        fprintf(stderr, "[sim] ResLoader: malloc(%ld) failed\n", sz);
        fclose(f);
        return false;
    }

    size_t nread = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    if (nread != (size_t)sz) {
        fprintf(stderr, "[sim] ResLoader: short read (%zu / %ld)\n", nread, sz);
        free(buf);
        return false;
    }

    g_res_bundle_data = buf;
    g_res_bundle_size = (size_t)sz;

    printf("[sim] ResLoader: loaded '%s' (%zu bytes)\n", path, g_res_bundle_size);
    return true;
}

void ResLoader::shutdown()
{
    free((void*)g_res_bundle_data);
    g_res_bundle_data = nullptr;
    g_res_bundle_size = 0;
}

} // namespace litho
