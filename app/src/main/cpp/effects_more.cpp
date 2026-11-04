// Efek tambahan (katalog bagian 2). Format sama dengan effects.cpp: GLSL ES 3.00 + daftar parameter.
// Semua efek satu lintasan kecuali disebutkan; maksimal 32 float parameter per efek.
#include "effects.h"

namespace studio {
namespace {
ParamDef F(const char* id, const char* l, float d, float mn, float mx) {
    return {id, l, ParamKind::Float, 1, {d, 0.f, 0.f, 0.f}, mn, mx};
}
ParamDef I(const char* id, const char* l, float d, float mn, float mx) {
    return {id, l, ParamKind::Int, 1, {d, 0.f, 0.f, 0.f}, mn, mx};
}
ParamDef A(const char* id, const char* l, float d) {
    return {id, l, ParamKind::Angle, 1, {d, 0.f, 0.f, 0.f}, -180.f, 180.f};
}
ParamDef A360(const char* id, const char* l, float d) {
    return {id, l, ParamKind::Angle, 1, {d, 0.f, 0.f, 0.f}, 0.f, 360.f};
}
ParamDef P(const char* id, const char* l, float x, float y) {
    return {id, l, ParamKind::Point, 2, {x, y, 0.f, 0.f}, 0.f, 1.f};
}
ParamDef C(const char* id, const char* l, float r, float g, float b, float a = 1.f) {
    return {id, l, ParamKind::Color, 4, {r, g, b, a}, 0.f, 1.f};
}
}  // namespace

const std::vector<EffectDef>& moreEffects() {
    static const std::vector<EffectDef> k = {
        // ================= Blur & Glow =================
        {"motion_blur", "Blur Gerak", "Blur & Glow", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec2 d = dirUv(uP[0]) * uP[1] / uSize;
    vec4 acc = vec4(0.0);
    for (int i = 0; i < 24; i++) {
        float t = float(i) / 23.0 - 0.5;
        acc += src(uv + d * t);
    }
    return acc / 24.0;
})GLSL",
         {A("angle", "Arah", 0.f), F("dist", "Jarak (px)", 30.f, 0.f, 250.f)}},

