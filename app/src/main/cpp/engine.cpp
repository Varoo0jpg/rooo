#include "engine.h"
#include "export.h"

#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "effects.h"
#include "model/anim.h"

#define LOG_TAG "StudioEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace studio;

namespace {

// Persegi satuan (-0.5..0.5) -> ukuran layer -> matriks (kanvas->piksel buffer) -> NDC.
// Sumbu Y buffer menghadap ke bawah (asal kiri-atas), NDC y dibalik sehingga baris atas kanvas = v tinggi.
const char* kLayerVS = R"(#version 300 es
layout(location = 0) in vec2 aPos;
uniform vec4 uAB;     // a, b, c, d
uniform vec2 uT;      // tx, ty (piksel buffer)
uniform vec2 uSize;   // ukuran lokal layer
uniform vec2 uView;   // ukuran buffer
out vec2 vUv;
void main() {
    vec2 l = aPos * uSize;
    vec2 p = vec2(uAB.x * l.x + uAB.z * l.y + uT.x, uAB.y * l.x + uAB.w * l.y + uT.y);
    vUv = aPos + 0.5;
    gl_Position = vec4(p.x / uView.x * 2.0 - 1.0, 1.0 - p.y / uView.y * 2.0, 0.0, 1.0);
}
)";

const char* kLayerFS = R"(#version 300 es
precision highp float;
precision highp int;
in vec2 vUv;
uniform vec2 uSize;
uniform vec4 uColor;     // rgba lurus
uniform float uOpacity;
uniform int uKind;       // 0 bentuk, 1 gambar, 2 pengganti gambar
uniform int uShape;      // ShapeKind
uniform float uCorner;
uniform int uSides;
uniform float uInner;
uniform sampler2D uImage;
out vec4 outColor;

float sdBox(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}
float sdPoly(vec2 p, int n) {
    float fn = float(n);
    float a = atan(p.x, -p.y);
    float s = 6.2831853 / fn;
    float k = floor(0.5 + a / s) * s - a;
    return length(p) * cos(k) - cos(3.14159265 / fn);
}
float sdStar(vec2 p, float r, int n, float m) {
    float an = 3.141593 / float(n);
    float en = 3.141593 / m;
    vec2 acs = vec2(cos(an), sin(an));
    vec2 ecs = vec2(cos(en), sin(en));
    float bn = mod(atan(p.x, p.y), 2.0 * an) - an;
    p = length(p) * vec2(cos(bn), abs(sin(bn)));
    p -= r * acs;
    p += ecs * clamp(-dot(p, ecs), 0.0, r * acs.y / ecs.y);
    return length(p) * sign(p.x);
}

void main() {
    if (uKind == 0) {
        vec2 hs = 0.5 * uSize;
        vec2 p = (vUv - 0.5) * uSize;
        float sd;
        if (uShape == 0) {
            sd = sdBox(p, hs);
        } else if (uShape == 1) {
            float r = min(uCorner, min(hs.x, hs.y));
            sd = sdBox(p, hs - vec2(r)) - r;
        } else if (uShape == 2) {
            sd = (length(p / hs) - 1.0) * min(hs.x, hs.y);
        } else if (uShape == 3) {
            vec2 q = p / hs;
            sd = sdPoly(q, uSides) * min(hs.x, hs.y);
        } else {
            vec2 q = p / hs;
            float m = 2.0 + (float(uSides) - 2.0) * uInner;
            sd = sdStar(vec2(q.x, -q.y), 1.0, uSides, m) * min(hs.x, hs.y);
        }
        float w = max(fwidth(sd), 1.0e-5);
        float cov = clamp(0.5 - sd / w, 0.0, 1.0);
        float a = uColor.a * cov;
        outColor = vec4(uColor.rgb * a, a) * uOpacity;
    } else if (uKind == 1) {
        vec4 t = texture(uImage, vUv);
        outColor = vec4(t.rgb * uColor.rgb, t.a) * uColor.a * uOpacity;
    } else {
        vec2 c = floor(vUv * 8.0);
        float k = mod(c.x + c.y, 2.0);
        vec3 col = mix(vec3(0.35), vec3(0.55), k);
        outColor = vec4(col, 1.0) * uOpacity;
    }
}
)";

const char* kQuadVS = R"(#version 300 es
layout(location = 0) in vec2 aPos;
out vec2 vUv;
void main() {
    vUv = aPos + 0.5;
    gl_Position = vec4(aPos * 2.0, 0.0, 1.0);
}
)";

const char* kBlitFS = R"(#version 300 es
precision highp float;
in vec2 vUv;
uniform sampler2D uSrc;
uniform float uOpacity;
out vec4 outColor;
void main() { outColor = texture(uSrc, vUv) * uOpacity; }
)";

const char* kMatteFS = R"(#version 300 es
precision highp float;
in vec2 vUv;
uniform sampler2D uSrc;
uniform sampler2D uMatte;
uniform float uInv;
out vec4 outColor;
void main() {
    float a = texture(uMatte, vUv).a;
    outColor = texture(uSrc, vUv) * mix(a, 1.0 - a, uInv);
}
)";

const char* kCompFS = R"(#version 300 es
precision highp float;
precision highp int;
in vec2 vUv;
uniform sampler2D uDst;
uniform sampler2D uSrc;
uniform float uOpacity;
uniform int uMode;
out vec4 outColor;

