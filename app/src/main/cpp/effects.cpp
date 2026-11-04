// DIHASILKAN dari tools; boleh diedit langsung. Tiap efek = GLSL ES 3.00 + daftar parameter.
#include "effects.h"

#include <algorithm>
#include <cstdio>

namespace studio {
namespace {
const char* kHeader = R"GLSL(#version 300 es
precision highp float;
precision highp int;
in vec2 vUv;
uniform sampler2D uTex;
uniform vec2 uSize;
uniform float uTime;
uniform int uPass;
uniform float uP[32];
out vec4 outColor;

vec4 src(vec2 uv) { return texture(uTex, uv); }
vec4 unpremul(vec4 c) { return c.a > 0.0001 ? vec4(c.rgb / c.a, c.a) : vec4(0.0); }
vec4 premul(vec4 c) { return vec4(c.rgb * c.a, c.a); }
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}
// Arah dari sudut (derajat, searah jarum jam di layar) dalam ruang uv (v ke atas).
vec2 dirUv(float deg) { float a = radians(deg); return vec2(cos(a), -sin(a)); }
// Titik 0..1 dengan asal kiri-atas -> ruang uv.
vec2 ptUv(float x, float y) { return vec2(x, 1.0 - y); }

vec4 srcB(vec2 uv) { return (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) ? vec4(0.0) : texture(uTex, uv); }
float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i), b = hash21(i + vec2(1.0, 0.0)), c = hash21(i + vec2(0.0, 1.0)), d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) { v += a * vnoise(p); p *= 2.03; a *= 0.5; }
    return v;
}
// Cat layer dengan warna col (0..1 = seberapa kuat), alfa layer dipertahankan.
vec4 paint(vec4 base, vec3 col, float m) {
    vec3 b = base.a > 0.0001 ? base.rgb / base.a : col;
    return vec4(mix(b, col, clamp(m, 0.0, 1.0)) * base.a, base.a);
}

vec4 effect(vec2 uv);
void main() { outColor = effect(vUv); }
)GLSL";
const char* kVertex = R"GLSL(#version 300 es
layout(location = 0) in vec2 aPos;
out vec2 vUv;
void main() {
    vUv = aPos + 0.5;
    gl_Position = vec4(aPos * 2.0, 0.0, 1.0);
}
)GLSL";
}
int EffectDef::flatCount() const {
    int n = 0;
    for (const ParamDef& p : params) n += p.comps;
    return n;
}

