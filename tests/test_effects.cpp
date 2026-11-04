// Konsistensi katalog efek: indeks uP dalam shader sesuai jumlah parameter, nilai bawaan wajar.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

#include "effects.h"

using namespace studio;
static int g_checks = 0, g_fail = 0;
#define CHECK(c, ...)                                                    \
    do {                                                                 \
        ++g_checks;                                                      \
        if (!(c)) {                                                      \
            ++g_fail;                                                    \
            std::printf("GAGAL %s:%d  %s  ", __FILE__, __LINE__, #c);    \
            std::printf(__VA_ARGS__);                                    \
            std::printf("\n");                                           \
        }                                                                \
    } while (0)

int main() {
    std::set<std::string> types;
    const auto& cat = effectCatalog();
    CHECK(cat.size() >= 25, "katalog terlalu kecil: %zu", cat.size());
    for (const EffectDef& d : cat) {
        CHECK(types.insert(d.type).second, "tipe ganda %s", d.type);
        CHECK(d.passes >= 1 && d.passes <= 4, "%s passes", d.type);
        const int flat = d.flatCount();
        CHECK(flat <= 32, "%s: %d parameter > 32", d.type, flat);
        CHECK(static_cast<int>(defaultParams(d).size()) == flat, "%s default size", d.type);
        for (const char* c = d.type; *c; ++c) CHECK((*c >= 'a' && *c <= 'z') || *c == '_' || (*c >= '0' && *c <= '9'), "%s karakter tipe", d.type);

        // Setiap uP[N] di body harus N < flat; dan setiap komponen dipakai setidaknya oleh sebuah uP
        std::string body = d.body;
        std::set<int> used;
        size_t pos = 0;
        while ((pos = body.find("uP[", pos)) != std::string::npos) {
            pos += 3;
            int n = std::atoi(body.c_str() + pos);
            used.insert(n);
            CHECK(n >= 0 && n < flat, "%s: uP[%d] di luar %d parameter", d.type, n, flat);
        }
        std::set<int> alphaComps;  // alfa warna boleh tidak dipakai
        { int o = 0; for (const ParamDef& p : d.params) { if (p.kind == ParamKind::Color) alphaComps.insert(o + 3); o += p.comps; } }
        for (int i = 0; i < flat; ++i) CHECK(used.count(i) || alphaComps.count(i), "%s: parameter %d tidak dipakai shader", d.type, i);
        CHECK(body.find("vec4 effect(vec2 uv)") != std::string::npos, "%s tanpa effect()", d.type);
        CHECK(body.find("uPass") != std::string::npos || d.passes == 1, "%s multipass tanpa uPass", d.type);

        for (const ParamDef& p : d.params) {
            for (int i = 0; i < p.comps; ++i) CHECK(p.def[i] >= p.min - 1e-6f && p.def[i] <= p.max + 1e-6f, "%s.%s bawaan di luar batas", d.type, p.id);
            CHECK(p.min < p.max, "%s.%s min<max", d.type, p.id);
            int want = (p.kind == ParamKind::Point) ? 2 : (p.kind == ParamKind::Color) ? 4 : 1;
            CHECK(p.comps == want, "%s.%s comps", d.type, p.id);
        }
        CHECK(findEffectDef(d.type) == &d, "find %s", d.type);
        CHECK(effectShaderSource(d).find("#version 300 es") == 0, "%s header", d.type);
    }
    CHECK(findEffectDef("tidak_ada") == nullptr, "find null");
    CHECK(describeCatalog().size() > 500, "describe");
    std::printf("effects: %zu efek, %d pemeriksaan, %d gagal\n", cat.size(), g_checks, g_fail);
    return g_fail ? 1 : 0;
}
