#include <cstdio>
#include <vector>

#include "export.h"

using namespace studio;
static int g_fail = 0, g_checks = 0;
#define CHECK(c) do { ++g_checks; if (!(c)) { ++g_fail; std::fprintf(stderr, "GAGAL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static std::vector<uint8_t> solid(int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4);
    for (size_t i = 0; i < px.size(); i += 4) { px[i] = r; px[i + 1] = g; px[i + 2] = b; px[i + 3] = 255; }
    return px;
}
static bool near(int a, int b, int e) { return a - b <= e && b - a <= e; }

int main() {
    std::vector<uint8_t> out;
    auto k = solid(8, 4, 0, 0, 0);
    CHECK(rgbaToNV12(k.data(), 8, 4, &out) && out.size() == 8 * 4 * 3 / 2);
    CHECK(out[0] == 16 && out[32] == 128 && out[33] == 128);       // hitam: Y=16, UV=128
    auto wht = solid(8, 4, 255, 255, 255);
    CHECK(rgbaToNV12(wht.data(), 8, 4, &out));
    CHECK(near(out[0], 235, 1) && near(out[32], 128, 1) && near(out[33], 128, 1));
    auto red = solid(8, 4, 255, 0, 0);
    CHECK(rgbaToNV12(red.data(), 8, 4, &out));
    CHECK(near(out[0], 63, 2) && near(out[32], 102, 2) && near(out[33], 240, 2));   // BT.709 merah
    auto blu = solid(8, 4, 0, 0, 255);
    CHECK(rgbaToNV12(blu.data(), 8, 4, &out));
    CHECK(near(out[0], 32, 2) && near(out[32], 240, 2) && near(out[33], 118, 2));
    CHECK(!rgbaToNV12(k.data(), 7, 4, &out));      // ganjil ditolak
    CHECK(!rgbaToNV12(nullptr, 8, 4, &out));
    CHECK(!rgbaToNV12(k.data(), 0, 4, &out));
    std::printf("export: %d pemeriksaan, %d gagal\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