int EffectDef::offsetOf(int idx) const {
    int n = 0;
    for (int i = 0; i < idx && i < static_cast<int>(params.size()); ++i) n += params[static_cast<size_t>(i)].comps;
    return n;
}
const std::vector<EffectDef>& moreEffects();  // effects_more.cpp
static const std::vector<EffectDef>& baseCatalog() {
    static const std::vector<EffectDef> kCatalog = {
        {"gaussian_blur", "Blur Gaussian", "Blur & Glow", 2,
         R"GLSL(
vec4 effect(vec2 uv) {
    float r = uP[0];
    if (r < 0.01) return src(uv);
    vec2 dir = (uPass == 0) ? vec2(1.0 / uSize.x, 0.0) : vec2(0.0, 1.0 / uSize.y);
    float sigma = max(r * 0.5, 0.0001);
    float st = r / 12.0;
    vec4 acc = vec4(0.0);
    float ws = 0.0;
    for (int i = -12; i <= 12; i++) {
        float x = float(i) * st;
        float w = exp(-(x * x) / (2.0 * sigma * sigma));
        acc += src(uv + dir * x) * w;
        ws += w;
    }
    return acc / ws;
})GLSL",
         {
             {"radius", "Radius (px)", ParamKind::Float, 1, {10.f, 0.f, 0.f, 0.f}, 0.f, 100.f},
         }},
        {"zoom_blur", "Blur Zoom", "Blur & Glow", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec2 c = ptUv(uP[0], uP[1]);
    vec2 d = (c - uv) * uP[2];
    vec4 acc = vec4(0.0);
    for (int i = 0; i < 16; i++) {
        float t = float(i) / 15.0;
        acc += src(uv + d * t);
    }
    return acc / 16.0;
})GLSL",
         {
             {"center", "Pusat", ParamKind::Point, 2, {0.5f, 0.5f, 0.f, 0.f}, 0.f, 1.f},
             {"amount", "Jumlah", ParamKind::Float, 1, {0.2f, 0.f, 0.f, 0.f}, 0.f, 1.f},
         }},
        {"glow", "Glow", "Blur & Glow", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    float r = uP[0];
    vec3 g = vec3(0.0);
    float ga = 0.0;
    float wsum = 0.0;
    for (int i = 0; i < 32; i++) {
        float fi = float(i) + 0.5;
        float rad = sqrt(fi / 32.0);
        float ang = fi * 2.399963;
        vec2 o = vec2(cos(ang), sin(ang)) * rad * r / uSize;
        vec4 s = src(uv + o);
        float l = luma(unpremul(s).rgb);
        float k = smoothstep(uP[2], uP[2] + 0.2, l) * s.a;
        float w = 1.0 - rad * 0.5;
        g += unpremul(s).rgb * k * w;
        ga += k * w;
        wsum += w;
    }
    g /= wsum; ga /= wsum;
    vec3 add = g * uP[1];
    float aa = clamp(ga * uP[1], 0.0, 1.0);
    return vec4(base.rgb + add, clamp(base.a + aa * (1.0 - base.a), 0.0, 1.0));
})GLSL",
         {
             {"radius", "Radius (px)", ParamKind::Float, 1, {20.f, 0.f, 0.f, 0.f}, 1.f, 100.f},
             {"intensity", "Intensitas", ParamKind::Float, 1, {1.f, 0.f, 0.f, 0.f}, 0.f, 4.f},
             {"threshold", "Ambang", ParamKind::Float, 1, {0.4f, 0.f, 0.f, 0.f}, 0.f, 1.f},
         }},
        {"drop_shadow", "Sombra", "Blur & Glow", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    vec2 off = dirUv(uP[6]) * uP[5] / uSize;
    float r = uP[7];
    float a = 0.0;
    for (int i = 0; i < 24; i++) {
        float fi = float(i) + 0.5;
        float rad = sqrt(fi / 24.0);
        float ang = fi * 2.399963;
        vec2 o = vec2(cos(ang), sin(ang)) * rad * r / uSize;
        a += src(uv - off + o).a;
    }
    a = a / 24.0 * uP[4] * uP[3];
    vec4 sh = vec4(uP[0], uP[1], uP[2], 1.0) * a;
    return base + sh * (1.0 - base.a);
})GLSL",
         {
             {"color", "Warna", ParamKind::Color, 4, {0.f, 0.f, 0.f, 1.f}, 0.f, 1.f},
             {"opacity", "Opasitas", ParamKind::Float, 1, {0.6f, 0.f, 0.f, 0.f}, 0.f, 1.f},
             {"distance", "Jarak (px)", ParamKind::Float, 1, {20.f, 0.f, 0.f, 0.f}, 0.f, 200.f},
             {"angle", "Sudut", ParamKind::Angle, 1, {45.f, 0.f, 0.f, 0.f}, -180.f, 180.f},
             {"soft", "Kelembutan (px)", ParamKind::Float, 1, {12.f, 0.f, 0.f, 0.f}, 0.f, 80.f},
         }},
        {"outline", "Garis Tepi", "Blur & Glow", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 base = src(uv);
    float w = uP[4];
    float m = 0.0;
    for (int ring = 1; ring <= 3; ring++) {
        float rr = w * float(ring) / 3.0;
        for (int i = 0; i < 16; i++) {
            float ang = float(i) * 0.3926991;
            m = max(m, src(uv + vec2(cos(ang), sin(ang)) * rr / uSize).a);
        }
    }
    vec4 ol = vec4(uP[0], uP[1], uP[2], 1.0) * (m * uP[3]);
    return base + ol * (1.0 - base.a);
})GLSL",
         {
             {"color", "Warna", ParamKind::Color, 4, {1.f, 1.f, 1.f, 1.f}, 0.f, 1.f},
             {"width", "Tebal (px)", ParamKind::Float, 1, {6.f, 0.f, 0.f, 0.f}, 0.f, 40.f},
         }},
        {"brightness_contrast", "Kecerahan/Kontras", "Warna", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    float k = uP[1] >= 0.0 ? 1.0 / max(1.0 - uP[1], 0.001) : 1.0 + uP[1];
    c.rgb = (c.rgb - 0.5) * k + 0.5 + uP[0];
    return premul(vec4(clamp(c.rgb, 0.0, 1.0), c.a));
})GLSL",
         {
             {"brightness", "Kecerahan", ParamKind::Float, 1, {0.f, 0.f, 0.f, 0.f}, -1.f, 1.f},
             {"contrast", "Kontras", ParamKind::Float, 1, {0.f, 0.f, 0.f, 0.f}, -1.f, 1.f},
         }},
        {"hue_saturation", "Hue/Saturasi", "Warna", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    vec3 h = rgb2hsv(c.rgb);
    h.x = fract(h.x + uP[0] / 360.0);
    h.y = clamp(h.y * (1.0 + uP[1]), 0.0, 1.0);
    vec3 rgb = hsv2rgb(h);
    rgb = uP[2] >= 0.0 ? mix(rgb, vec3(1.0), uP[2]) : mix(rgb, vec3(0.0), -uP[2]);
    return premul(vec4(rgb, c.a));
})GLSL",
         {
             {"hue", "Hue", ParamKind::Angle, 1, {0.f, 0.f, 0.f, 0.f}, -180.f, 180.f},
             {"sat", "Saturasi", ParamKind::Float, 1, {0.f, 0.f, 0.f, 0.f}, -1.f, 1.f},
             {"light", "Terang", ParamKind::Float, 1, {0.f, 0.f, 0.f, 0.f}, -1.f, 1.f},
         }},
        {"levels", "Levels", "Warna", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    vec3 x = clamp((c.rgb - uP[0]) / max(uP[1] - uP[0], 0.0001), 0.0, 1.0);
    x = pow(x, vec3(1.0 / max(uP[2], 0.01)));
    return premul(vec4(x, c.a));
})GLSL",
         {
             {"inb", "Hitam masuk", ParamKind::Float, 1, {0.f, 0.f, 0.f, 0.f}, 0.f, 1.f},
             {"inw", "Putih masuk", ParamKind::Float, 1, {1.f, 0.f, 0.f, 0.f}, 0.f, 1.f},
             {"gamma", "Gamma", ParamKind::Float, 1, {1.f, 0.f, 0.f, 0.f}, 0.1f, 4.f},
         }},
        {"invert", "Invert", "Warna", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    return premul(vec4(mix(c.rgb, 1.0 - c.rgb, uP[0]), c.a));
})GLSL",
         {
             {"amount", "Jumlah", ParamKind::Float, 1, {1.f, 0.f, 0.f, 0.f}, 0.f, 1.f},
         }},
        {"grayscale", "Hitam Putih", "Warna", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    return premul(vec4(mix(c.rgb, vec3(luma(c.rgb)), uP[0]), c.a));
})GLSL",
         {
             {"amount", "Jumlah", ParamKind::Float, 1, {1.f, 0.f, 0.f, 0.f}, 0.f, 1.f},
         }},
        {"sepia", "Sepia", "Warna", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    vec3 s = vec3(dot(c.rgb, vec3(0.393, 0.769, 0.189)), dot(c.rgb, vec3(0.349, 0.686, 0.168)), dot(c.rgb, vec3(0.272, 0.534, 0.131)));
    return premul(vec4(mix(c.rgb, clamp(s, 0.0, 1.0), uP[0]), c.a));
})GLSL",
         {
             {"amount", "Jumlah", ParamKind::Float, 1, {1.f, 0.f, 0.f, 0.f}, 0.f, 1.f},
         }},
        {"tint", "Tint", "Warna", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    vec3 t = vec3(uP[0], uP[1], uP[2]) * luma(c.rgb) * 1.6;
    return premul(vec4(mix(c.rgb, clamp(t, 0.0, 1.0), uP[4]), c.a));
})GLSL",
         {
             {"color", "Warna", ParamKind::Color, 4, {1.f, 0.5f, 0.2f, 1.f}, 0.f, 1.f},
             {"amount", "Jumlah", ParamKind::Float, 1, {0.6f, 0.f, 0.f, 0.f}, 0.f, 1.f},
         }},
        {"posterize", "Posterize", "Warna", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    float n = max(uP[0] - 1.0, 1.0);
    return premul(vec4(floor(c.rgb * n + 0.5) / n, c.a));
})GLSL",
         {
             {"levels", "Level", ParamKind::Int, 1, {6.f, 0.f, 0.f, 0.f}, 2.f, 32.f},
         }},
        {"threshold", "Threshold", "Warna", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    float v = step(uP[0], luma(c.rgb));
    return premul(vec4(vec3(v), c.a));
})GLSL",
         {
             {"level", "Ambang", ParamKind::Float, 1, {0.5f, 0.f, 0.f, 0.f}, 0.f, 1.f},
         }},
        {"fill", "Isi Warna", "Warna", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = src(uv);
    return vec4(vec3(uP[0], uP[1], uP[2]) * c.a * uP[3], c.a);
})GLSL",
         {
             {"color", "Warna", ParamKind::Color, 4, {1.f, 0.f, 0.f, 1.f}, 0.f, 1.f},
         }},
        {"gradient_overlay", "Gradien", "Warna", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = src(uv);
    vec2 d = dirUv(uP[8]);
    float t = clamp(dot(uv - 0.5, d) + 0.5, 0.0, 1.0);
    vec3 g = mix(vec3(uP[0], uP[1], uP[2]), vec3(uP[4], uP[5], uP[6]), t);
    vec3 base = unpremul(c).rgb;
    return premul(vec4(mix(base, g, uP[9]), c.a));
})GLSL",
         {
             {"c1", "Warna 1", ParamKind::Color, 4, {1.f, 0.2f, 0.4f, 1.f}, 0.f, 1.f},
             {"c2", "Warna 2", ParamKind::Color, 4, {0.2f, 0.4f, 1.f, 1.f}, 0.f, 1.f},
             {"angle", "Sudut", ParamKind::Angle, 1, {90.f, 0.f, 0.f, 0.f}, -180.f, 180.f},
             {"amount", "Jumlah", ParamKind::Float, 1, {0.8f, 0.f, 0.f, 0.f}, 0.f, 1.f},
         }},
        {"chroma_key", "Chroma Key", "Warna", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = unpremul(src(uv));
    float d = distance(c.rgb, vec3(uP[0], uP[1], uP[2]));
    float k = smoothstep(uP[4], uP[4] + max(uP[5], 0.001), d);
    return premul(vec4(c.rgb, c.a * k));
})GLSL",
         {
             {"key", "Warna kunci", ParamKind::Color, 4, {0.f, 1.f, 0.f, 1.f}, 0.f, 1.f},
             {"tol", "Toleransi", ParamKind::Float, 1, {0.3f, 0.f, 0.f, 0.f}, 0.f, 1.f},
             {"soft", "Kelembutan", ParamKind::Float, 1, {0.1f, 0.f, 0.f, 0.f}, 0.f, 1.f},
         }},
        {"wave", "Gelombang", "Distorsi", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec2 px = uv * uSize;
    vec2 d = dirUv(uP[3]);
    vec2 n = vec2(-d.y, d.x);
    float ph = dot(px, d) / uP[1] * 6.2831853 + uTime * uP[2];
    vec2 off = n * sin(ph) * uP[0] / uSize;
    return src(uv + off);
})GLSL",
         {
             {"amp", "Amplitudo (px)", ParamKind::Float, 1, {20.f, 0.f, 0.f, 0.f}, 0.f, 150.f},
             {"len", "Panjang (px)", ParamKind::Float, 1, {120.f, 0.f, 0.f, 0.f}, 10.f, 600.f},
             {"speed", "Kecepatan", ParamKind::Float, 1, {2.f, 0.f, 0.f, 0.f}, 0.f, 20.f},
             {"angle", "Arah", ParamKind::Angle, 1, {0.f, 0.f, 0.f, 0.f}, -180.f, 180.f},
         }},
        {"ripple", "Riak Air", "Distorsi", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec2 c = ptUv(uP[0], uP[1]);
    vec2 px = (uv - c) * uSize;
    float r = length(px);
    vec2 dir = r > 0.001 ? px / r : vec2(0.0);
    float ph = r / uP[3] * 6.2831853 - uTime * uP[4];
    vec2 off = dir * sin(ph) * uP[2] / uSize;
    return src(uv + off);
})GLSL",
         {
             {"center", "Pusat", ParamKind::Point, 2, {0.5f, 0.5f, 0.f, 0.f}, 0.f, 1.f},
             {"amp", "Amplitudo (px)", ParamKind::Float, 1, {18.f, 0.f, 0.f, 0.f}, 0.f, 100.f},
             {"len", "Panjang (px)", ParamKind::Float, 1, {80.f, 0.f, 0.f, 0.f}, 10.f, 400.f},
             {"speed", "Kecepatan", ParamKind::Float, 1, {3.f, 0.f, 0.f, 0.f}, 0.f, 20.f},
         }},
        {"twirl", "Pusaran", "Distorsi", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec2 c = ptUv(uP[0], uP[1]);
    float m = min(uSize.x, uSize.y);
    vec2 p = (uv - c) * uSize / m;
    float r = length(p);
    float R = uP[3];
    if (r >= R) return src(uv);
    float t = 1.0 - r / R;
    float a = radians(uP[2]) * t * t;
    float cs = cos(a), sn = sin(a);
    vec2 q = vec2(cs * p.x - sn * p.y, sn * p.x + cs * p.y);
    return src(c + q * m / uSize);
})GLSL",
         {
             {"center", "Pusat", ParamKind::Point, 2, {0.5f, 0.5f, 0.f, 0.f}, 0.f, 1.f},
             {"angle", "Sudut", ParamKind::Angle, 1, {180.f, 0.f, 0.f, 0.f}, -720.f, 720.f},
             {"radius", "Radius", ParamKind::Float, 1, {0.4f, 0.f, 0.f, 0.f}, 0.01f, 1.f},
         }},
        {"bulge", "Cembung/Cekung", "Distorsi", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec2 c = ptUv(uP[0], uP[1]);
    float m = min(uSize.x, uSize.y);
    vec2 p = (uv - c) * uSize / m;
    float r = length(p);
    float R = uP[3];
    if (r >= R) return src(uv);
    float t = r / R;
    float k = uP[2] >= 0.0 ? mix(1.0, t, uP[2]) : mix(1.0, 1.0 / max(t, 0.05), -uP[2] * 0.6);
    return src(c + p * k * m / uSize);
})GLSL",
         {
             {"center", "Pusat", ParamKind::Point, 2, {0.5f, 0.5f, 0.f, 0.f}, 0.f, 1.f},
             {"amount", "Jumlah", ParamKind::Float, 1, {0.5f, 0.f, 0.f, 0.f}, -1.f, 1.f},
             {"radius", "Radius", ParamKind::Float, 1, {0.4f, 0.f, 0.f, 0.f}, 0.01f, 1.f},
         }},
        {"kaleidoscope", "Kaleidoskop", "Distorsi", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec2 c = ptUv(uP[0], uP[1]);
    float m = min(uSize.x, uSize.y);
    vec2 p = (uv - c) * uSize / m;
    float r = length(p);
    float a = atan(p.y, p.x) - radians(uP[3]);
    float seg = 6.2831853 / max(uP[2], 2.0);
    a = mod(a, seg);
    a = abs(a - seg * 0.5);
    a += radians(uP[3]);
    vec2 q = vec2(cos(a), sin(a)) * r;
    return src(c + q * m / uSize);
})GLSL",
         {
             {"center", "Pusat", ParamKind::Point, 2, {0.5f, 0.5f, 0.f, 0.f}, 0.f, 1.f},
             {"segments", "Segmen", ParamKind::Int, 1, {6.f, 0.f, 0.f, 0.f}, 2.f, 24.f},
             {"rotation", "Rotasi", ParamKind::Angle, 1, {0.f, 0.f, 0.f, 0.f}, -180.f, 180.f},
         }},
        {"mirror", "Cermin", "Distorsi", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec2 c = ptUv(uP[0], uP[1]);
    vec2 n = vec2(-dirUv(uP[2]).y, dirUv(uP[2]).x);
    vec2 p = (uv - c) * uSize;
    float s = dot(p, n);
    if (s > 0.0) p -= 2.0 * s * n;
    return src(c + p / uSize);
})GLSL",
         {
             {"center", "Pusat", ParamKind::Point, 2, {0.5f, 0.5f, 0.f, 0.f}, 0.f, 1.f},
             {"angle", "Sudut garis", ParamKind::Angle, 1, {90.f, 0.f, 0.f, 0.f}, -180.f, 180.f},
         }},
        {"pixelate", "Pixelate", "Distorsi", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec2 cell = vec2(max(uP[0], 1.0)) / uSize;
    vec2 q = (floor(uv / cell) + 0.5) * cell;
    return src(q);
})GLSL",
         {
             {"size", "Ukuran (px)", ParamKind::Float, 1, {16.f, 0.f, 0.f, 0.f}, 1.f, 100.f},
         }},
        {"rgb_split", "Pisah RGB", "Distorsi", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec2 o = dirUv(uP[1]) * uP[0] / uSize;
    vec4 r = src(uv + o);
    vec4 g = src(uv);
    vec4 b = src(uv - o);
    float a = max(max(r.a, g.a), b.a);
    return vec4(r.r, g.g, b.b, a);
})GLSL",
         {
             {"amount", "Jumlah (px)", ParamKind::Float, 1, {10.f, 0.f, 0.f, 0.f}, 0.f, 80.f},
             {"angle", "Sudut", ParamKind::Angle, 1, {0.f, 0.f, 0.f, 0.f}, -180.f, 180.f},
         }},
        {"glitch", "Glitch", "Distorsi", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    float t = floor(uTime * uP[2]);
    float row = floor(uv.y * uP[1]);
    float h = hash21(vec2(row, t));
    float on = step(1.0 - uP[0] * 0.6, h);
    float shift = (hash21(vec2(row + 17.0, t)) - 0.5) * 0.25 * uP[0] * on;
    vec2 u2 = vec2(uv.x + shift, uv.y);
    float sp = 0.01 * uP[0] * on;
    vec4 r = src(u2 + vec2(sp, 0.0));
    vec4 g = src(u2);
    vec4 b = src(u2 - vec2(sp, 0.0));
    return vec4(r.r, g.g, b.b, max(max(r.a, g.a), b.a));
})GLSL",
         {
             {"amount", "Jumlah", ParamKind::Float, 1, {0.5f, 0.f, 0.f, 0.f}, 0.f, 1.f},
             {"slices", "Irisan", ParamKind::Int, 1, {16.f, 0.f, 0.f, 0.f}, 2.f, 60.f},
             {"speed", "Kecepatan", ParamKind::Float, 1, {8.f, 0.f, 0.f, 0.f}, 0.f, 30.f},
         }},
        {"noise", "Grain", "Stilisasi", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = src(uv);
    vec2 cell = floor(uv * uSize / max(uP[1], 1.0));
    float n = hash21(cell + (uP[2] > 0.5 ? floor(uTime * 24.0) * 7.31 : 0.0)) - 0.5;
    vec3 rgb = unpremul(c).rgb + n * uP[0];
    return premul(vec4(clamp(rgb, 0.0, 1.0), c.a));
})GLSL",
         {
             {"amount", "Jumlah", ParamKind::Float, 1, {0.25f, 0.f, 0.f, 0.f}, 0.f, 1.f},
             {"size", "Ukuran (px)", ParamKind::Float, 1, {2.f, 0.f, 0.f, 0.f}, 1.f, 10.f},
             {"anim", "Animasi", ParamKind::Bool, 1, {1.f, 0.f, 0.f, 0.f}, 0.f, 1.f},
         }},
        {"scanlines", "Garis Pindai", "Stilisasi", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = src(uv);
    float l = mod(floor(uv.y * uSize.y / max(uP[0], 1.0)), 2.0);
    return c * (1.0 - l * uP[1]);
})GLSL",
         {
             {"height", "Tinggi baris (px)", ParamKind::Float, 1, {4.f, 0.f, 0.f, 0.f}, 1.f, 30.f},
             {"intensity", "Intensitas", ParamKind::Float, 1, {0.4f, 0.f, 0.f, 0.f}, 0.f, 1.f},
         }},
        {"vignette", "Vinyet", "Stilisasi", 1,
         R"GLSL(
vec4 effect(vec2 uv) {
    vec4 c = src(uv);
    float d = length((uv - 0.5) * vec2(uSize.x / uSize.y, 1.0));
    float v = smoothstep(uP[1], uP[1] + uP[2], d);
    return vec4(c.rgb * (1.0 - v * uP[0]), c.a);
})GLSL",
         {
             {"amount", "Jumlah", ParamKind::Float, 1, {0.6f, 0.f, 0.f, 0.f}, 0.f, 1.f},
             {"size", "Ukuran", ParamKind::Float, 1, {0.55f, 0.f, 0.f, 0.f}, 0.f, 1.5f},
             {"soft", "Kelembutan", ParamKind::Float, 1, {0.5f, 0.f, 0.f, 0.f}, 0.01f, 1.f},
         }},
        {"edge_detect", "Deteksi Tepi", "Stilisasi", 1,
         R"GLSL(
float lumAt(vec2 uv) { vec4 s = src(uv); return luma(unpremul(s).rgb) * s.a; }
vec4 effect(vec2 uv) {
    vec2 t = 1.0 / uSize;
    float tl = lumAt(uv + vec2(-t.x, t.y)), tc = lumAt(uv + vec2(0.0, t.y)), tr = lumAt(uv + vec2(t.x, t.y));
    float ml = lumAt(uv + vec2(-t.x, 0.0)), mr = lumAt(uv + vec2(t.x, 0.0));
    float bl = lumAt(uv + vec2(-t.x, -t.y)), bc = lumAt(uv + vec2(0.0, -t.y)), br = lumAt(uv + vec2(t.x, -t.y));
    float gx = -tl - 2.0 * ml - bl + tr + 2.0 * mr + br;
    float gy = -tl - 2.0 * tc - tr + bl + 2.0 * bc + br;
    float e = clamp(length(vec2(gx, gy)) * uP[0], 0.0, 1.0);
    vec4 c = src(uv);
    vec4 edge = vec4(vec3(e), 1.0) * max(c.a, e);
    return mix(c, edge, uP[1]);
})GLSL",
         {
             {"strength", "Kekuatan", ParamKind::Float, 1, {2.f, 0.f, 0.f, 0.f}, 0.f, 6.f},
             {"mix", "Campur", ParamKind::Float, 1, {1.f, 0.f, 0.f, 0.f}, 0.f, 1.f},
         }},
    };
    return kCatalog;
}