        {"radial_blur", "Blur Putar", "Blur & Glow", 1, R"GLSL(
vec4 effect(vec2 uv) {
    float ar = uSize.x / uSize.y;
    vec2 c = ptUv(uP[0], uP[1]);
    vec2 p = (uv - c) * vec2(ar, 1.0);
    vec4 acc = vec4(0.0);
    for (int i = 0; i < 20; i++) {
        float a = radians(uP[2]) * (float(i) / 19.0 - 0.5);
        float s = sin(a), co = cos(a);
        vec2 q = vec2(p.x * co - p.y * s, p.x * s + p.y * co);
        acc += src(c + q / vec2(ar, 1.0));
    }
    return acc / 20.0;
})GLSL",
         {P("center", "Pusat", 0.5f, 0.5f), F("amount", "Sudut (derajat)", 10.f, 0.f, 90.f)}},

        {"lens_blur", "Blur Lensa (Bokeh)", "Blur & Glow", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 acc = vec4(0.0);
    float ws = 0.0;
    for (int i = 0; i < 32; i++) {
        float fi = float(i) + 0.5;
        float rad = sqrt(fi / 32.0);
        float ang = fi * 2.399963;
        vec2 o = vec2(cos(ang), sin(ang)) * rad * uP[0] / uSize;
        vec4 s = src(uv + o);
        float l = luma(unpremul(s).rgb);
        float w = 1.0 + uP[1] * pow(l, 4.0) * 4.0;
        acc += s * w;
        ws += w;
    }
    return acc / ws;
})GLSL",
         {F("radius", "Radius (px)", 14.f, 0.f, 80.f), F("boost", "Sorot terang", 1.f, 0.f, 3.f)}},

        {"soft_glow", "Cahaya Lembut", "Blur & Glow", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    vec4 b = vec4(0.0);
    for (int i = 0; i < 24; i++) {
        float fi = float(i) + 0.5;
        float rad = sqrt(fi / 24.0);
        float ang = fi * 2.399963;
        b += src(uv + vec2(cos(ang), sin(ang)) * rad * uP[0] / uSize);
    }
    b /= 24.0;
    return vec4(1.0 - (1.0 - base.rgb) * (1.0 - b.rgb * uP[1]), 1.0 - (1.0 - base.a) * (1.0 - b.a * uP[1]));
})GLSL",
         {F("radius", "Radius (px)", 30.f, 1.f, 100.f), F("amount", "Jumlah", 0.6f, 0.f, 1.f)}},

        // ================= Warna =================
        {"exposure", "Eksposur", "Warna", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    vec3 rgb = c.rgb * exp2(uP[0]) + uP[1];
    return premul(vec4(clamp(rgb, 0.0, 1.0), c.a));
})GLSL",
         {F("ev", "Eksposur (EV)", 0.5f, -3.f, 3.f), F("offset", "Offset", 0.f, -0.5f, 0.5f)}},

        {"temperature", "Suhu & Tint", "Warna", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    vec3 rgb = c.rgb * vec3(1.0 + uP[0] * 0.3, 1.0 + uP[1] * 0.2, 1.0 - uP[0] * 0.3);
    return premul(vec4(clamp(rgb, 0.0, 1.0), c.a));
})GLSL",
         {F("temp", "Suhu (dingin-hangat)", 0.4f, -1.f, 1.f), F("tint", "Tint (hijau-magenta)", 0.f, -1.f, 1.f)}},

        {"vibrance", "Vibrance", "Warna", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    float sat = max(max(c.r, c.g), c.b) - min(min(c.r, c.g), c.b);
    float k = uP[0] * (1.0 - sat);
    vec3 g = vec3(luma(c.rgb));
    return premul(vec4(clamp(mix(g, c.rgb, 1.0 + k), 0.0, 1.0), c.a));
})GLSL",
         {F("amount", "Jumlah", 0.9f, -1.f, 2.f)}},

        {"duotone", "Duotone", "Warna", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    vec3 col = mix(vec3(uP[0], uP[1], uP[2]), vec3(uP[4], uP[5], uP[6]), luma(c.rgb));
    return premul(vec4(mix(c.rgb, col, uP[8]), c.a));
})GLSL",
         {C("shadow", "Bayangan", 0.08f, 0.05f, 0.35f), C("light", "Terang", 1.f, 0.55f, 0.3f), F("amount", "Jumlah", 1.f, 0.f, 1.f)}},

        {"split_tone", "Split Tone", "Warna", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    float l = luma(c.rgb);
    float w = smoothstep(0.2, 0.8, l + uP[8] * 0.3);
    vec3 t = mix(vec3(uP[0], uP[1], uP[2]), vec3(uP[4], uP[5], uP[6]), w);
    vec3 rgb = mix(c.rgb, c.rgb * 0.5 + t * (l * 0.9 + 0.1), uP[9]);
    return premul(vec4(clamp(rgb, 0.0, 1.0), c.a));
})GLSL",
         {C("shadow", "Bayangan", 0.1f, 0.3f, 0.8f), C("light", "Terang", 1.f, 0.6f, 0.2f),
          F("balance", "Keseimbangan", 0.f, -1.f, 1.f), F("amount", "Jumlah", 0.6f, 0.f, 1.f)}},

        {"solarize", "Solarize", "Warna", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    vec3 rgb = mix(c.rgb, 1.0 - c.rgb, step(vec3(uP[0]), c.rgb));
    return premul(vec4(rgb, c.a));
})GLSL",
         {F("threshold", "Ambang", 0.5f, 0.f, 1.f)}},

        {"channel_swap", "Tukar Kanal", "Warna", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    int m = int(uP[0] + 0.5);
    vec3 o = c.rgb;
    if (m == 1) o = c.rbg;
    else if (m == 2) o = c.grb;
    else if (m == 3) o = c.gbr;
    else if (m == 4) o = c.brg;
    else if (m == 5) o = c.bgr;
    return premul(vec4(o, c.a));
})GLSL",
         {I("mode", "Urutan (0-5)", 5.f, 0.f, 5.f)}},

        {"film_matte", "Pudar Matte", "Warna", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    vec3 rgb = c.rgb * (1.0 - uP[0]) + uP[0] * 0.35;
    rgb = mix(rgb, vec3(0.5), uP[1]);
    return premul(vec4(rgb, c.a));
})GLSL",
         {F("fade", "Pudar hitam", 0.2f, 0.f, 1.f), F("flat", "Kurangi kontras", 0.2f, 0.f, 1.f)}},

        {"gradient_map", "Peta Gradien", "Warna", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    float l = luma(c.rgb);
    vec3 a = vec3(uP[0], uP[1], uP[2]);
    vec3 b = vec3(uP[4], uP[5], uP[6]);
    vec3 d = vec3(uP[8], uP[9], uP[10]);
    vec3 col = l < 0.5 ? mix(a, b, l * 2.0) : mix(b, d, (l - 0.5) * 2.0);
    return premul(vec4(mix(c.rgb, col, uP[12]), c.a));
})GLSL",
         {C("c1", "Gelap", 0.05f, 0.02f, 0.2f), C("c2", "Tengah", 0.85f, 0.2f, 0.5f), C("c3", "Terang", 1.f, 0.9f, 0.5f),
          F("amount", "Jumlah", 1.f, 0.f, 1.f)}},

        {"color_pop", "Warna Terpilih", "Warna", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    vec3 h = rgb2hsv(c.rgb);
    float dh = abs(fract(h.x - uP[0] / 360.0 + 0.5) - 0.5) * 360.0;
    float keep = 1.0 - smoothstep(uP[1] * (1.0 - uP[2] * 0.9), uP[1], dh);
    vec3 rgb = mix(c.rgb, mix(vec3(luma(c.rgb)), c.rgb, keep), uP[3]);
    return premul(vec4(rgb, c.a));
})GLSL",
         {A360("hue", "Hue dipertahankan", 0.f), F("range", "Rentang (derajat)", 40.f, 5.f, 180.f),
          F("soft", "Kelembutan", 0.3f, 0.f, 1.f), F("amount", "Jumlah", 1.f, 0.f, 1.f)}},

        // ================= Distorsi =================
        {"fisheye", "Lensa Fisheye", "Distorsi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    float ar = uSize.x / uSize.y;
    vec2 c = ptUv(uP[0], uP[1]);
    vec2 p = (uv - c) * vec2(ar, 1.0);
    float r = length(p);
    vec2 q = p * (1.0 + uP[2] * r * r * 2.0);
    return srcB(c + q / vec2(ar, 1.0));
})GLSL",
         {P("center", "Pusat", 0.5f, 0.5f), F("amount", "Jumlah", 0.6f, -1.f, 1.f)}},

        {"shake", "Getar", "Distorsi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    float t = floor(uTime * uP[1]);
    vec2 o = (vec2(hash21(vec2(t, 1.3)), hash21(vec2(t, 7.7))) - 0.5) * 2.0 * uP[0] / uSize;
    return srcB(uv + o);
})GLSL",
         {F("amount", "Jumlah (px)", 12.f, 0.f, 80.f), F("speed", "Kecepatan", 12.f, 1.f, 40.f)}},

        {"turbulent", "Turbulen", "Distorsi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec2 p = uv * uSize / uP[1];
    float t = uTime * uP[2];
    vec2 o = vec2(fbm(p + vec2(t, 0.0)) - 0.5, fbm(p + vec2(5.2, 1.3 + t)) - 0.5) * 2.0;
    return src(uv + o * uP[0] / uSize);
})GLSL",
         {F("amount", "Jumlah (px)", 24.f, 0.f, 120.f), F("size", "Ukuran (px)", 140.f, 10.f, 600.f),
          F("speed", "Animasi", 0.5f, 0.f, 3.f)}},

        {"polar", "Koordinat Polar", "Distorsi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    float ar = uSize.x / uSize.y;
    vec2 p = uv - 0.5;
    if (int(uP[0] + 0.5) == 0) {
        float a = atan(p.y, p.x) / 6.2831853 + 0.5;
        float r = clamp(length(p * vec2(ar, 1.0)) * 2.0 / max(ar, 1.0), 0.0, 1.0);
        return src(vec2(a, 1.0 - r));
    }
    float a = (uv.x - 0.5) * 6.2831853;
    float r = 1.0 - uv.y;
    vec2 q = vec2(cos(a), sin(a)) * r * 0.5;
    return srcB(vec2(0.5) + q * max(ar, 1.0) / vec2(ar, 1.0));
})GLSL",
         {I("mode", "Mode (0 persegi→polar, 1 polar→persegi)", 0.f, 0.f, 1.f)}},

        {"melt", "Meleleh", "Distorsi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    float n = vnoise(vec2(uv.x * uP[1], uTime * uP[2]));
    return src(uv + vec2(0.0, pow(n, 1.5) * uP[0] / uSize.y));
})GLSL",
         {F("amount", "Panjang (px)", 80.f, 0.f, 400.f), F("scale", "Jumlah tetes", 8.f, 1.f, 40.f),
          F("speed", "Animasi", 0.f, 0.f, 3.f)}},

        // ================= Stilisasi =================
        {"halftone", "Halftone", "Stilisasi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    vec2 px = uv * uSize;
    float a = radians(uP[1]);
    mat2 R = mat2(cos(a), -sin(a), sin(a), cos(a));
    vec2 g = R * px / uP[0];
    vec2 cell = floor(g) + 0.5;
    vec2 centerPx = transpose(R) * (cell * uP[0]);
    vec4 s = unpremul(src(centerPx / uSize));
    float l = luma(s.rgb);
    float rad = sqrt(clamp(1.0 - l * 0.85, 0.0, 1.0)) * 0.72;
    float m = 1.0 - smoothstep(rad - uP[3] * 0.4, rad + 0.04, length(g - cell));
    vec3 col = mix(vec3(1.0), s.rgb, m);
    vec4 o = vec4(col * base.a, base.a);
    return mix(base, o, uP[2]);
})GLSL",
         {F("size", "Ukuran (px)", 12.f, 3.f, 60.f), A360("angle", "Sudut", 45.f), F("mix", "Campur", 1.f, 0.f, 1.f),
          F("soft", "Kelembutan", 0.3f, 0.f, 1.f)}},

        {"emboss", "Emboss", "Stilisasi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec2 d = dirUv(uP[1]) / uSize;
    vec4 c = src(uv);
    float e = (luma(src(uv + d).rgb) - luma(src(uv - d).rgb)) * uP[0];
    vec4 o = vec4(vec3(0.5 + e) * c.a, c.a);
    return mix(c, o, uP[2]);
})GLSL",
         {F("strength", "Kekuatan", 2.f, 0.f, 6.f), A360("angle", "Arah cahaya", 135.f), F("mix", "Campur", 1.f, 0.f, 1.f)}},

        {"sharpen", "Pertajam", "Stilisasi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec2 dx = vec2(uP[1] / uSize.x, 0.0);
    vec2 dy = vec2(0.0, uP[1] / uSize.y);
    vec4 c = src(uv);
    vec4 bl = (src(uv + dx) + src(uv - dx) + src(uv + dy) + src(uv - dy)) * 0.25;
    vec4 o = c + (c - bl) * uP[0];
    o.a = clamp(o.a, 0.0, 1.0);
    o.rgb = clamp(o.rgb, 0.0, o.a);
    return o;
})GLSL",
         {F("amount", "Jumlah", 1.5f, 0.f, 4.f), F("radius", "Radius (px)", 1.2f, 0.5f, 4.f)}},

        {"sketch", "Sketsa Pensil", "Stilisasi", 1, R"GLSL(
float L(vec2 uv) { vec4 s = src(uv); return luma(s.rgb) + s.a * 0.3; }
vec4 effect(vec2 uv) {
    vec2 dx = vec2(1.0 / uSize.x, 0.0);
    vec2 dy = vec2(0.0, 1.0 / uSize.y);
    float gx = -L(uv - dx - dy) - 2.0 * L(uv - dx) - L(uv - dx + dy) + L(uv + dx - dy) + 2.0 * L(uv + dx) + L(uv + dx + dy);
    float gy = -L(uv - dx - dy) - 2.0 * L(uv - dy) - L(uv + dx - dy) + L(uv - dx + dy) + 2.0 * L(uv + dy) + L(uv + dx + dy);
    float e = clamp(length(vec2(gx, gy)) * uP[0], 0.0, 1.0);
    vec4 c = src(uv);
    vec4 o = vec4(vec3(uP[1] * (1.0 - e)) * c.a, c.a);
    return mix(c, o, uP[2]);
})GLSL",
         {F("strength", "Kekuatan garis", 3.f, 0.f, 8.f), F("paper", "Warna kertas", 0.95f, 0.f, 1.f), F("mix", "Campur", 1.f, 0.f, 1.f)}},

        {"comic", "Komik", "Stilisasi", 1, R"GLSL(
float L(vec2 uv) { vec4 s = src(uv); return luma(s.rgb) + s.a * 0.3; }
vec4 effect(vec2 uv) {
    vec2 dx = vec2(1.0 / uSize.x, 0.0);
    vec2 dy = vec2(0.0, 1.0 / uSize.y);
    float gx = L(uv + dx) - L(uv - dx);
    float gy = L(uv + dy) - L(uv - dy);
    float e = clamp(length(vec2(gx, gy)) * uP[1], 0.0, 1.0);
    vec4 c = unpremul(src(uv));
    vec3 rgb = floor(c.rgb * uP[0] + 0.5) / uP[0];
    rgb *= 1.0 - e;
    return premul(vec4(rgb, c.a));
})GLSL",
         {I("levels", "Level warna", 4.f, 2.f, 10.f), F("edge", "Garis tepi", 4.f, 0.f, 10.f)}},

        {"vhs", "VHS", "Stilisasi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    float y = uv.y * uSize.y;
    float w = (vnoise(vec2(y * 0.05, uTime * 3.0)) - 0.5) * uP[0] + (hash21(vec2(floor(y * 0.5), floor(uTime * 20.0))) - 0.5) * uP[0] * 0.3;
    vec2 o = vec2(w / uSize.x, 0.0);
    vec2 ch = vec2(uP[1] / uSize.x, 0.0);
    vec4 c = vec4(src(uv + o + ch).r, src(uv + o).g, src(uv + o - ch).b, src(uv + o).a);
    float n = hash21(uv * uSize + uTime) - 0.5;
    c.rgb += n * uP[2] * c.a;
    float sl = 0.5 + 0.5 * sin(y * 3.14159);
    c.rgb *= 1.0 - uP[3] * 0.5 * sl;
    return c;
})GLSL",
         {F("wobble", "Goyang (px)", 8.f, 0.f, 40.f), F("chroma", "Geser warna (px)", 4.f, 0.f, 20.f),
          F("noise", "Noise", 0.2f, 0.f, 1.f), F("scan", "Garis pindai", 0.3f, 0.f, 1.f)}},

        {"chromatic", "Aberasi Kromatik", "Stilisasi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec2 c = ptUv(uP[0], uP[1]);
    vec2 o = (uv - c) * 2.0 * uP[2] / uSize;
    vec4 a = src(uv + o);
    vec4 b = src(uv);
    vec4 d = src(uv - o);
    return vec4(a.r, b.g, d.b, max(max(a.a, b.a), d.a));
})GLSL",
         {P("center", "Pusat", 0.5f, 0.5f), F("amount", "Jumlah (px)", 10.f, 0.f, 50.f)}},

        {"dither", "Dither", "Stilisasi", 1, R"GLSL(
const float bay[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    ivec2 p = ivec2(floor(uv * uSize / uP[1])) & 3;
    float thr = (bay[p.y * 4 + p.x] + 0.5) / 16.0;
    float n = max(uP[0] - 1.0, 1.0);
    vec3 rgb = floor(c.rgb * n + thr) / n;
    return premul(vec4(rgb, c.a));
})GLSL",
         {I("levels", "Level warna", 3.f, 2.f, 16.f), F("size", "Ukuran piksel", 2.f, 1.f, 8.f)}},

        {"oil", "Lukisan Minyak", "Stilisasi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec2 px = uP[0] / uSize;
    float best = 1.0e9;
    vec4 res = src(uv);
    for (int q = 0; q < 4; q++) {
        vec2 sgn = vec2((q & 1) == 0 ? -1.0 : 1.0, (q & 2) == 0 ? -1.0 : 1.0);
        vec4 mean = vec4(0.0);
        float sq = 0.0;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) {
                vec4 s = src(uv + sgn * vec2(float(i), float(j)) * px);
                mean += s;
                float l = luma(s.rgb);
                sq += l * l;
            }
        mean /= 9.0;
        float v = sq / 9.0 - pow(luma(mean.rgb), 2.0);
        if (v < best) { best = v; res = mean; }
    }
    return res;
})GLSL",
         {F("radius", "Ukuran sapuan (px)", 3.f, 1.f, 8.f)}},

        // ================= Cahaya =================
        {"shine", "Kilau (Shine)", "Cahaya", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = src(uv);
    float ar = uSize.x / uSize.y;
    vec2 d = dirUv(uP[1] + 90.0);
    vec2 p = (uv - 0.5) * vec2(ar, 1.0);
    float extent = abs(ar * d.x) + abs(d.y);
    float t = dot(p, d) / extent + 0.5;
    float band = exp(-pow((t - uP[0]) / max(uP[2], 0.001), 2.0));
    vec3 add = vec3(uP[4], uP[5], uP[6]) * band * uP[3] * c.a;
    return vec4(clamp(c.rgb + add, 0.0, 1.0), c.a);
})GLSL",
         {F("pos", "Posisi (animasikan)", 0.5f, -0.5f, 1.5f), A360("angle", "Sudut", 30.f), F("width", "Lebar", 0.12f, 0.02f, 0.6f),
          F("intensity", "Intensitas", 1.2f, 0.f, 3.f), C("color", "Warna", 1.f, 1.f, 1.f)}},

        {"sun_rays", "Sinar Matahari", "Cahaya", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    vec2 c = ptUv(uP[0], uP[1]);
    vec2 d = (c - uv) * uP[2] / 24.0;
    vec3 acc = vec3(0.0);
    float aa = 0.0;
    float decay = 1.0;
    vec2 p = uv;
    for (int i = 0; i < 24; i++) {
        vec4 s = src(p);
        float k = smoothstep(uP[4], uP[4] + 0.2, luma(unpremul(s).rgb)) * s.a;
        acc += s.rgb * k * decay;
        aa += k * decay;
        decay *= 0.93;
        p += d;
    }
    acc /= 24.0;
    aa /= 24.0;
    return vec4(clamp(base.rgb + acc * uP[3] * 2.0, 0.0, 1.0), clamp(base.a + aa * uP[3] * 2.0, 0.0, 1.0));
})GLSL",
         {P("center", "Sumber", 0.8f, 0.2f), F("len", "Panjang", 0.6f, 0.f, 1.f), F("intensity", "Intensitas", 1.f, 0.f, 3.f),
          F("threshold", "Ambang", 0.5f, 0.f, 1.f)}},

        {"lens_flare", "Lens Flare", "Cahaya", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    float ar = uSize.x / uSize.y;
    vec2 c = ptUv(uP[0], uP[1]);
    vec3 col = vec3(uP[4], uP[5], uP[6]);
    float sz = uP[3];
    vec2 p = (uv - c) * vec2(ar, 1.0);
    float r = length(p);
    vec3 f = col * (exp(-r / sz * 3.0) * 1.2 + 0.25 * exp(-pow((r - sz * 1.6) / (sz * 0.12), 2.0)));
    vec2 axis = vec2(0.5) - c;
    for (int i = 1; i <= 3; i++) {
        vec2 gp = c + axis * (float(i) * 0.6);
        vec2 q = (uv - gp) * vec2(ar, 1.0);
        f += col * vec3(1.0, 0.8 + 0.07 * float(i), 0.6 + 0.1 * float(i)) * 0.3 * exp(-length(q) / (sz * (0.3 + 0.12 * float(i))) * 2.5);
    }
    f += col * 0.35 * exp(-abs(p.y) / (sz * 0.05)) * exp(-abs(p.x) / (sz * 1.8));
    f *= uP[2];
    return vec4(clamp(base.rgb + f, 0.0, 1.0), clamp(base.a + max(max(f.r, f.g), f.b), 0.0, 1.0));
})GLSL",
         {P("pos", "Posisi", 0.72f, 0.28f), F("intensity", "Intensitas", 1.f, 0.f, 3.f), F("size", "Ukuran", 0.2f, 0.05f, 0.6f),
          C("color", "Warna", 1.f, 0.85f, 0.6f)}},

        {"neon", "Neon", "Cahaya", 1, R"GLSL(
float L(vec2 uv) { vec4 s = src(uv); return luma(s.rgb) + s.a * 0.5; }
vec4 effect(vec2 uv) {
    vec2 dx = vec2(uP[5] / uSize.x, 0.0);
    vec2 dy = vec2(0.0, uP[5] / uSize.y);
    float gx = L(uv + dx) - L(uv - dx);
    float gy = L(uv + dy) - L(uv - dy);
    float e = clamp(length(vec2(gx, gy)) * uP[4], 0.0, 1.0);
    vec4 c = src(uv);
    vec3 lines = vec3(uP[0], uP[1], uP[2]) * e * 1.5;
    return vec4(clamp(c.rgb * (1.0 - uP[6]) + lines * uP[6], 0.0, 1.0), clamp(max(c.a * (1.0 - uP[6] * 0.5), e * uP[6]), 0.0, 1.0));
})GLSL",
         {C("color", "Warna", 0.2f, 1.f, 0.9f), F("strength", "Kekuatan", 3.f, 0.f, 8.f), F("width", "Tebal (px)", 3.f, 1.f, 30.f),
          F("mix", "Campur", 1.f, 0.f, 1.f)}},

        {"flicker", "Kedip", "Cahaya", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = src(uv);
    float f = 1.0 - uP[0] * hash21(vec2(floor(uTime * uP[1]), 3.1));
    return vec4(c.rgb * f, c.a);
})GLSL",
         {F("amount", "Jumlah", 0.5f, 0.f, 1.f), F("speed", "Kecepatan", 20.f, 1.f, 60.f)}},

        {"light_leak", "Bocoran Cahaya", "Cahaya", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    float ar = uSize.x / uSize.y;
    float t = uTime * uP[9];
    vec2 p1 = vec2(0.15 + 0.25 * sin(t * 1.3), 0.5 + 0.4 * cos(t * 0.9));
    vec2 p2 = vec2(0.85 + 0.2 * cos(t * 1.1), 0.3 + 0.4 * sin(t * 0.7));
    float g1 = exp(-pow(length((uv - p1) * vec2(ar, 1.0)) / uP[10], 2.0) * 3.0);
    float g2 = exp(-pow(length((uv - p2) * vec2(ar, 1.0)) / uP[10], 2.0) * 3.0);
    vec3 leak = (vec3(uP[0], uP[1], uP[2]) * g1 + vec3(uP[4], uP[5], uP[6]) * g2) * uP[8];
    leak = clamp(leak, 0.0, 1.0);
    return vec4(1.0 - (1.0 - base.rgb) * (1.0 - leak), 1.0 - (1.0 - base.a) * (1.0 - max(max(leak.r, leak.g), leak.b)));
})GLSL",
         {C("c1", "Warna 1", 1.f, 0.5f, 0.15f), C("c2", "Warna 2", 1.f, 0.2f, 0.4f), F("intensity", "Intensitas", 0.9f, 0.f, 2.f),
          F("speed", "Kecepatan", 0.4f, 0.f, 3.f), F("size", "Ukuran", 0.7f, 0.2f, 1.5f)}},

        // ================= Procedural =================
        {"checker", "Papan Catur", "Procedural", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    float a = radians(uP[1]);
    vec2 px = uv * uSize;
    vec2 r = vec2(px.x * cos(a) - px.y * sin(a), px.x * sin(a) + px.y * cos(a)) / uP[0];
    float par = mod(floor(r.x) + floor(r.y), 2.0);
    vec4 col = mix(vec4(uP[2], uP[3], uP[4], uP[5]), vec4(uP[6], uP[7], uP[8], uP[9]), par);
    return paint(base, col.rgb, col.a * uP[10]);
})GLSL",
         {F("size", "Ukuran (px)", 40.f, 4.f, 300.f), A360("angle", "Sudut", 0.f), C("a", "Warna A", 0.1f, 0.1f, 0.12f),
          C("b", "Warna B", 0.95f, 0.95f, 0.95f), F("amount", "Jumlah", 1.f, 0.f, 1.f)}},

        {"stripes", "Garis-garis", "Procedural", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    vec2 d = dirUv(uP[1]);
    float t = fract(dot(uv * uSize, d) / (uP[0] * 2.0));
    float s = smoothstep(0.5 - max(uP[10], 0.01) / (uP[0] * 2.0), 0.5 + max(uP[10], 0.01) / (uP[0] * 2.0), abs(t - 0.5) * 2.0 * 0.5 + 0.25);
    vec4 col = mix(vec4(uP[2], uP[3], uP[4], uP[5]), vec4(uP[6], uP[7], uP[8], uP[9]), s);
    return paint(base, col.rgb, col.a * uP[11]);
})GLSL",
         {F("width", "Lebar (px)", 30.f, 2.f, 200.f), A360("angle", "Sudut", 45.f), C("a", "Warna A", 1.f, 0.3f, 0.45f),
          C("b", "Warna B", 1.f, 0.9f, 0.6f), F("soft", "Kelembutan (px)", 1.f, 0.f, 20.f), F("amount", "Jumlah", 1.f, 0.f, 1.f)}},

        {"dots", "Titik-titik", "Procedural", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    float a = radians(uP[7]);
    vec2 px = uv * uSize;
    vec2 r = vec2(px.x * cos(a) - px.y * sin(a), px.x * sin(a) + px.y * cos(a)) / uP[0];
    float m = 1.0 - smoothstep(uP[1] - 0.04, uP[1] + 0.04, length(fract(r) - 0.5));
    return paint(base, vec3(uP[2], uP[3], uP[4]), m * uP[5] * uP[6]);
})GLSL",
         {F("spacing", "Jarak (px)", 30.f, 6.f, 200.f), F("radius", "Radius titik", 0.3f, 0.1f, 0.5f), C("color", "Warna", 1.f, 0.4f, 0.5f),
          F("amount", "Jumlah", 1.f, 0.f, 1.f), A360("angle", "Sudut", 0.f)}},

        {"grid_lines", "Kisi", "Procedural", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    vec2 px = uv * uSize;
    vec2 g = abs(fract(px / uP[0] + 0.5) - 0.5) * uP[0];
    float m = 1.0 - smoothstep(uP[1] * 0.5 - 0.5, uP[1] * 0.5 + 0.5, min(g.x, g.y));
    return paint(base, vec3(uP[2], uP[3], uP[4]), m * uP[5] * uP[6]);
})GLSL",
         {F("spacing", "Jarak (px)", 60.f, 10.f, 300.f), F("thick", "Tebal (px)", 2.f, 0.5f, 20.f), C("color", "Warna", 1.f, 1.f, 1.f),
          F("amount", "Jumlah", 1.f, 0.f, 1.f)}},

        {"plasma", "Plasma", "Procedural", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    float ar = uSize.x / uSize.y;
    vec2 p = uv * vec2(ar, 1.0);
    float t = uTime * uP[1];
    float v = sin(p.x * uP[0] + t) + sin(p.y * uP[0] + t * 1.3) + sin((p.x + p.y) * uP[0] * 0.7 + t * 0.7) + sin(length(p - 0.5) * uP[0] * 2.0 - t);
    vec3 col = 0.5 + 0.5 * cos(6.2831853 * (v * 0.25 + vec3(0.0, 0.33, 0.67)));
    return paint(base, col, uP[2]);
})GLSL",
         {F("scale", "Skala", 3.f, 0.5f, 10.f), F("speed", "Kecepatan", 1.f, 0.f, 4.f), F("amount", "Jumlah", 0.85f, 0.f, 1.f)}},

        {"clouds", "Awan (Noise)", "Procedural", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    float t = uTime * uP[1];
    float n = fbm(uv * uSize / uP[0] + vec2(t, t * 0.6));
    n = clamp((n - 0.5) * uP[2] + 0.5, 0.0, 1.0);
    vec4 col = mix(vec4(uP[3], uP[4], uP[5], uP[6]), vec4(uP[7], uP[8], uP[9], uP[10]), n);
    return paint(base, col.rgb, col.a * uP[11]);
})GLSL",
         {F("scale", "Skala (px)", 200.f, 20.f, 800.f), F("speed", "Kecepatan", 0.3f, 0.f, 2.f), F("contrast", "Kontras", 1.6f, 0.5f, 3.f),
          C("a", "Warna A", 0.05f, 0.05f, 0.25f), C("b", "Warna B", 0.95f, 0.65f, 1.f), F("amount", "Jumlah", 1.f, 0.f, 1.f)}},

        {"voronoi", "Sel Voronoi", "Procedural", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    vec2 p = uv * uSize / uP[0];
    vec2 ip = floor(p);
    vec2 fp = fract(p);
    float d1 = 8.0, d2 = 8.0;
    vec2 id = vec2(0.0);
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            vec2 g = vec2(float(i), float(j));
            vec2 o = vec2(hash21(ip + g), hash21(ip + g + vec2(31.7, 17.3)));
            o = 0.5 + 0.5 * sin(uTime * uP[1] + 6.2831853 * o);
            vec2 r = g + o - fp;
            float d = dot(r, r);
            if (d < d1) { d2 = d1; d1 = d; id = ip + g; }
            else if (d < d2) { d2 = d; }
        }
    float edge = smoothstep(0.0, max(uP[2], 0.001), sqrt(d2) - sqrt(d1));
    vec3 col = hsv2rgb(vec3(hash21(id), 0.55, 0.95)) * edge;
    return paint(base, col, uP[3]);
})GLSL",
         {F("scale", "Ukuran sel (px)", 100.f, 20.f, 400.f), F("speed", "Animasi", 0.5f, 0.f, 2.f), F("edge", "Garis tepi", 0.05f, 0.f, 0.3f),
          F("amount", "Jumlah", 1.f, 0.f, 1.f)}},

        {"radial_gradient", "Gradien Radial", "Procedural", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    float ar = uSize.x / uSize.y;
    vec2 p = (uv - ptUv(uP[0], uP[1])) * vec2(ar, 1.0);
    float t = smoothstep(0.0, 1.0, length(p) / uP[2]);
    vec4 col = mix(vec4(uP[3], uP[4], uP[5], uP[6]), vec4(uP[7], uP[8], uP[9], uP[10]), t);
    return paint(base, col.rgb, col.a * uP[11]);
})GLSL",
         {P("center", "Pusat", 0.5f, 0.5f), F("radius", "Radius", 0.6f, 0.05f, 1.5f), C("inner", "Warna tengah", 1.f, 0.8f, 0.3f),
          C("outer", "Warna luar", 0.8f, 0.1f, 0.5f), F("amount", "Jumlah", 1.f, 0.f, 1.f)}},

        // ================= Transisi (progress 0 = hilang, 1 = tampil; animasikan dengan keyframe) =================
        {"linear_wipe", "Wipe Linear", "Transisi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    float ar = uSize.x / uSize.y;
    vec2 d = dirUv(uP[1]);
    vec2 p = (uv - 0.5) * vec2(ar, 1.0);
    float t = dot(p, d) / (abs(ar * d.x) + abs(d.y)) + 0.5;
    float s = max(uP[2], 0.002);
    float th = uP[0] * (1.0 + s);
    float m = 1.0 - smoothstep(th - s, th, t);
    return src(uv) * m;
})GLSL",
         {F("progress", "Kemajuan", 0.5f, 0.f, 1.f), A360("angle", "Arah", 0.f), F("soft", "Kelembutan", 0.1f, 0.f, 1.f)}},

        {"radial_wipe", "Wipe Radial (Jam)", "Transisi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    float ar = uSize.x / uSize.y;
    vec2 p = (uv - 0.5) * vec2(ar, 1.0);
    float a = fract(atan(p.x, -p.y) / 6.2831853 + uP[1] / 360.0);
    float s = max(uP[2], 0.002);
    float th = uP[0] * (1.0 + s);
    float m = 1.0 - smoothstep(th - s, th, a);
    return src(uv) * m;
})GLSL",
         {F("progress", "Kemajuan", 0.5f, 0.f, 1.f), A360("start", "Sudut awal", 0.f), F("soft", "Kelembutan", 0.03f, 0.f, 0.5f)}},

        {"iris", "Iris (Lingkaran)", "Transisi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    float ar = uSize.x / uSize.y;
    vec2 c = ptUv(uP[1], uP[2]);
    float maxR = length(max(c, 1.0 - c) * vec2(ar, 1.0));
    float r = length((uv - c) * vec2(ar, 1.0)) / maxR;
    float s = max(uP[3], 0.002);
    float th = uP[0] * (1.0 + s);
    float m = 1.0 - smoothstep(th - s, th, r);
    return src(uv) * m;
})GLSL",
         {F("progress", "Kemajuan", 0.5f, 0.f, 1.f), P("center", "Pusat", 0.5f, 0.5f), F("soft", "Kelembutan", 0.05f, 0.f, 0.5f)}},

        {"blinds", "Tirai", "Transisi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    float ar = uSize.x / uSize.y;
    vec2 d = dirUv(uP[2]);
    float f = fract(dot((uv - 0.5) * vec2(ar, 1.0), d) * uP[1]);
    float m = 1.0 - smoothstep(uP[0] - 0.02, uP[0] + 0.02, f);
    m = uP[0] >= 0.999 ? 1.0 : m;
    return src(uv) * m;
})GLSL",
         {F("progress", "Kemajuan", 0.5f, 0.f, 1.f), I("count", "Jumlah tirai", 8.f, 1.f, 40.f), A360("angle", "Arah", 0.f)}},

        {"pixel_dissolve", "Urai Piksel", "Transisi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    float h = hash21(floor(uv * uSize / uP[1]));
    float s = max(uP[2], 0.002);
    float th = uP[0] * (1.0 + s);
    float m = 1.0 - smoothstep(th - s, th, h);
    return src(uv) * m;
})GLSL",
         {F("progress", "Kemajuan", 0.5f, 0.f, 1.f), F("size", "Ukuran blok (px)", 24.f, 2.f, 120.f), F("soft", "Kelembutan", 0.02f, 0.f, 0.5f)}},

        {"noise_dissolve", "Urai Noise", "Transisi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    float n = clamp((fbm(uv * uSize / uP[1]) - 0.2) / 0.6, 0.0, 1.0);
    float s = max(uP[2], 0.002);
    float th = uP[0] * (1.0 + s);
    float m = 1.0 - smoothstep(th - s, th, n);
    return src(uv) * m;
})GLSL",
         {F("progress", "Kemajuan", 0.5f, 0.f, 1.f), F("scale", "Skala (px)", 200.f, 20.f, 600.f), F("soft", "Kelembutan", 0.1f, 0.f, 0.5f)}},

        // ================= Ulangi =================
        {"tile", "Ubin (Ulangi)", "Ulangi", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec2 p = uv * vec2(uP[0], uP[1]);
    if (uP[2] > 0.5) {
        vec2 m = fract(p * 0.5) * 2.0;
        return src(vec2(m.x > 1.0 ? 2.0 - m.x : m.x, m.y > 1.0 ? 2.0 - m.y : m.y));
    }
    return src(fract(p));
})GLSL",
         {F("x", "Ulangi X", 3.f, 1.f, 12.f), F("y", "Ulangi Y", 3.f, 1.f, 12.f), {"mirror", "Cermin bergantian", ParamKind::Bool, 1, {0.f, 0.f, 0.f, 0.f}, 0.f, 1.f}}},

        // ================= 3D =================
        {"rotate3d", "Putar 3D", "3D", 1, R"GLSL(
mat3 rotX(float a) { float c = cos(a), s = sin(a); return mat3(1.0, 0.0, 0.0, 0.0, c, s, 0.0, -s, c); }
mat3 rotY(float a) { float c = cos(a), s = sin(a); return mat3(c, 0.0, -s, 0.0, 1.0, 0.0, s, 0.0, c); }
mat3 rotZ(float a) { float c = cos(a), s = sin(a); return mat3(c, s, 0.0, -s, c, 0.0, 0.0, 0.0, 1.0); }
vec4 effect(vec2 uv) {
    float ar = uSize.x / uSize.y;
    vec2 s = (uv - 0.5) * vec2(ar, 1.0);
    float d = uP[3];
    mat3 R = rotZ(radians(uP[2])) * rotY(radians(uP[1])) * rotX(radians(uP[0]));
    vec3 n = R * vec3(0.0, 0.0, 1.0);
    vec3 O = vec3(0.0, 0.0, d);
    vec3 D = vec3(s, -d);
    float den = dot(n, D);
    if (abs(den) < 1.0e-4) return vec4(0.0);
    float t = -dot(n, O) / den;
    if (t <= 0.0) return vec4(0.0);
    vec3 q = transpose(R) * (O + t * D);
    return srcB(q.xy / vec2(ar, 1.0) + 0.5);
})GLSL",
         {A("rx", "Putar X", 25.f), A("ry", "Putar Y", 35.f), A("rz", "Putar Z", 0.f), F("dist", "Jarak kamera", 2.5f, 0.8f, 6.f)}},

        {"extrude", "Ekstrusi 3D", "3D", 1, R"GLSL(
vec4 effect(vec2 uv) {
    vec2 d = dirUv(uP[1]) * uP[0] / uSize;
    vec3 side = vec3(uP[2], uP[3], uP[4]);
    vec4 acc = vec4(0.0);
    for (int i = 31; i >= 1; i--) {
        float t = float(i) / 31.0;
        vec4 s = src(uv - d * t);
        vec3 col = mix(unpremul(s).rgb * (1.0 - 0.5 * t), side * (1.0 - 0.4 * t), uP[6]);
        s = vec4(col * s.a, s.a);
        acc = s + acc * (1.0 - s.a);
    }
    vec4 f = src(uv);
    return f + acc * (1.0 - f.a);
})GLSL",
         {F("depth", "Kedalaman (px)", 60.f, 0.f, 150.f), A360("angle", "Arah", 315.f), C("side", "Warna sisi", 0.35f, 0.25f, 0.9f),
          F("mix", "Warna sisi vs asli", 0.6f, 0.f, 1.f)}},
    };
    return k;
}

}  // namespace studio
