// Harness desktop: menjalankan engine render ASLI (engine.cpp) di GPU perangkat lunak (Mesa llvmpipe)
// lewat EGL tanpa layar. Menghasilkan gambar PPM untuk diperiksa mata + pemeriksaan piksel otomatis.
//
// Jalankan:  EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 ./gl_harness <folder-keluaran>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

#include "effects.h"
#include "engine.h"
#include "model/model.h"
#include "model/scene.h"

typedef void* EGLDisplay;
typedef void* EGLConfig;
typedef void* EGLContext;
typedef int EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
extern "C" {
EGLDisplay eglGetDisplay(void*);
EGLBoolean eglInitialize(EGLDisplay, EGLint*, EGLint*);
EGLBoolean eglBindAPI(EGLenum);
EGLBoolean eglChooseConfig(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
EGLContext eglCreateContext(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
EGLBoolean eglMakeCurrent(EGLDisplay, void*, void*, EGLContext);
}

using namespace studio;

static int g_checks = 0, g_fail = 0;
#define CHECK(c)                                                         \
    do {                                                                 \
        ++g_checks;                                                      \
        if (!(c)) {                                                      \
            ++g_fail;                                                    \
            std::printf("GAGAL %s:%d  %s\n", __FILE__, __LINE__, #c);    \
        }                                                                \
    } while (0)

static Engine g_eng;
static std::string g_out = ".";

static bool initGL() {
    EGLDisplay d = eglGetDisplay(nullptr);
    EGLint ma, mi;
    if (!eglInitialize(d, &ma, &mi)) return false;
    eglBindAPI(0x30A0);
    EGLint ca[] = {0x3040, 0x40, 0x3038};
    EGLConfig cfg = nullptr;
    EGLint n = 0;
    eglChooseConfig(d, ca, &cfg, 1, &n);
    EGLint xa[] = {0x3098, 3, 0x3038};
    EGLContext c = eglCreateContext(d, cfg, nullptr, xa);
    if (!c) return false;
    return eglMakeCurrent(d, nullptr, nullptr, c) != 0;
}

static void writePPM(const std::string& name, const std::vector<uint8_t>& rgba, int w, int h) {
    FILE* f = std::fopen((g_out + "/" + name + ".ppm").c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (size_t i = 0; i < rgba.size(); i += 4) std::fwrite(&rgba[i], 1, 3, f);
    std::fclose(f);
}

// Gambar uji 256x256 (premultiplied): gradasi warna, grid, bentuk, tepi lembut beralfa.
static GLuint makeTestImage(int N) {
    std::vector<uint8_t> px(static_cast<size_t>(N) * N * 4);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            float u = float(x) / N, v = float(y) / N;
            float r = 0.5f + 0.5f * std::sin(6.28f * (u + 0.0f)), g = 0.5f + 0.5f * std::sin(6.28f * (u + 0.33f)),
                  b = 0.5f + 0.5f * std::sin(6.28f * (u + 0.66f));
            float lum = 0.35f + 0.65f * (1.f - v);
            r *= lum; g *= lum; b *= lum;
            if (x % 32 < 2 || y % 32 < 2) { r = g = b = 1.f; }
            float dx = x - N * 0.32f, dy = y - N * 0.35f;
            if (dx * dx + dy * dy < (N * 0.14f) * (N * 0.14f)) { r = 1; g = 0.1f; b = 0.1f; }
            if (x > N * 0.55f && x < N * 0.8f && y > N * 0.5f && y < N * 0.75f) { r = 0.1f; g = 0.9f; b = 0.2f; }
            if (std::abs((x - y) - 20) < 6) { r = g = b = 0.05f; }
            float cx = x - N * 0.5f, cy = y - N * 0.5f;
            float rad = std::sqrt(cx * cx + cy * cy);
            float a = std::fmin(1.f, std::fmax(0.f, (N * 0.46f - rad) / 3.f));
            uint8_t* p = &px[(static_cast<size_t>(y) * N + x) * 4];
            // baris 0 = atas gambar (sesuai Android Bitmap)
            p[0] = uint8_t(255.f * r * a); p[1] = uint8_t(255.f * g * a); p[2] = uint8_t(255.f * b * a); p[3] = uint8_t(255.f * a);
        }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

static std::vector<uint8_t> renderDoc(const Document& d, double frame, int w, int h) {
    std::vector<Resolved> sc;
    resolveScene(d.project(), frame, 0, nullptr, &sc);
    std::vector<uint8_t> px;
    CHECK(g_eng.renderToPixels(d.project(), sc, frame, w, h, &px));
    return px;
}

struct Px { int r, g, b, a; };
static Px at(const std::vector<uint8_t>& p, int w, int x, int y) {
    const uint8_t* q = &p[(static_cast<size_t>(y) * w + x) * 4];
    return {q[0], q[1], q[2], q[3]};
}
static bool near(Px a, int r, int g, int b, int tol = 3) {
    return std::abs(a.r - r) <= tol && std::abs(a.g - g) <= tol && std::abs(a.b - b) <= tol;
}

static Project smallProject() {
    Project p;
    p.width = 200; p.height = 200; p.durationFrames = 100;
    return p;
}

static Layer rectLayer(float w, float h, uint32_t color, float x = 0, float y = 0) {
    Layer l;
    l.name = "r"; l.inFrame = 0; l.outFrame = 100; l.width = w; l.height = h; l.color = color;
    l.transform.posX = x; l.transform.posY = y;
    return l;
}

static void numericChecks() {
    // 1) bentuk dasar + orientasi: kotak merah di kiri-atas kanvas harus tampil di kiri-atas gambar
    {
        Document d(smallProject());
        d.addLayer(rectLayer(60, 60, 0xFF0000FF, -70, -70));
        auto px = renderDoc(d, 0, 200, 200);
        CHECK(near(at(px, 200, 30, 30), 255, 0, 0));       // dalam kotak (kiri-atas)
        CHECK(near(at(px, 200, 170, 170), 33, 33, 41));    // latar
        CHECK(near(at(px, 200, 170, 30), 33, 33, 41));
        CHECK(near(at(px, 200, 30, 170), 33, 33, 41));
        writePPM("check_orient", px, 200, 200);
    }
    // 2) opasitas 0.5 di atas latar (0.13,0.13,0.16)
    {
        Document d(smallProject());
        Layer l = rectLayer(100, 100, 0xFF0000FF);
        l.transform.opacity = 0.5f;
        d.addLayer(l);
        auto px = renderDoc(d, 0, 200, 200);
        CHECK(near(at(px, 200, 100, 100), int(255 * 0.5 + 33 * 0.5 + 0.5), int(33 * 0.5 + 0.5), int(41 * 0.5 + 0.5), 3));
    }
    // 3) efek invert pada merah -> cyan; layer ber-efek masuk jalur penuh
    {
        Document d(smallProject());
        LayerId id = d.addLayer(rectLayer(100, 100, 0xFF0000FF));
        d.addEffect(id, "invert", defaultParams(*findEffectDef("invert")));
        auto px = renderDoc(d, 0, 200, 200);
        CHECK(near(at(px, 200, 100, 100), 0, 255, 255));
        CHECK(near(at(px, 200, 5, 5), 33, 33, 41));
    }
    // 4) blend Multiply: merah murni x latar -> (0.13*1, 0, 0)
    {
        Document d(smallProject());
        Layer l = rectLayer(100, 100, 0xFF0000FF);
        l.blend = BlendMode::Multiply;
        d.addLayer(l);
        auto px = renderDoc(d, 0, 200, 200);
        CHECK(near(at(px, 200, 100, 100), 33, 0, 0));
    }
    // 5) blend Add: merah + latar
    {
        Document d(smallProject());
        Layer l = rectLayer(100, 100, 0xFF0000FF);
        l.blend = BlendMode::Add;
        d.addLayer(l);
        auto px = renderDoc(d, 0, 200, 200);
        CHECK(near(at(px, 200, 100, 100), 255, 33, 41));
    }
    // 6) Screen: biru 0.5 di atas latar
    {
        Document d(smallProject());
        Layer l = rectLayer(100, 100, 0x000080FF);
        l.blend = BlendMode::Screen;
        d.addLayer(l);
        auto px = renderDoc(d, 0, 200, 200);
        // screen: cb + cs - cb*cs ; biru: 0.16 + 0.5 - 0.08 = 0.58 -> 148
        CHECK(near(at(px, 200, 100, 100), 33, 33, 148, 3));
    }
    // 7) Blur: tepi kotak jadi bertahap
    {
        Document d(smallProject());
        LayerId id = d.addLayer(rectLayer(100, 100, 0xFFFFFFFF));
        d.addEffect(id, "gaussian_blur", {16.f});
        auto px = renderDoc(d, 0, 200, 200);
        Px edge = at(px, 200, 50, 100);  // tepat di tepi kiri kotak
        CHECK(edge.r > 90 && edge.r < 190);
        CHECK(near(at(px, 200, 100, 100), 255, 255, 255));
    }
    // 8) animasi parameter efek: fill warna dianimasikan lewat kanal (R: 0 -> 1 antara frame 0..10)
    {
        Document d(smallProject());
        LayerId id = d.addLayer(rectLayer(100, 100, 0xFFFFFFFF));
        d.addEffect(id, "fill", {0.f, 0.f, 0.f, 1.f});
        d.setKey(id, effectChannel(0, 0), 0, 0.f);
        d.setKey(id, effectChannel(0, 0), 10, 1.f);
        auto p0 = renderDoc(d, 0, 200, 200);
        auto p5 = renderDoc(d, 5, 200, 200);
        auto p10 = renderDoc(d, 10, 200, 200);
        CHECK(near(at(p0, 200, 100, 100), 0, 0, 0));
        CHECK(near(at(p5, 200, 100, 100), 128, 0, 0, 3));
        CHECK(near(at(p10, 200, 100, 100), 255, 0, 0));
    }
    // 9) efek dinonaktifkan tidak berpengaruh
    {
        Document d(smallProject());
        LayerId id = d.addLayer(rectLayer(100, 100, 0xFF0000FF));
        d.addEffect(id, "invert", {1.f});
        d.setEffectEnabled(id, 0, false);
        auto px = renderDoc(d, 0, 200, 200);
        CHECK(near(at(px, 200, 100, 100), 255, 0, 0));
    }
    // 10) efek tidak dikenal dilewati tanpa crash
    {
        Document d(smallProject());
        LayerId id = d.addLayer(rectLayer(100, 100, 0xFF0000FF));
        d.addEffect(id, "efek_tidak_ada", {1.f});
        auto px = renderDoc(d, 0, 200, 200);
        CHECK(near(at(px, 200, 100, 100), 255, 0, 0));
    }
    // 11) bentuk ellipse: pusat terisi, sudut kosong
    {
        Document d(smallProject());
        Layer l = rectLayer(160, 160, 0x00FF00FF);
        l.shape = ShapeKind::Ellipse;
        d.addLayer(l);
        auto px = renderDoc(d, 0, 200, 200);
        CHECK(near(at(px, 200, 100, 100), 0, 255, 0));
        CHECK(near(at(px, 200, 28, 28), 33, 33, 41));
    }
    // 11b) masker (gaya Alight Motion): layer atas = masker bagi layer tepat di bawahnya; masker tidak tampil sendiri
    {
        Document d(smallProject());
        const LayerId base = d.addLayer(rectLayer(200, 200, 0xFF0000FF));      // merah penuh (target)
        Layer m = rectLayer(100, 100, 0x00FF00FF);                              // hijau, elips (masker)
        m.shape = ShapeKind::Ellipse;
        const LayerId mask = d.addLayer(m);
        (void)base;
        CHECK(d.setMatte(mask, 1));
        auto px = renderDoc(d, 0, 200, 200);
        CHECK(near(at(px, 200, 100, 100), 255, 0, 0));      // di dalam elips: merah target, bukan hijau masker
        CHECK(near(at(px, 200, 8, 8), 33, 33, 41));         // di luar elips: latar
        CHECK(d.setMatte(mask, 2));
        px = renderDoc(d, 0, 200, 200);
        CHECK(near(at(px, 200, 100, 100), 33, 33, 41));     // terbalik: dilubangi
        CHECK(near(at(px, 200, 8, 8), 255, 0, 0));
        CHECK(d.setMatte(mask, 0));
        px = renderDoc(d, 0, 200, 200);
        CHECK(near(at(px, 200, 100, 100), 0, 255, 0));      // tanpa masker: elips hijau tampil di atas
        CHECK(near(at(px, 200, 8, 8), 255, 0, 0));
    }
    // 12) grup: anak ikut memutar (90 derajat) -> kotak lebar jadi tinggi
    {
        Document d(smallProject());
        Layer g;
        g.name = "g"; g.type = LayerType::Group; g.inFrame = 0; g.outFrame = 100; g.transform.rotation = 90;
        LayerId gid = d.addLayer(g);
        Layer c = rectLayer(120, 20, 0xFFFF00FF);
        c.parent = gid;
        d.addLayer(c);
        auto px = renderDoc(d, 0, 200, 200);
        CHECK(near(at(px, 200, 100, 50), 255, 255, 0));   // sekarang vertikal
        CHECK(near(at(px, 200, 50, 100), 33, 33, 41));
    }
}

static void gallery(const char* prefix, Project proj, const std::vector<std::string>& types, GLuint imgTex, const char* imgName, double frame) {
    (void)imgTex;
    for (const std::string& t : types) {
        const EffectDef* def = findEffectDef(t);
        Document d(proj);
        Layer bg = rectLayer(float(proj.width), float(proj.height), 0x24304AFF);
        d.addLayer(bg);
        Layer im;
        im.name = "gambar"; im.type = LayerType::Image; im.image = imgName; im.inFrame = 0; im.outFrame = 100;
        im.width = 200; im.height = 200;
        LayerId id = d.addLayer(im);
        if (def) CHECK(d.addEffect(id, t, defaultParams(*def)) == 0);
        auto px = renderDoc(d, frame, proj.width, proj.height);
        writePPM(std::string(prefix) + t, px, proj.width, proj.height);
    }
}

int main(int argc, char** argv) {
    if (argc > 1) g_out = argv[1];
    if (!initGL()) { std::printf("GAGAL: tidak bisa membuat konteks GL\n"); return 2; }
    if (!g_eng.init()) { std::printf("GAGAL: engine.init()\n%s\n", g_eng.diagnostics().c_str()); return 2; }
    g_eng.runSelfTest();
    std::printf("%s", g_eng.diagnostics().c_str());

    GLuint tex = makeTestImage(256);
    g_eng.setImageTexture("test.png", tex, 256, 256);

    numericChecks();

    // Galeri efek: tiap efek pada gambar uji (bingkai 10 supaya efek berbasis waktu terlihat)
    Project proj;
    proj.width = 256; proj.height = 256; proj.durationFrames = 100;
    std::vector<std::string> types = {"none"};
    for (const EffectDef& d : effectCatalog()) types.push_back(d.type);
    gallery("fx_", proj, types, tex, "test.png", 10);

    // Galeri bentuk
    {
        Project sp;
        sp.width = 256; sp.height = 256; sp.durationFrames = 100;
        const struct { ShapeKind k; float corner; int sides; float inner; const char* n; } shapes[] = {
            {ShapeKind::Rect, 0, 5, 0.5f, "rect"}, {ShapeKind::RoundRect, 40, 5, 0.5f, "round"},
            {ShapeKind::Ellipse, 0, 5, 0.5f, "ellipse"}, {ShapeKind::Polygon, 0, 6, 0.5f, "polygon6"},
            {ShapeKind::Polygon, 0, 3, 0.5f, "triangle"}, {ShapeKind::Star, 0, 5, 0.45f, "star5"},
            {ShapeKind::Star, 0, 8, 0.7f, "star8"}, {ShapeKind::Star, 0, 12, 0.3f, "star12"}};
        for (const auto& s : shapes) {
            Document d(sp);
            Layer l = rectLayer(200, 200, 0xFFB030FF);
            l.shape = s.k; l.cornerRadius = s.corner; l.sides = s.sides; l.innerRatio = s.inner;
            d.addLayer(l);
            writePPM(std::string("shape_") + s.n, renderDoc(d, 0, 256, 256), 256, 256);
        }
        // bentuk non-seragam (lebar 220 x tinggi 100) agar skala tidak rusak
        Document d(sp);
        Layer l = rectLayer(220, 100, 0x30B0FFFF);
        l.shape = ShapeKind::Star; l.sides = 5; l.innerRatio = 0.5f;
        l.transform.rotation = 20;
        d.addLayer(l);
        writePPM("shape_star_wide", renderDoc(d, 0, 256, 256), 256, 256);
    }

    // Galeri blend: latar gambar uji + lingkaran oranye dengan tiap blend mode
    {
        Project bp;
        bp.width = 256; bp.height = 256; bp.durationFrames = 100;
        const char* names[] = {"normal", "add", "multiply", "screen", "overlay", "darken", "lighten", "colordodge", "colorburn", "hardlight", "softlight", "difference", "exclusion",
                               "linearburn", "darkercolor", "lightercolor", "vividlight", "linearlight", "pinlight", "hardmix", "subtract", "divide", "hue", "saturation", "color", "luminosity"};
        for (int m = 0; m < 26; ++m) {
            Document d(bp);
            Layer im;
            im.name = "g"; im.type = LayerType::Image; im.image = "test.png"; im.inFrame = 0; im.outFrame = 100;
            im.width = 256; im.height = 256;
            d.addLayer(im);
            Layer c = rectLayer(150, 150, 0xFF8030FF);
            c.shape = ShapeKind::Ellipse;
            c.blend = static_cast<BlendMode>(m);
            c.transform.opacity = 0.9f;
            d.addLayer(c);
            const auto img = renderDoc(d, 0, 256, 256);
            if (m >= 13) {   // mode baru harus terkompilasi dan menghasilkan gambar tidak kosong
                long sum = 0;
                for (auto v : img) sum += v;
                CHECK(sum > 1000);
            }
            writePPM(std::string("blend_") + names[m], img, 256, 256);
        }
    }

    {   // Thumbnail efek untuk menu pemilihan
        for (int si = 0; si < 5; ++si) {
            FILE* sf = std::fopen((g_out + "/thumb_src_" + std::to_string(si) + ".rgba").c_str(), "rb");
            if (sf) {
                std::vector<uint8_t> raw(512 * 512 * 4);
                if (std::fread(raw.data(), 1, raw.size(), sf) == raw.size()) g_eng.setThumbSource(si, raw.data(), 512, 512);
                std::fclose(sf);
            }
        }
        g_eng.requestThumbs();
        studio::App a;
        a.open(g_out + "/thumb_app.studio");
        g_eng.resize(64, 64);
        a.setViewSize(64, 64);
        g_eng.render(a, 0);
        CHECK(g_eng.thumbsReady());
        const int S = Engine::kThumbSize;
        std::vector<std::string> ts = {"none"};
        for (const EffectDef& d : effectCatalog()) ts.push_back(d.type);
        const int cols = 8, rows = (static_cast<int>(ts.size()) + cols - 1) / cols;
        std::vector<uint8_t> sheet(static_cast<size_t>(cols) * S * rows * S * 4, 0);
        int missing = 0;
        for (size_t i = 0; i < ts.size(); ++i) {
            std::vector<uint8_t> t;
            if (!g_eng.thumb(ts[i], &t) || t.size() != static_cast<size_t>(S) * S * 4) { ++missing; continue; }
            const int cx = static_cast<int>(i) % cols, cy = static_cast<int>(i) / cols;
            for (int y = 0; y < S; ++y)
                std::memcpy(&sheet[((static_cast<size_t>(cy) * S + y) * cols * S + static_cast<size_t>(cx) * S) * 4],
                            &t[static_cast<size_t>(y) * S * 4], static_cast<size_t>(S) * 4);
        }
        CHECK(missing == 0);
        writePPM("thumbs", sheet, cols * S, rows * S);
    }

    // Ekspor: render demo -> NV12 (ukuran benar, tidak hitam semua, ganjil ditolak)
    {
        Project demo = makeDemoProject();
        std::vector<uint8_t> nv;
        CHECK(g_eng.renderFrameNV12(demo, 60.0, 270, 480, &nv));
        CHECK(nv.size() == static_cast<size_t>(270) * 480 * 3 / 2);
        size_t bright = 0;
        for (size_t i = 0; i < 270u * 480u; ++i) bright += nv[i] > 40;
        CHECK(bright > 1000);
        CHECK(!g_eng.renderFrameNV12(demo, 60.0, 271, 480, &nv));
        FILE* f = std::fopen((g_out + "/frame.nv12").c_str(), "wb");
        if (f) { std::fwrite(nv.data(), 1, nv.size(), f); std::fclose(f); }
    }

    std::printf("%s", g_eng.diagnostics().c_str());
    std::printf("harness: %d pemeriksaan, %d gagal\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