float softL(float cb, float cs) {
    if (cs <= 0.5) return cb - (1.0 - 2.0 * cs) * cb * (1.0 - cb);
    float d = (cb <= 0.25) ? ((16.0 * cb - 12.0) * cb + 4.0) * cb : sqrt(cb);
    return cb + (2.0 * cs - 1.0) * (d - cb);
}
vec3 hardLight(vec3 cb, vec3 cs) {
    return mix(cb * 2.0 * cs, 1.0 - 2.0 * (1.0 - cb) * (1.0 - cs), step(0.5, cs));
}
float lumOf(vec3 c) { return dot(c, vec3(0.3, 0.59, 0.11)); }
vec3 clipColor(vec3 c) {
    float l = lumOf(c);
    float n = min(min(c.r, c.g), c.b);
    float x = max(max(c.r, c.g), c.b);
    if (n < 0.0) c = l + (c - l) * l / max(l - n, 1.0e-5);
    if (x > 1.0) c = l + (c - l) * (1.0 - l) / max(x - l, 1.0e-5);
    return c;
}
vec3 setLum(vec3 c, float l) { return clipColor(c + (l - lumOf(c))); }
float satOf(vec3 c) { return max(max(c.r, c.g), c.b) - min(min(c.r, c.g), c.b); }
vec3 setSat(vec3 c, float s) {
    float mn = min(min(c.r, c.g), c.b);
    float mx = max(max(c.r, c.g), c.b);
    if (mx <= mn) return vec3(0.0);
    return (c - mn) * s / (mx - mn);
}
float vividCh(float b, float s) {
    if (s <= 0.5) { float s2 = 2.0 * s; return s2 <= 0.0 ? 0.0 : 1.0 - min((1.0 - b) / s2, 1.0); }
    float s2 = 2.0 * (s - 0.5); return s2 >= 1.0 ? 1.0 : min(b / (1.0 - s2), 1.0);
}
float pinCh(float b, float s) { return s < 0.5 ? min(b, 2.0 * s) : max(b, 2.0 * (s - 0.5)); }
vec3 blendFn(int m, vec3 cb, vec3 cs) {
    if (m == 13) return max(cb + cs - 1.0, 0.0);
    if (m == 14) return lumOf(cs) < lumOf(cb) ? cs : cb;
    if (m == 15) return lumOf(cs) > lumOf(cb) ? cs : cb;
    if (m == 16) return vec3(vividCh(cb.r, cs.r), vividCh(cb.g, cs.g), vividCh(cb.b, cs.b));
    if (m == 17) return clamp(cb + 2.0 * cs - 1.0, 0.0, 1.0);
    if (m == 18) return vec3(pinCh(cb.r, cs.r), pinCh(cb.g, cs.g), pinCh(cb.b, cs.b));
    if (m == 19) return step(vec3(1.0), cb + cs);
    if (m == 20) return max(cb - cs, 0.0);
    if (m == 21) return min(cb / max(cs, vec3(1.0e-4)), 1.0);
    if (m == 22) return setLum(setSat(cs, satOf(cb)), lumOf(cb));
    if (m == 23) return setLum(setSat(cb, satOf(cs)), lumOf(cb));
    if (m == 24) return setLum(cs, lumOf(cb));
    if (m == 25) return setLum(cb, lumOf(cs));
    if (m == 1) return min(cb + cs, 1.0);
    if (m == 2) return cb * cs;
    if (m == 3) return cb + cs - cb * cs;
    if (m == 4) return hardLight(cs, cb);
    if (m == 5) return min(cb, cs);
    if (m == 6) return max(cb, cs);
    if (m == 7) return mix(min(cb / max(1.0 - cs, 1.0e-4), 1.0), vec3(0.0), step(cb, vec3(0.0)));
    if (m == 8) return mix(1.0 - min((1.0 - cb) / max(cs, 1.0e-4), 1.0), vec3(1.0), step(vec3(1.0), cb));
    if (m == 9) return hardLight(cb, cs);
    if (m == 10) return vec3(softL(cb.r, cs.r), softL(cb.g, cs.g), softL(cb.b, cs.b));
    if (m == 11) return abs(cb - cs);
    if (m == 12) return cb + cs - 2.0 * cb * cs;
    return cs;
}
void main() {
    vec4 s = texture(uSrc, vUv) * uOpacity;
    vec4 d = texture(uDst, vUv);
    vec3 cs = s.a > 0.0001 ? s.rgb / s.a : vec3(0.0);
    vec3 cb = d.a > 0.0001 ? d.rgb / d.a : vec3(0.0);
    vec3 B = blendFn(uMode, cb, cs);
    float ao = s.a + d.a * (1.0 - s.a);
    vec3 co = (1.0 - d.a) * s.rgb + (1.0 - s.a) * d.rgb + s.a * d.a * B;
    outColor = vec4(co, ao);
}
)";

const char* kOutlineFS = R"(#version 300 es
precision highp float;
in vec2 vUv;
uniform vec4 uColor;
uniform vec2 uSizePx;
out vec4 outColor;
void main() {
    float d = min(min(vUv.x, 1.0 - vUv.x) * uSizePx.x, min(vUv.y, 1.0 - vUv.y) * uSizePx.y);
    if (d > 3.0) discard;
    outColor = uColor;
}
)";

}  // namespace

static bool drawable(const Layer& l) {
    return l.type != LayerType::Group && l.type != LayerType::Null && l.type != LayerType::Audio;
}

// Apakah layer i adalah masker yang sah bagi layer di bawahnya?
static bool isMaskFor(const Project& p, size_t i) {
    const Layer& m = p.layers[i];
    if (m.matte == 0 || i == 0 || !drawable(m)) return false;
    const Layer& t = p.layers[i - 1];
    return drawable(t) && t.matte == 0;
}

void Engine::addDiag(const std::string& line) {
    LOGE("%s", line.c_str());
    if (diag_.size() < 400) diag_.push_back(line);
}

