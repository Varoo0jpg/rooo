#pragma once
// Model data proyek. Murni C++ (tanpa OpenGL/Android) supaya bisa diuji otomatis.
//
// Aturan emas untuk mencegah proyek rusak:
//  1. Semua perubahan lewat Document. Perubahan dibuat pada SALINAN proyek.
//  2. Salinan itu divalidasi penuh. Jika tidak valid, perubahan DITOLAK dan
//     proyek asli tidak tersentuh (jaminan "semua atau tidak sama sekali").
//  3. Undo/redo memakai snapshot utuh, bukan operasi kebalikan, sehingga tidak
//     ada kemungkinan undo "meleset" dari keadaan sebelumnya.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace studio {

using LayerId = uint32_t;
constexpr LayerId kRoot = 0;  // "induk" semu: layer di level paling atas

enum class LayerType : uint8_t { Shape = 0, Group = 1, Null = 2, Image = 3, Video = 4, Audio = 5, Text = 6 };

// Bentuk Shape (digambar di GPU sebagai signed-distance-field, tajam di zoom berapa pun).
enum class ShapeKind : uint8_t { Rect = 0, RoundRect, Ellipse, Polygon, Star, Count };

enum class BlendMode : uint8_t {
    Normal = 0, Add, Multiply, Screen, Overlay, Darken, Lighten, ColorDodge, ColorBurn,
    HardLight, SoftLight, Difference, Exclusion,
    LinearBurn, DarkerColor, LighterColor, VividLight, LinearLight, PinLight, HardMix, Subtract, Divide,
    Hue, Saturation, ColorMode, Luminosity, Count
};

struct Transform {
    float posX = 0, posY = 0, posZ = 0;
    float scaleX = 1, scaleY = 1;
    float rotation = 0;  // derajat, sumbu Z
    float opacity = 1;   // 0..1
    float anchorX = 0, anchorY = 0, anchorZ = 0;  // pivot, sumbu Z sudah ada sejak awal

    bool finite() const {
        return std::isfinite(posX) && std::isfinite(posY) && std::isfinite(posZ) &&
               std::isfinite(scaleX) && std::isfinite(scaleY) && std::isfinite(rotation) &&
               std::isfinite(opacity) && std::isfinite(anchorX) && std::isfinite(anchorY) &&
               std::isfinite(anchorZ);
    }
    bool operator==(const Transform&) const = default;
};

// Properti Transform yang bisa dianimasikan.
//
// "Prop" juga dipakai sebagai nomor KANAL animasi: nilai < kPropCount = properti Transform;
// nilai >= kEffectChannelBase = parameter efek (lihat effectChannel()). Dengan begitu semua operasi
// keyframe/graph editor bekerja sama untuk transformasi maupun parameter efek.
enum class Prop : uint16_t {
    PosX = 0, PosY, PosZ, ScaleX, ScaleY, Rotation, Opacity, AnchorX, AnchorY, AnchorZ,
    Time,   // kurva waktu (kecepatan): nilai = waktu-layer yang ditampilkan pada frame itu
    Count
};
constexpr int kPropCount = static_cast<int>(Prop::Count);
constexpr int kTransformProps = static_cast<int>(Prop::Time);   // jumlah properti yang ada di struct Transform

constexpr int kEffectChannelBase = 256;
constexpr int kMaxEffectParams = 32;
constexpr int kMaxEffects = 16;
constexpr size_t kMaxTextLen = 2000;
inline Prop effectChannel(int fx, int param) {
    return static_cast<Prop>(kEffectChannelBase + fx * kMaxEffectParams + param);
}
inline bool isEffectChannel(Prop p) { return static_cast<int>(p) >= kEffectChannelBase; }
inline int effectIndexOf(Prop p) { return (static_cast<int>(p) - kEffectChannelBase) / kMaxEffectParams; }
inline int effectParamOf(Prop p) { return (static_cast<int>(p) - kEffectChannelBase) % kMaxEffectParams; }

inline float getProp(const Transform& t, Prop p) {
    switch (p) {
        case Prop::PosX: return t.posX;
        case Prop::PosY: return t.posY;
        case Prop::PosZ: return t.posZ;
        case Prop::ScaleX: return t.scaleX;
        case Prop::ScaleY: return t.scaleY;
        case Prop::Rotation: return t.rotation;
        case Prop::Opacity: return t.opacity;
        case Prop::AnchorX: return t.anchorX;
        case Prop::AnchorY: return t.anchorY;
        case Prop::AnchorZ: return t.anchorZ;
        default: return 0.f;
    }
}
inline void setProp(Transform& t, Prop p, float v) {
    switch (p) {
        case Prop::PosX: t.posX = v; break;
        case Prop::PosY: t.posY = v; break;
        case Prop::PosZ: t.posZ = v; break;
        case Prop::ScaleX: t.scaleX = v; break;
        case Prop::ScaleY: t.scaleY = v; break;
        case Prop::Rotation: t.rotation = v; break;
        case Prop::Opacity: t.opacity = v; break;
        case Prop::AnchorX: t.anchorX = v; break;
        case Prop::AnchorY: t.anchorY = v; break;
        case Prop::AnchorZ: t.anchorZ = v; break;
        default: break;
    }
}

