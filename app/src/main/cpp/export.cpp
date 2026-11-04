#include "export.h"

#include <algorithm>

namespace studio {

namespace {
inline uint8_t clamp8(int v) { return static_cast<uint8_t>(std::clamp(v, 0, 255)); }
}  // namespace

bool rgbaToNV12(const uint8_t* rgba, int w, int h, std::vector<uint8_t>* out) {
    if (!rgba || !out || w <= 0 || h <= 0 || (w & 1) || (h & 1) || w > 8192 || h > 8192) return false;
    const size_t ySize = static_cast<size_t>(w) * h;
    out->assign(ySize + ySize / 2, 0);
    uint8_t* Y = out->data();
    uint8_t* UV = out->data() + ySize;
    // Koefisien BT.709 limited, aritmetika bilangan bulat 8 bit.
    for (int y = 0; y < h; ++y) {
        const uint8_t* row = rgba + static_cast<size_t>(y) * w * 4;
        for (int x = 0; x < w; ++x) {
            const int r = row[x * 4], g = row[x * 4 + 1], b = row[x * 4 + 2];
            Y[static_cast<size_t>(y) * w + x] = clamp8(16 + ((47 * r + 157 * g + 16 * b + 128) >> 8));
        }
    }
    for (int y = 0; y < h; y += 2) {
        for (int x = 0; x < w; x += 2) {
            int r = 0, g = 0, b = 0;
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    const uint8_t* p = rgba + (static_cast<size_t>(y + dy) * w + (x + dx)) * 4;
                    r += p[0]; g += p[1]; b += p[2];
                }
            r = (r + 2) / 4; g = (g + 2) / 4; b = (b + 2) / 4;
            const int u = 128 + ((-26 * r - 87 * g + 112 * b + 128) >> 8);
            const int v = 128 + ((112 * r - 102 * g - 10 * b + 128) >> 8);
            const size_t o = static_cast<size_t>(y / 2) * w + x;
            UV[o] = clamp8(u);
            UV[o + 1] = clamp8(v);
        }
    }
    return true;
}

}  // namespace studio