std::string Engine::diagnostics() const {
    std::string s = "GPU: " + glRenderer_ + "\nGL: " + glVersion_ + "\n";
    if (diag_.empty()) return s + "(tidak ada pesan)\n";
    for (const auto& l : diag_) s += l + "\n";
    return s;
}

GLuint Engine::compile(GLenum type, const char* src, const std::string& what) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        addDiag("GAGAL kompilasi " + what + ": " + buf);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint Engine::link(const char* vs, const char* fs, const std::string& what) {
    GLuint v = compile(GL_VERTEX_SHADER, vs, what + " (vertex)");
    GLuint f = compile(GL_FRAGMENT_SHADER, fs, what + " (fragment)");
    if (!v || !f) {
        if (v) glDeleteShader(v);
        if (f) glDeleteShader(f);
        return 0;
    }
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        glGetProgramInfoLog(p, sizeof(buf), nullptr, buf);
        addDiag("GAGAL link " + what + ": " + buf);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

bool Engine::buildPrograms() {
    auto loc = [](GLuint p, const char* n) { return glGetUniformLocation(p, n); };
    layerProg_ = Prog{};
    layerProg_.id = link(kLayerVS, kLayerFS, "layer");
    if (!layerProg_.id) return false;
    {
        GLuint p = layerProg_.id;
        const char* names[] = {"uAB", "uT", "uSize", "uView", "uColor", "uOpacity", "uKind",
                               "uShape", "uCorner", "uSides", "uInner", "uImage"};
        for (int i = 0; i < 12; ++i) layerProg_.u[i] = loc(p, names[i]);
    }
    compProg_ = Prog{};
    compProg_.id = link(kQuadVS, kCompFS, "komposit");
    if (compProg_.id) {
        compProg_.u[0] = loc(compProg_.id, "uDst");
        compProg_.u[1] = loc(compProg_.id, "uSrc");
        compProg_.u[2] = loc(compProg_.id, "uOpacity");
        compProg_.u[3] = loc(compProg_.id, "uMode");
    }
    blitProg_ = Prog{};
    blitProg_.id = link(kQuadVS, kBlitFS, "blit");
    if (blitProg_.id) {
        blitProg_.u[0] = loc(blitProg_.id, "uSrc");
        blitProg_.u[1] = loc(blitProg_.id, "uOpacity");
    }
    matteProg_ = Prog{};
    matteProg_.id = link(kQuadVS, kMatteFS, "matte");
    if (matteProg_.id) {
        matteProg_.u[0] = loc(matteProg_.id, "uSrc");
        matteProg_.u[1] = loc(matteProg_.id, "uMatte");
        matteProg_.u[2] = loc(matteProg_.id, "uInv");
    }
    outlineProg_ = Prog{};
    outlineProg_.id = link(kLayerVS, kOutlineFS, "garis-tepi");
    if (outlineProg_.id) {
        const char* names[] = {"uAB", "uT", "uSize", "uView", "uColor", "uSizePx"};
        for (int i = 0; i < 6; ++i) outlineProg_.u[i] = loc(outlineProg_.id, names[i]);
    }
    return layerProg_.id && compProg_.id && blitProg_.id && matteProg_.id && outlineProg_.id;
}

Engine::Prog* Engine::effectProgram(const EffectDef& d) {
    auto it = effectProgs_.find(d.type);
    if (it != effectProgs_.end()) return it->second.failed ? nullptr : &it->second;
    Prog pr;
    const std::string src = effectShaderSource(d);
    pr.id = link(effectVertexSource(), src.c_str(), std::string("efek ") + d.type);
    if (!pr.id) {
        pr.failed = true;
        effectProgs_[d.type] = pr;
        return nullptr;
    }
    const char* names[] = {"uTex", "uSize", "uTime", "uPass", "uP"};
    for (int i = 0; i < 5; ++i) pr.u[i] = glGetUniformLocation(pr.id, names[i]);
    if (pr.u[4] < 0) pr.u[4] = glGetUniformLocation(pr.id, "uP[0]");
    effectProgs_[d.type] = pr;
    return &effectProgs_[d.type];
}

void Engine::runSelfTest() {
    selfTestRequested_ = false;
    int ok = 0, bad = 0;
    for (const EffectDef& d : effectCatalog()) {
        auto it = effectProgs_.find(d.type);
        const bool had = it != effectProgs_.end();
        Prog* p = effectProgram(d);
        if (p) ++ok; else ++bad;
        (void)had;
    }
    char buf[160];
    std::snprintf(buf, sizeof buf, "UJI KOMPILASI EFEK: %d berhasil, %d gagal (dari %zu)", ok, bad,
                  effectCatalog().size());
    diag_.push_back(buf);
    LOGI("%s", buf);
}

bool Engine::init() {
    glRenderer_ = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    glVersion_ = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    LOGI("GL_RENDERER: %s", glRenderer_.c_str());
    LOGI("GL_VERSION : %s", glVersion_.c_str());

    // Konteks baru: semua objek GL lama sudah tidak berlaku.
    effectProgs_.clear();
    images_.clear();
    requested_.clear();
    pendingImages_.clear();
    videoUse_.clear();
    videoShown_.clear();
    for (Target* t : {&accum_[0], &accum_[1], &work_[0], &work_[1], &layerBuf_, &matteBuf_}) *t = Target{};

    if (!buildPrograms()) return false;

    static const float quad[] = {-0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);
    return true;
}

void Engine::resize(int width, int height) {
    width_ = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;
}

void Engine::freeTarget(Target& t) {
    if (t.fbo) glDeleteFramebuffers(1, &t.fbo);
    if (t.tex) glDeleteTextures(1, &t.tex);
    t = Target{};
}

void Engine::ensureTarget(Target& t, int w, int h) {
    if (t.fbo && t.w == w && t.h == h) return;
    freeTarget(t);
    glGenTextures(1, &t.tex);
    glBindTexture(GL_TEXTURE_2D, t.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &t.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        addDiag("Framebuffer tidak lengkap " + std::to_string(w) + "x" + std::to_string(h));
    t.w = w;
    t.h = h;
}

void Engine::bindTarget(const Target& t, bool clear) {
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glViewport(0, 0, t.w, t.h);
    if (clear) {
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

void Engine::drawFullscreen() {
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void Engine::blit(GLuint tex, float opacity) {
    glUseProgram(blitProg_.id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(blitProg_.u[0], 0);
    glUniform1f(blitProg_.u[1], opacity);
    drawFullscreen();
}

void Engine::drawLayer(const Layer& l, const Affine& view, const Affine& world, float opacity) {
    const Affine v = view * world;
    glUseProgram(layerProg_.id);
    const GLint* u = layerProg_.u;
    glUniform4f(u[0], v.a, v.b, v.c, v.d);
    glUniform2f(u[1], v.tx, v.ty);
    glUniform2f(u[2], l.width, l.height);
    const float cr = static_cast<float>((l.color >> 24) & 0xFF) / 255.f;
    const float cg = static_cast<float>((l.color >> 16) & 0xFF) / 255.f;
    const float cb = static_cast<float>((l.color >> 8) & 0xFF) / 255.f;
    const float ca = static_cast<float>(l.color & 0xFF) / 255.f;
    glUniform4f(u[4], cr, cg, cb, ca);
    glUniform1f(u[5], opacity);
    int kind = 0;
    if (l.type == LayerType::Image || l.type == LayerType::Video || l.type == LayerType::Text) {
        std::string key = l.image;
        if (l.type == LayerType::Video) {
            const double lt = layerTime(l, curFrame_);
            const int64_t idx = videoSrcIndex(l, lt);
            key = videoFrameNameAt(l, idx);
            // ambil beberapa frame berikutnya lebih dulu supaya pemutaran lancar
            for (int k = 1; k <= 3; ++k) {
                const std::string nx = videoFrameNameAt(l, idx + static_cast<int64_t>(std::llround(k * static_cast<double>(l.rate)) + (l.rate < 1.f ? 1 : 0)));
                if (images_.find(nx) == images_.end()) requestImage(nx);
            }
        }
        auto it = images_.find(key);
        if (l.type == LayerType::Video && it != images_.end() && it->second.failed && drawTick_ >= it->second.retryAt) {
            // frame proxy belum selesai dibuat di latar: coba minta lagi
            images_.erase(it);
            requested_.erase(key);
            videoUse_.erase(key);
            it = images_.end();
        }
        if ((it == images_.end() || !it->second.tex || it->second.failed) && l.type == LayerType::Video) {
            // frame belum siap: tampilkan frame terakhir yang ada agar tidak berkedip
            requestImage(key);
            auto sh = videoShown_.find(l.id);
            if (sh != videoShown_.end()) {
                auto it2 = images_.find(sh->second);
                if (it2 != images_.end() && it2->second.tex && !it2->second.failed) { it = it2; key = sh->second; }
            }
        }
        if (it != images_.end() && it->second.tex && !it->second.failed) {
            kind = 1;
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, it->second.tex);
            glUniform1i(u[11], 1);
            if (l.type == LayerType::Video) {
                videoUse_[key] = ++useStamp_;
                videoShown_[l.id] = key;
            }
        } else {
            kind = 2;
            if ((l.type == LayerType::Image || l.type == LayerType::Text) && it == images_.end() && requested_.insert(l.image).second)
                pendingImages_.push_back(l.image);
        }
    }
    glUniform1i(u[6], kind);
    glUniform1i(u[7], static_cast<int>(l.shape));
    glUniform1f(u[8], l.cornerRadius);
    glUniform1i(u[9], l.sides);
    glUniform1f(u[10], l.innerRatio);
    drawFullscreen();
    glActiveTexture(GL_TEXTURE0);
}

void Engine::requestImage(const std::string& name) {
    if (!requested_.insert(name).second) return;
    pendingImages_.push_back(name);
    // saat menggeser cepat, buang permintaan lama yang sudah tidak relevan
    while (pendingImages_.size() > 24) {
        requested_.erase(pendingImages_.front());
        pendingImages_.erase(pendingImages_.begin());
    }
}

int Engine::videoTextureCount() const { return static_cast<int>(videoUse_.size()); }

std::vector<std::string> Engine::missingVideoFrames(const Project& p, double frame) const {
    std::vector<std::string> out;
    for (const Layer& l : p.layers) {
        if (l.type != LayerType::Video) continue;
        if (frame < static_cast<double>(l.inFrame) || frame >= static_cast<double>(l.outFrame)) continue;
        const std::string n = videoFrameName(l, layerTime(l, frame));
        auto it = images_.find(n);
        if (it == images_.end() || !it->second.tex) out.push_back(n);
    }
    return out;
}

std::string Engine::nextImageRequest() {
    if (pendingImages_.empty()) return std::string();
    std::string s = pendingImages_.back();
    pendingImages_.pop_back();
    return s;
}

void Engine::setImageTexture(const std::string& name, GLuint tex, int w, int h) {
    ImgTex t;
    t.tex = tex;
    t.w = w;
    t.h = h;
    t.failed = (tex == 0);
    t.retryAt = drawTick_ + 20;
    if (t.failed && name.find('/') == std::string::npos) addDiag("Gagal memuat gambar: " + name);
    images_[name] = t;
    if (name.find('/') != std::string::npos) {   // frame video: batasi jumlah tekstur (buang yang paling lama tak terpakai)
        videoUse_[name] = ++useStamp_;
        while (videoUse_.size() > kMaxVideoTex) {
            auto oldest = videoUse_.begin();
            for (auto i = videoUse_.begin(); i != videoUse_.end(); ++i)
                if (i->second < oldest->second) oldest = i;
            if (oldest->first == name) break;
            auto im = images_.find(oldest->first);
            if (im != images_.end()) {
                if (im->second.tex) glDeleteTextures(1, &im->second.tex);
                images_.erase(im);
            }
            requested_.erase(oldest->first);
            videoUse_.erase(oldest);
        }
    }
}

Engine::Target* Engine::renderScene(const Project& p, const std::vector<Resolved>& scene,
                                    double frame, int w, int h) {
    curFrame_ = frame;
    ++drawTick_;
    for (int i = 0; i < 2; ++i) {
        ensureTarget(accum_[i], w, h);
        ensureTarget(work_[i], w, h);
    }
    ensureTarget(layerBuf_, w, h);
    ensureTarget(matteBuf_, w, h);
    accumCur_ = 0;

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    bindTarget(accum_[accumCur_], false);
    glClearColor(0.13f, 0.13f, 0.16f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    Affine view;  // kanvas -> piksel buffer
    const float s = static_cast<float>(w) / static_cast<float>(p.width);
    view.a = s;
    view.d = s;
    view.tx = static_cast<float>(w) * 0.5f;
    view.ty = static_cast<float>(h) * 0.5f;

    glUseProgram(layerProg_.id);
    glUniform2f(layerProg_.u[3], static_cast<float>(w), static_cast<float>(h));

    const double fps = p.fps > 0 ? p.fps : 30;
    for (size_t i = 0; i < p.layers.size() && i < scene.size(); ++i) {
        const Layer& l = p.layers[i];
        const Resolved& r = scene[i];
        if (!r.visible || r.opacity <= 0.f) continue;
        if (!drawable(l)) continue;
        // Layer masker (matte != 0) tidak digambar sendiri: ia memotong layer tepat di bawahnya, asal layer itu
        // digambar dan bukan masker juga (bila tidak, flag diabaikan agar layer tidak hilang).
        if (isMaskFor(p, i)) continue;
        const size_t mi = i + 1;
        const bool useMatte = mi < p.layers.size() && mi < scene.size() && isMaskFor(p, mi);

        // Efek yang bisa dipakai (katalog dikenal & shader berhasil dikompilasi)
        struct Active { const EffectDef* def; Prog* prog; const Effect* fx; };
        std::vector<Active> chain;
        for (const Effect& e : l.effects) {
            if (!e.enabled) continue;
            const EffectDef* d = findEffectDef(e.type);
            if (!d) {
                if (diagOnce_.insert("tipe:" + e.type).second) addDiag("Efek tidak dikenal: " + e.type);
                continue;
            }
            Prog* pr = effectProgram(*d);
            if (pr) chain.push_back({d, pr, &e});
        }
        const bool fast = chain.empty() && !useMatte && (l.blend == BlendMode::Normal || l.blend == BlendMode::Add);

        if (fast) {
            bindTarget(accum_[accumCur_], false);
            glEnable(GL_BLEND);
            if (l.blend == BlendMode::Add) glBlendFunc(GL_ONE, GL_ONE);
            else glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            drawLayer(l, view, r.world, r.opacity);
            glDisable(GL_BLEND);
            continue;
        }

        // Jalur penuh: layer -> buffer layer -> rantai efek -> komposit
        bindTarget(layerBuf_, true);
        glDisable(GL_BLEND);
        drawLayer(l, view, r.world, 1.f);
        GLuint curTex = layerBuf_.tex;
        int w0 = 0;
        if (useMatte) {   // alfa layer sumber -> buffer matte (kosong bila sumber tersembunyi)
            const Resolved& mr = scene[mi];
            bindTarget(matteBuf_, true);
            if (mr.visible && mr.opacity > 0.f) drawLayer(p.layers[mi], view, mr.world, mr.opacity);
        }
        for (const Active& a : chain) {
            float params[kMaxEffectParams];
            evalEffectParams(*a.fx, layerTime(l, frame), params);
            glUseProgram(a.prog->id);
            glUniform1i(a.prog->u[0], 0);
            glUniform2f(a.prog->u[1], static_cast<float>(w), static_cast<float>(h));
            glUniform1f(a.prog->u[2], static_cast<float>(frame / fps));
            if (a.prog->u[4] >= 0) glUniform1fv(a.prog->u[4], kMaxEffectParams, params);
            for (int pass = 0; pass < a.def->passes; ++pass) {
                Target& dst = work_[w0];
                w0 ^= 1;
                bindTarget(dst, true);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, curTex);
                glUniform1i(a.prog->u[3], pass);
                drawFullscreen();
                curTex = dst.tex;
            }
        }
        if (useMatte) {
            Target& dst = work_[w0];
            w0 ^= 1;
            bindTarget(dst, true);
            glUseProgram(matteProg_.id);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, curTex);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, matteBuf_.tex);
            glUniform1i(matteProg_.u[0], 0);
            glUniform1i(matteProg_.u[1], 1);
            glUniform1f(matteProg_.u[2], p.layers[mi].matte == 2 ? 1.f : 0.f);
            drawFullscreen();
            glActiveTexture(GL_TEXTURE0);
            curTex = dst.tex;
        }
        // Komposit hasil ke akumulasi
        if (l.blend == BlendMode::Normal || l.blend == BlendMode::Add) {
            bindTarget(accum_[accumCur_], false);
            glEnable(GL_BLEND);
            if (l.blend == BlendMode::Add) glBlendFunc(GL_ONE, GL_ONE);
            else glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            blit(curTex, r.opacity);
            glDisable(GL_BLEND);
        } else {
            const int next = accumCur_ ^ 1;
            bindTarget(accum_[next], false);
            glUseProgram(compProg_.id);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, accum_[accumCur_].tex);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, curTex);
            glUniform1i(compProg_.u[0], 0);
            glUniform1i(compProg_.u[1], 1);
            glUniform1f(compProg_.u[2], r.opacity);
            glUniform1i(compProg_.u[3], static_cast<int>(l.blend));
            drawFullscreen();
            glActiveTexture(GL_TEXTURE0);
            accumCur_ = next;
        }
        glUseProgram(layerProg_.id);
    }
    return &accum_[accumCur_];
}

bool Engine::renderToPixels(const Project& p, const std::vector<Resolved>& scene, double frame, int w,
                            int h, std::vector<uint8_t>* rgba) {
    if (!layerProg_.id) return false;
    Target* t = renderScene(p, scene, frame, w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
    std::vector<uint8_t> raw(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());
    rgba->resize(raw.size());
    for (int y = 0; y < h; ++y)  // balik vertikal: GL membaca dari bawah
        std::memcpy(rgba->data() + static_cast<size_t>(y) * w * 4,
                    raw.data() + static_cast<size_t>(h - 1 - y) * w * 4, static_cast<size_t>(w) * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, screenFbo_);
    return true;
}


bool Engine::renderFrameNV12(const Project& p, double frame, int w, int h, std::vector<uint8_t>* nv12) {
    if (w <= 0 || h <= 0 || (w & 1) || (h & 1)) return false;
    std::vector<Resolved> scene;
    resolveScene(p, frame, 0, nullptr, &scene);
    std::vector<uint8_t> rgba;
    if (!renderToPixels(p, scene, frame, w, h, &rgba)) return false;
    return rgbaToNV12(rgba.data(), w, h, nv12);
}

// ---------- Thumbnail efek ----------
namespace {
// Gambar contoh prosedural 512x512: gradien, kisi putih, lingkaran, kotak bulat, segitiga, batang hitam.
std::vector<uint8_t> makeThumbSource(int n) {
    std::vector<uint8_t> px(static_cast<size_t>(n) * n * 4);
    auto mix = [](float a, float b, float t) { return a + (b - a) * t; };
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            const float u = static_cast<float>(x) / n, v = static_cast<float>(y) / n;
            const float t = (u + v) * 0.5f;
            float r = t < 0.5f ? mix(0.10f, 0.55f, t * 2) : mix(0.55f, 1.0f, (t - 0.5f) * 2);
            float g = t < 0.5f ? mix(0.18f, 0.20f, t * 2) : mix(0.20f, 0.62f, (t - 0.5f) * 2);
            float b = t < 0.5f ? mix(0.50f, 0.85f, t * 2) : mix(0.85f, 0.35f, (t - 0.5f) * 2);
            const int cell = n / 8;
            if (x % cell < 3 || y % cell < 3) { r = mix(r, 1, 0.55f); g = mix(g, 1, 0.55f); b = mix(b, 1, 0.55f); }
            auto fx = [&](float px_, float py_) { return std::make_pair(x - px_ * n, y - py_ * n); };
            {   // lingkaran merah
                auto d = fx(0.30f, 0.34f);
                if (std::sqrt(d.first * d.first + d.second * d.second) < 0.17f * n) { r = 1.f; g = 0.30f; b = 0.32f; }
            }
            {   // kotak bulat hijau
                auto d = fx(0.70f, 0.32f);
                const float hx = 0.14f * n, rr = 0.05f * n;
                const float qx = std::fabs(d.first) - hx + rr, qy = std::fabs(d.second) - hx + rr;
                const float dist = std::sqrt(std::max(qx, 0.f) * std::max(qx, 0.f) + std::max(qy, 0.f) * std::max(qy, 0.f)) +
                                   std::min(std::max(qx, qy), 0.f) - rr;
                if (dist < 0) { r = 0.25f; g = 0.95f; b = 0.50f; }
            }
            {   // segitiga kuning
                const float cx = 0.32f * n, top = 0.58f * n, bot = 0.88f * n, half = 0.17f * n;
                if (y > top && y < bot) {
                    const float k = (y - top) / (bot - top);
                    if (std::fabs(x - cx) < half * k) { r = 1.f; g = 0.85f; b = 0.25f; }
                }
            }
            {   // batang hitam diagonal tipis + kotak putih
                const float dd = std::fabs((x - 0.55f * n) * 0.7071f - (y - 0.78f * n) * 0.7071f);
                if (dd < 0.018f * n && u > 0.45f && u < 0.95f) { r = 0.05f; g = 0.05f; b = 0.08f; }
                if (x > 0.74f * n && x < 0.9f * n && y > 0.62f * n && y < 0.78f * n) { r = 1.f; g = 1.f; b = 1.f; }
            }
            uint8_t* o = &px[(static_cast<size_t>(y) * n + x) * 4];
            o[0] = static_cast<uint8_t>(std::clamp(r, 0.f, 1.f) * 255.f);
            o[1] = static_cast<uint8_t>(std::clamp(g, 0.f, 1.f) * 255.f);
            o[2] = static_cast<uint8_t>(std::clamp(b, 0.f, 1.f) * 255.f);
            o[3] = 255;
        }
    return px;
}

// Nilai awal yang terlalu "netral" diganti supaya efeknya terlihat di contoh.
void thumbOverrides(const std::string& type, std::vector<float>* v) {
    auto set = [&](size_t i, float x) { if (i < v->size()) (*v)[i] = x; };
    if (type == "brightness_contrast") { set(0, 0.2f); set(1, 0.35f); }
    else if (type == "hue_saturation") { set(0, 120.f); set(1, 0.3f); }
    else if (type == "levels") { set(0, 0.2f); set(1, 0.8f); set(2, 1.6f); }
    else if (type == "linear_wipe" || type == "radial_wipe" || type == "iris" || type == "blinds" ||
             type == "pixel_dissolve" || type == "noise_dissolve") set(0, 0.5f);
}
}  // namespace

void Engine::setThumbSource(int idx, const uint8_t* rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0 || w > 4096 || h > 4096 || idx < 0 || idx >= 16) return;
    if (static_cast<size_t>(idx) >= thumbSrcs_.size()) thumbSrcs_.resize(static_cast<size_t>(idx) + 1);
    ThumbSrc& t = thumbSrcs_[static_cast<size_t>(idx)];
    t.px.assign(rgba, rgba + static_cast<size_t>(w) * h * 4);
    t.w = w;
    t.h = h;
}