// Cara nilai bergerak dari keyframe ini ke keyframe berikutnya.
enum class Interp : uint8_t { Hold = 0, Linear = 1, Bezier = 2 };

// Kurva Bezier kubik antara dua keyframe (k0 -> k1), sumbu x dinormalisasi 0..1 sepanjang segmen:
//   P0 = (0, v0)   P1 = (k0.outX, v0 + k0.outY)   P2 = (1 - k1.inX, v1 + k1.inY)   P3 = (1, v1)
// outX/inX harus 0..1 (menjamin waktu selalu maju); outY/inY bebas (boleh overshoot).
struct Keyframe {
    int64_t frame = 0;
    float value = 0;
    Interp interp = Interp::Linear;  // untuk segmen KELUAR (menuju keyframe berikutnya)
    float outX = 1.f / 3.f, outY = 0;
    float inX = 1.f / 3.f, inY = 0;
    bool operator==(const Keyframe&) const = default;
};

// Semua keyframe satu properti; frame berurutan naik ketat; tidak pernah kosong.
struct Track {
    Prop prop = Prop::PosX;
    std::vector<Keyframe> keys;
    bool operator==(const Track&) const = default;
};

// Satu efek pada sebuah layer. `type` merujuk katalog efek (effects.h); params = nilai dasar
// (rata, semua komponen berurutan); tracks = animasi per parameter (Track.prop = indeks parameter).
struct Effect {
    std::string type;
    bool enabled = true;
    std::vector<float> params;
    std::vector<Track> tracks;
    bool operator==(const Effect&) const = default;
};

struct Layer {
    LayerId id = 0;
    std::string name;
    LayerType type = LayerType::Shape;
    LayerId parent = kRoot;  // kRoot atau id sebuah Group
    int64_t inFrame = 0;     // frame pertama layer terlihat
    int64_t outFrame = 1;    // frame pertama layer TIDAK terlihat (eksklusif)
    uint32_t color = 0xFFFFFFFFu;  // RGBA (0xRRGGBBAA)
    float width = 200;             // ukuran bentuk dalam piksel kanvas (Shape/Null)
    float height = 200;
    Transform transform;        // nilai dasar (dipakai jika properti tidak punya keyframe)
    std::vector<Track> tracks;  // terurut menurut prop, tanpa duplikat

    // Tampilan
    ShapeKind shape = ShapeKind::Rect;
    float cornerRadius = 0;      // RoundRect, dalam piksel
    int32_t sides = 5;           // Polygon/Star: jumlah sisi/ujung (3..64)
    float innerRatio = 0.5f;     // Star: 0.05..1 (1 = poligon biasa)
    BlendMode blend = BlendMode::Normal;
    // Masker (gaya Alight Motion): bila matte != 0, layer ini menjadi MASKER bagi layer tepat di bawahnya (indeks z - 1):
    // 1 = hanya bagian layer bawah yang menimpa bentuk masker yang terlihat, 2 = kebalikannya (dilubangi).
    // Layer masker tidak digambar sendiri. Hanya untuk layer yang digambar (bukan Grup/Null/Suara).
    uint8_t matte = 0;
    bool hidden = false;         // ikon mata di timeline: layer disembunyikan (tidak tampil di pratinjau maupun ekspor)
    std::string image;           // Image: nama file di folder media aplikasi (tanpa path)
    std::vector<Effect> effects; // diterapkan berurutan dari indeks 0

    // Teks (LayerType::Text): `text` = isi (UTF-8, boleh berbaris-baris), `fontSize` px proyek, `textStyle` bit0 tebal,
    // bit1 miring, bit2-3 keluarga huruf. Piksel teks dirender aplikasi menjadi gambar putih `image` (media/<image>)
    // lalu diwarnai oleh `color` seperti gambar biasa.
    std::string text;
    float fontSize = 96.f;
    uint8_t textStyle = 0;

