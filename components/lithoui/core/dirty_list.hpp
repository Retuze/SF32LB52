#pragma once
#include "region.hpp"
#include <stdint.h>

namespace litho {

// Dirty region list — collects screen regions that need redraw.
// markDirty() maintains the invariant that all regions are non-overlapping.

class DirtyList {
public:
    static constexpr int kMaxRegions = 16;

    void markDirty(const Region& r) {
        if (r.isEmpty()) return;

        // Merge r with all existing regions it overlaps.
        // Write surviving (non-overlapping) regions to the front of the array.
        Region merged = r;
        int    write  = 0;

        for (int i = 0; i < mCount; i++) {
            if (overlaps(mRegions[i], merged)) {
                mergeInto(merged, mRegions[i]);
            } else {
                mRegions[write++] = mRegions[i];
            }
        }

        if (write < kMaxRegions) {
            mRegions[write++] = merged;
            mCount = write;
        } else {
            // overflow — collapse everything into one bounding rect
            for (int i = write; i < mCount; i++) {
                mergeInto(merged, mRegions[i]);
            }
            mRegions[0] = merged;
            mCount = 1;
        }
    }

    const Region* regions() const { return mRegions; }
    int           count()   const { return mCount; }
    void          clear()          { mCount = 0; }

private:
    static bool overlaps(const Region& a, const Region& b) {
        int aR = a.x + a.width,  aB = a.y + a.height;
        int bR = b.x + b.width,  bB = b.y + b.height;
        return a.x < bR && aR >= b.x && a.y < bB && aB >= b.y;
    }

    static void mergeInto(Region& dst, const Region& src) {
        int dstR = dst.x + dst.width,  dstB = dst.y + dst.height;
        int srcR = src.x + src.width,  srcB = src.y + src.height;
        if (src.x  < dst.x) dst.x = src.x;
        if (src.y  < dst.y) dst.y = src.y;
        if (srcR > dstR)   dstR = srcR;
        if (srcB > dstB)   dstB = srcB;
        dst.width  = dstR - dst.x;
        dst.height = dstB - dst.y;
    }

    Region   mRegions[kMaxRegions];
    uint16_t mCount = 0;
};

} // namespace litho