void Engine::buildThumbs() {
    thumbs_.clear();
    constexpr int kSrc = 512, kCanvas = 1080;
    const int sz = kThumbSize;  // 1080 / 180 = 6

    // Sumber gambar contoh: tiap kategori efek memakai gambar yang berbeda (bergilir).
    struct Tex { GLuint id = 0; std::string name; int w = 0, h = 0; };
    std::vector<Tex> texs;
    std::vector<ThumbSrc> srcs;
    for (const ThumbSrc& t : thumbSrcs_) if (!t.px.empty()) srcs.push_back(t);
    if (srcs.empty()) {
        ThumbSrc t;
        t.px = makeThumbSource(kSrc);
        t.w = t.h = kSrc;
        srcs.push_back(std::move(t));
    }
    for (size_t i = 0; i < srcs.size(); ++i) {
        Tex t;
        t.name = "__thumb_src" + std::to_string(i);
        t.w = srcs[i].w;
        t.h = srcs[i].h;
        glGenTextures(1, &t.id);
        glBindTexture(GL_TEXTURE_2D, t.id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, t.w, t.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, srcs[i].px.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        setImageTexture(t.name, t.id, t.w, t.h);
        texs.push_back(t);
    }
    std::map<std::string, size_t> catIndex;
    for (const EffectDef& d : effectCatalog()) catIndex.emplace(d.category, catIndex.size());

    std::vector<std::string> types = {""};  // "" = tanpa efek (asli)
    for (const EffectDef& d : effectCatalog()) types.push_back(d.type);
    for (const std::string& type : types) {
        size_t si = 0;
        if (!type.empty()) {
            const EffectDef* d0 = findEffectDef(type);
            if (!d0) continue;
            si = catIndex[d0->category] % texs.size();
        }
        Project p;
        p.width = p.height = kCanvas;
        p.fps = 30;
        p.durationFrames = 30;
        Layer l;
        l.id = 1;
        l.name = "thumb";
        l.type = LayerType::Image;
        l.image = texs[si].name;
        l.inFrame = 0;
        l.outFrame = 30;
        l.width = l.height = static_cast<float>(kCanvas);
        if (!type.empty()) {
            const EffectDef* d = findEffectDef(type);
            Effect e;
            e.type = type;
            e.params = defaultParams(*d);
            thumbOverrides(type, &e.params);
            l.effects.push_back(e);
        }
        p.layers.push_back(l);
        p.nextId = 2;
        std::vector<Resolved> scene;
        resolveScene(p, 0.0, 0, nullptr, &scene);
        std::vector<uint8_t> full;
        if (!renderToPixels(p, scene, 0.0, kCanvas, kCanvas, &full)) continue;
        std::vector<uint8_t> small(static_cast<size_t>(sz) * sz * 4);
        const int f = kCanvas / sz;
        for (int y = 0; y < sz; ++y)
            for (int x = 0; x < sz; ++x)
                for (int c = 0; c < 4; ++c) {
                    int sum = 0;
                    for (int dy = 0; dy < f; ++dy)
                        for (int dx = 0; dx < f; ++dx)
                            sum += full[((static_cast<size_t>(y) * f + dy) * kCanvas + (static_cast<size_t>(x) * f + dx)) * 4 + c];
                    small[(static_cast<size_t>(y) * sz + x) * 4 + c] = static_cast<uint8_t>(sum / (f * f));
                }
        thumbs_[type.empty() ? "none" : type] = std::move(small);
    }
    for (const Tex& t : texs) {
        images_.erase(t.name);
        glDeleteTextures(1, &t.id);
    }
    thumbsDone_ = true;
    addDiag("Contoh efek siap: " + std::to_string(thumbs_.size()));
}

bool Engine::thumb(const std::string& type, std::vector<uint8_t>* rgba) const {
    auto it = thumbs_.find(type);
    if (it == thumbs_.end()) return false;
    *rgba = it->second;
    return true;
}

void Engine::render(const App& app, int64_t timeMs) {
    if (selfTestRequested_) runSelfTest();
    if (thumbsRequested_ && layerProg_.id) {
        thumbsRequested_ = false;
        buildThumbs();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, screenFbo_);
    glViewport(0, 0, width_, height_);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.07f, 0.07f, 0.09f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!layerProg_.id) return;

    const Project& p = app.project();
    std::vector<Resolved> scene;
    app.sceneNow(&scene);

    const float s = app.viewScale();                // pas-layar x zoom
    const float panX = app.viewPanX(), panY = app.viewPanY();
    const int cw = std::max(1, static_cast<int>(std::lround(p.width * s)));
    const int ch = std::max(1, static_cast<int>(std::lround(p.height * s)));
    // Resolusi render dibatasi (zoom besar tidak menambah beban); hasilnya diregangkan saat ditampilkan.
    const float cap = 2048.f / static_cast<float>(std::max(cw, ch));
    const int rw = cap < 1.f ? std::max(1, static_cast<int>(std::lround(cw * cap))) : cw;
    const int rh = cap < 1.f ? std::max(1, static_cast<int>(std::lround(ch * cap))) : ch;
    Target* t = renderScene(p, scene, static_cast<double>(app.frame()), rw, rh);

    // Kirim ke layar: di tengah, digeser pan (y layar ke bawah, y GL ke atas)
    glBindFramebuffer(GL_FRAMEBUFFER, screenFbo_);
    const int x0 = static_cast<int>(std::lround((width_ - cw) * 0.5f + panX));
    const int y0 = static_cast<int>(std::lround((height_ - ch) * 0.5f - panY));
    glViewport(x0, y0, cw, ch);
    blit(t->tex, 1.f);

    // Garis tepi layer terpilih, di ruang layar
    glViewport(0, 0, width_, height_);
    const LayerId sel = app.selected();
    if (sel != 0 && outlineProg_.id) {
        Affine view;
        view.a = s;
        view.d = s;
        view.tx = width_ * 0.5f + panX;
        view.ty = height_ * 0.5f + panY;
        for (size_t i = 0; i < p.layers.size() && i < scene.size(); ++i) {
            const Layer& l = p.layers[i];
            if (l.id != sel || !scene[i].visible) continue;
            if (l.type == LayerType::Group || l.type == LayerType::Null || l.type == LayerType::Audio) continue;
            const Affine v = view * scene[i].world;
            glUseProgram(outlineProg_.id);
            const GLint* u = outlineProg_.u;
            glUniform4f(u[0], v.a, v.b, v.c, v.d);
            glUniform2f(u[1], v.tx, v.ty);
            glUniform2f(u[2], l.width, l.height);
            glUniform2f(u[3], static_cast<float>(width_), static_cast<float>(height_));
            glUniform4f(u[4], 1.f, 1.f, 1.f, 1.f);
            glUniform2f(u[5], std::hypot(v.a, v.b) * l.width, std::hypot(v.c, v.d) * l.height);
            drawFullscreen();
        }
    }

    // Gizmo: 4 pegangan skala di sudut + 1 pegangan putar (kotak kecil via scissor, tanpa shader tambahan)
    if (sel != 0) {
        float g[12];
        if (app.gizmoPoints(g)) {
            glEnable(GL_SCISSOR_TEST);
            auto box = [&](float cx, float cy, int px, float r, float gg, float b, float inner) {
                const int sx = static_cast<int>(width_ * 0.5f + panX + cx * s);
                const int sy = static_cast<int>(height_ - (height_ * 0.5f + panY + cy * s));
                glScissor(sx - px, sy - px, px * 2, px * 2);
                glClearColor(1.f, 1.f, 1.f, 1.f);
                glClear(GL_COLOR_BUFFER_BIT);
                const int q = static_cast<int>(px * inner);
                glScissor(sx - q, sy - q, q * 2, q * 2);
                glClearColor(r, gg, b, 1.f);
                glClear(GL_COLOR_BUFFER_BIT);
            };
            for (int k = 0; k < 4; ++k) box(g[2 + k * 2], g[3 + k * 2], 11, 0.49f, 0.42f, 1.f, 0.62f);
            box(g[10], g[11], 14, 1.f, 0.82f, 0.31f, 0.62f);
            glDisable(GL_SCISSOR_TEST);
        }
    }

    // Garis bantu: kotak-kotak persegi seragam (10 petak pada sisi pendek kanvas), di dalam area kanvas
    if (app.showGrid()) {
        glEnable(GL_SCISSOR_TEST);
        glClearColor(0.62f, 0.62f, 0.7f, 1.f);
        const int gx0 = std::max(x0, 0), gx1 = std::min(x0 + cw, width_);
        const int gy0 = std::max(y0, 0), gy1 = std::min(y0 + ch, height_);
        if (gx1 > gx0 && gy1 > gy0 && cw > 10 && ch > 10) {
            const float base = static_cast<float>(std::min(cw, ch)) / 10.f;     // sisi petak
            const int nx = std::max(1, static_cast<int>(std::lround(cw / base)));
            const int ny = std::max(1, static_cast<int>(std::lround(ch / base)));
            for (int k = 1; k < nx; ++k) {
                const int vx = x0 + static_cast<int>(std::lround(cw * static_cast<double>(k) / nx));
                if (vx >= gx0 && vx < gx1) { glScissor(vx, gy0, 1, gy1 - gy0); glClear(GL_COLOR_BUFFER_BIT); }
            }
            for (int k = 1; k < ny; ++k) {
                const int vy = y0 + static_cast<int>(std::lround(ch * static_cast<double>(k) / ny));
                if (vy >= gy0 && vy < gy1) { glScissor(gx0, vy, gx1 - gx0, 1); glClear(GL_COLOR_BUFFER_BIT); }
            }
        }
        glDisable(GL_SCISSOR_TEST);
    }

    // log FPS rata-rata tiap ~2 detik
    frames_++;
    if (timeMs - lastLogMs_ >= 2000) {
        double fps = frames_ * 1000.0 / static_cast<double>(timeMs - lastLogMs_);
        LOGI("FPS rata-rata: %.1f", fps);
        frames_ = 0;
        lastLogMs_ = timeMs;
    }
}