    // Audio (LayerType::Audio): tidak digambar. `image` = nama klip suara (media/<klip>/audio.pcm); srcStart/srcCount/rate
    // dalam satuan frame proyek seperti video. Volume = opacity layer (boleh dikeyframe).
    // Video (LayerType::Video): `image` = nama folder proxy (satu JPEG per frame proyek di <folder>/NNNNNN.jpg).
    // Frame proxy yang tampil di waktu-layer t = srcStart + (t - inFrame) * rate, dijepit ke 0..srcCount-1.
    int64_t srcStart = 0;
    int64_t srcCount = 0;
    float rate = 1.f;

    bool operator==(const Layer&) const = default;
};

struct Project {
    int32_t width = 1080;
    int32_t height = 1920;
    int32_t fps = 30;
    int64_t durationFrames = 300;
    LayerId nextId = 1;
    std::vector<Layer> layers;  // urutan Z: indeks 0 = paling bawah

    bool operator==(const Project&) const = default;
};

// Indeks frame proxy video untuk waktu-layer `t` (sudah melalui kurva Waktu bila ada).
int64_t videoSrcIndex(const Layer& l, double t);
// Nama berkas proxy relatif terhadap folder media: "<folder>/<6 digit>.jpg".
std::string videoFrameName(const Layer& l, double t);
std::string videoFrameNameAt(const Layer& l, int64_t index);

// Pemeriksa konsistensi penuh. true jika proyek sehat; jika tidak, err berisi alasannya.
bool validate(const Project& p, std::string* err = nullptr);

// Katalog preset animasi. Kategori: 0 masuk, 1 keluar, 2 loop.
int presetCount();
const char* presetId(int i);
const char* presetLabel(int i);
int presetCategory(int i);

class Document {
public:
    explicit Document(Project p = Project{}, size_t maxUndo = 200);

    const Project& project() const { return project_; }
    const Layer* find(LayerId id) const;
    const std::string& lastError() const { return lastError_; }

    // Semua operasi: true/id jika berhasil, false/0 jika ditolak (proyek tidak berubah).
    LayerId addLayer(LayerType type, const std::string& name, int64_t inFrame, int64_t outFrame,
                     LayerId parent = kRoot);
    LayerId addLayer(const Layer& proto);               // id pada proto diabaikan; dibuat baru
    bool setSize(LayerId id, float width, float height);
    // Kelompokkan beberapa layer ke Grup baru (satu undo); mengembalikan id Grup atau 0. frame = waktu untuk menghitung pusat.
    LayerId groupLayers(const std::vector<LayerId>& ids, const std::string& name, double frame);
    bool ungroup(LayerId id);                           // bongkar Grup; isinya tetap ada
    bool removeLayer(LayerId id);                       // anak-anak sebuah Group ikut terhapus
    bool moveLayer(LayerId id, size_t newIndex);        // ubah urutan Z
    bool setParent(LayerId id, LayerId newParent);      // masuk/keluar Group
    bool setTransform(LayerId id, const Transform& t);  // nilai dasar (tanpa menyentuh keyframe)

