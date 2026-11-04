#pragma once

#include <GLES3/gl3.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "app.h"
#include "effects.h"
#include "model/model.h"
#include "model/scene.h"

// Engine render (OpenGL ES 3.0). Tidak menyimpan state proyek: menggambar adegan yang diberikan.
//
// Alur satu frame:
//   - tiap layer Shape/Image digambar (bentuk = SDF di fragment shader; gambar = tekstur);
//   - layer tanpa efek & blend Normal/Add digambar langsung ke buffer akumulasi (cepat);
//   - selain itu: digambar ke buffer layer, lewat rantai efek (ping-pong), lalu dikomposit
//     dengan blend mode-nya;
//   - hasil dikirim ke layar (atau dibaca ke memori untuk tes/ekspor).
// Shader efek dikompilasi saat pertama dipakai; efek yang gagal dikompilasi dicatat di
// diagnostik dan dilewati (tidak membuat aplikasi crash).
class Engine {
public:
    bool init();  // dipanggil setiap konteks GL baru dibuat (semua ID lama dianggap hilang)
    void resize(int width, int height);

    // Gambar ke layar (framebuffer yang sedang aktif / screenFbo) dari keadaan App.
    void render(const studio::App& app, int64_t timeMs);

    // Gambar satu frame ke memori (RGBA8, baris pertama = bagian ATAS gambar). Untuk tes & ekspor.
    // Render satu frame proyek pada ukuran w x h (genap) lalu ubah ke NV12 untuk encoder. false jika GPU belum siap.
    bool renderFrameNV12(const studio::Project& p, double frame, int w, int h, std::vector<uint8_t>* nv12);
    bool renderToPixels(const studio::Project& p, const std::vector<studio::Resolved>& scene,
                        double frame, int w, int h, std::vector<uint8_t>* rgba);

    // Framebuffer tujuan layar (0 untuk Android; berbeda saat dijalankan di tes desktop).
    void setScreenFbo(GLuint fbo) { screenFbo_ = fbo; }

    // Diagnostik shader/efek (teks multi-baris) dan uji kompilasi semua efek di GPU ini.
    std::string diagnostics() const;
    void requestSelfTest() { selfTestRequested_ = true; }
    void runSelfTest();

    // Gambar contoh efek (thumbnail 180x180 RGBA) untuk menu pemilihan efek. Dibuat di thread GL
    // pada frame render berikutnya setelah requestThumbs(); thumbsReady() menandai selesai.
    static constexpr int kThumbSize = 180;
    // Foto sumber contoh efek (RGBA, baris pertama = atas). Jika tidak diberikan dipakai gambar prosedural.
    // Gambar contoh ke-idx (0..15); tiap kategori efek memakai satu gambar secara bergilir.
    void setThumbSource(int idx, const uint8_t* rgba, int w, int h);
    void requestThumbs() { if (!thumbsDone_) thumbsRequested_ = true; }
    bool thumbsReady() const { return thumbsDone_; }
    bool thumb(const std::string& type, std::vector<uint8_t>* rgba) const;

    // Gambar: engine meminta tekstur lewat nextImageRequest(); pemanggil (thread GL) mengunggah
    // piksel lalu memanggil setImageTexture. tex == 0 menandai gagal dimuat.
    std::string nextImageRequest();
    void setImageTexture(const std::string& name, GLuint tex, int w, int h);
    // Ekspor: nama frame proxy video yang dibutuhkan pada `frame` tetapi belum ada teksturnya.
    std::vector<std::string> missingVideoFrames(const studio::Project& p, double frame) const;
    int videoTextureCount() const;

private:
    struct Prog {
        GLuint id = 0;
        GLint u[16];
        bool failed = false;
    };
    struct Target {
        GLuint fbo = 0, tex = 0;
        int w = 0, h = 0;
    };
    struct ImgTex {
        GLuint tex = 0;
        int w = 0, h = 0;
        bool failed = false;
        uint64_t retryAt = 0;   // frame video yang belum jadi dicoba lagi setelah tick ini
    };

    GLuint compile(GLenum type, const char* src, const std::string& what);
    GLuint link(const char* vs, const char* fs, const std::string& what);
    void addDiag(const std::string& line);
    Prog* effectProgram(const studio::EffectDef& d);
    bool buildPrograms();
    void ensureTarget(Target& t, int w, int h);
    void freeTarget(Target& t);
    void bindTarget(const Target& t, bool clear);
    void drawFullscreen();
    void drawLayer(const studio::Layer& l, const studio::Affine& view, const studio::Affine& world,
                   float opacity);
    void blit(GLuint tex, float opacity);
    Target* renderScene(const studio::Project& p, const std::vector<studio::Resolved>& scene,
                        double frame, int w, int h);

    GLuint vao_ = 0, vbo_ = 0;
    Prog layerProg_, compProg_, blitProg_, matteProg_, outlineProg_;
    std::map<std::string, Prog> effectProgs_;
    Target accum_[2], work_[2], layerBuf_, matteBuf_;
    int accumCur_ = 0;
    std::map<std::string, ImgTex> images_;
    std::set<std::string> requested_;
    std::vector<std::string> pendingImages_;
    std::map<std::string, uint64_t> videoUse_;      // frame video yang tertanam di GPU -> stempel pemakaian terakhir (LRU)
    std::map<uint32_t, std::string> videoShown_;    // layer -> frame terakhir yang tampil (dipakai bila frame baru belum siap)
    uint64_t useStamp_ = 0;
    uint64_t drawTick_ = 0;
    double curFrame_ = 0;
    static constexpr size_t kMaxVideoTex = 48;
    void requestImage(const std::string& name);
    std::vector<std::string> diag_;
    std::set<std::string> diagOnce_;
    bool selfTestRequested_ = false;
    bool thumbsRequested_ = false;
    bool thumbsDone_ = false;
    struct ThumbSrc { std::vector<uint8_t> px; int w = 0, h = 0; };
    std::vector<ThumbSrc> thumbSrcs_;
    std::map<std::string, std::vector<uint8_t>> thumbs_;
    void buildThumbs();
    GLuint screenFbo_ = 0;
    int width_ = 1, height_ = 1;

    int64_t lastLogMs_ = 0;
    int frames_ = 0;
    std::string glRenderer_, glVersion_;
};