const std::vector<EffectDef>& effectCatalog() {
    static const std::vector<EffectDef> all = [] {
        std::vector<EffectDef> v = baseCatalog();
        const std::vector<EffectDef>& m = moreEffects();
        v.insert(v.end(), m.begin(), m.end());
        return v;
    }();
    return all;
}

const EffectDef* findEffectDef(const std::string& type) {
    for (const EffectDef& d : effectCatalog())
        if (type == d.type) return &d;
    return nullptr;
}

std::vector<float> defaultParams(const EffectDef& d) {
    std::vector<float> v;
    for (const ParamDef& p : d.params)
        for (int i = 0; i < p.comps; ++i) v.push_back(p.def[i]);
    return v;
}

std::string effectShaderSource(const EffectDef& d) {
    return std::string(kHeader) + d.body + "\n";
}

const char* effectVertexSource() { return kVertex; }

std::string describeCatalog() {
    std::string s;
    char buf[256];
    for (const EffectDef& d : effectCatalog()) {
        std::snprintf(buf, sizeof buf, "E|%s|%s|%s|%d\n", d.type, d.label, d.category, d.passes);
        s += buf;
        int flat = 0;
        for (const ParamDef& p : d.params) {
            static const char* axis2[] = {" X", " Y"};
            static const char* axis4[] = {" R", " G", " B", " A"};
            for (int i = 0; i < p.comps; ++i) {
                std::string label = p.label;
                if (p.comps == 2) label += axis2[i];
                if (p.comps == 4) label += axis4[i];
                int kind = p.kind == ParamKind::Int ? 1 : p.kind == ParamKind::Angle ? 2 : p.kind == ParamKind::Bool ? 3 : 0;
                std::snprintf(buf, sizeof buf, "P|%d|%s|%g|%g|%g|%d\n", flat++, label.c_str(), static_cast<double>(p.min),
                              static_cast<double>(p.max), static_cast<double>(p.def[i]), kind);
                s += buf;
            }
        }
    }
    return s;
}

}  // namespace studio