    // --- Animasi (semua frame = frame proyek absolut) ---
    // Tambah/ganti keyframe; membuat track jika belum ada.
    bool setKey(LayerId id, Prop prop, int64_t frame, float value);
    // Beri keyframe di `frame` untuk semua properti pada mask bit (1 << prop), memakai nilai
    // terevaluasi saat ini (tidak mengubah tampilan). Satu langkah undo.
    bool keyProps(LayerId id, int64_t frame, uint32_t propMask);
    // Hapus keyframe. Jika track jadi kosong, nilai dasar = nilai keyframe terakhir itu.
    bool removeKey(LayerId id, Prop prop, int64_t frame);
    bool moveKey(LayerId id, Prop prop, int64_t frame, int64_t newFrame);  // tolak jika bentrok
    // Pindah frame DAN ubah nilai dalam satu langkah (satu undo). newFrame == frame boleh.
    bool editKey(LayerId id, Prop prop, int64_t frame, int64_t newFrame, float newValue);
    // Mengubah ke Bezier dari jenis lain memberi handle setara garis lurus (tidak ada lompatan bentuk).
    bool setKeyInterp(LayerId id, Prop prop, int64_t frame, Interp interp);
    // Atur kedua handle segmen yang DIMULAI di keyframe ini: handle keluar milik keyframe ini
    // (outX,outY) dan handle masuk milik keyframe berikutnya (inX,inY); segmen jadi Bezier.
    // outX/inX harus 0..1 (di luar itu ditolak oleh validasi).
    bool setKeyHandles(LayerId id, Prop prop, int64_t frame, float outX, float outY, float inX,
                       float inY);
    // Easing gaya CSS cubic-bezier(x1,y1,x2,y2) pada segmen yang dimulai di keyframe ini.
    bool setKeyEase(LayerId id, Prop prop, int64_t frame, float x1, float y1, float x2, float y2);
    // Gaya After Effects. mode: 0 = Easy Ease (kedua sisi datar), 1 = hanya sisi masuk, 2 = hanya sisi keluar,
    // 3 = Auto Bezier (tangen mengikuti keyframe tetangga → kurva mulus), 4 = Linear (kedua sisi lurus).
    // Segmen bukan-Bezier di sekitar keyframe otomatis diubah jadi Bezier setara garis lurus.
    bool easeKey(LayerId id, Prop prop, int64_t frame, int mode);
    // Tempel gaya dari layer `src` ke layer `id`. mask: 1 = warna & bentuk, 2 = blend & opasitas,
    // 4 = transform (posisi/skala/rotasi/anchor + animasinya), 8 = efek. Keyframe digeser mengikuti awal layer
    // tujuan dan yang jatuh di luar rentang layer tujuan dibuang.
    bool pasteStyle(LayerId id, const Layer& src, uint32_t mask);
    // Ubah kecepatan layer: durasi dan semua keyframe diskalakan terhadap awal layer (2.0 = dua kali lebih cepat).
    bool scaleLayerTime(LayerId id, double factor);
    // Hapus seluruh animasi satu properti; nilai dasar = nilai pada `frame`.
    bool clearTrack(LayerId id, Prop prop, int64_t frame);
    // --- Tampilan & efek ---
    bool setShape(LayerId id, ShapeKind kind, float cornerRadius, int sides, float innerRatio);
    bool setBlend(LayerId id, BlendMode mode);
    bool setMatte(LayerId id, int mode);
    bool setHidden(LayerId id, bool hidden);
    // Atur ulang satu kanal: hapus SEMUA keyframe-nya dan kembalikan nilai dasar ke `value`.
    bool resetProp(LayerId id, Prop prop, float value);   // 0..2, lihat Layer::matte
    // Preset animasi (lihat presetCount/presetLabel): masuk/keluar memakai jendela `step` frame di awal/akhir layer,
    // loop memakai `step` frame per setengah siklus. Keyframe preset menggantikan keyframe lama di jendelanya.
    bool applyPreset(LayerId id, int preset, int64_t step);
    bool setText(LayerId id, const std::string& image, float width, float height, const std::string& text, float fontSize,
                 uint8_t style);   // hanya layer Teks
    // Tambah efek di akhir daftar; `params` = nilai awal (dari katalog). Mengembalikan indeks efek, -1 jika gagal.
    int addEffect(LayerId id, const std::string& type, const std::vector<float>& params);
    bool removeEffect(LayerId id, int index);
    bool moveEffect(LayerId id, int index, int newIndex);
    bool setEffectEnabled(LayerId id, int index, bool enabled);
    // Seperti auto-key tetapi untuk kanal mana pun (transformasi atau parameter efek).
    bool setPropAt(LayerId id, Prop channel, int64_t frame, float value);
    // Edit seperti "auto-key": untuk tiap properti yang berbeda dari nilai terevaluasi di `frame`,
    // buat/ubah keyframe jika properti itu beranimasi, jika tidak ubah nilai dasar. Satu commit.
    bool setTransformAt(LayerId id, int64_t frame, const Transform& t);
    bool setColor(LayerId id, uint32_t rgba);
    bool renameLayer(LayerId id, const std::string& name);
    bool shiftLayer(LayerId id, int64_t deltaFrames);   // geser in & out bersama
    bool trimLayer(LayerId id, int64_t newIn, int64_t newOut);
    LayerId splitLayer(LayerId id, int64_t frame);      // potong; mengembalikan id potongan kanan

    // Gestur (mis. menggeser slider): semua edit di antara beginBatch/endBatch menjadi SATU langkah undo.
    void beginBatch();
    void endBatch();

    bool undo();
    bool redo();
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    size_t undoDepth() const { return undo_.size(); }
    size_t redoDepth() const { return redo_.size(); }
    // Naik setiap kali isi proyek benar-benar berubah (edit, undo, redo). Dipakai autosave.
    uint64_t revision() const { return revision_; }

private:
    bool commit(Project&& candidate);

    uint64_t revision_ = 0;
    bool batch_ = false;
    bool batchHas_ = false;
    Project batchBase_;
    Project project_;
    std::vector<Project> undo_;
    std::vector<Project> redo_;
    size_t maxUndo_;
    std::string lastError_;
};

}  // namespace studio
