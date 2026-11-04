#include "model/model.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "model/anim.h"

namespace studio {

namespace {
constexpr size_t kNpos = static_cast<size_t>(-1);
constexpr size_t kMaxNameLen = 256;
constexpr size_t kMaxKeysPerTrack = 100000;
constexpr int64_t kMaxKeyFrame = INT64_C(1) << 40;
constexpr float kMaxHandleY = 1e9f;
constexpr size_t kMaxImageName = 128;
constexpr size_t kMaxEffectType = 48;

size_t indexOf(const Project& p, LayerId id) {
    for (size_t i = 0; i < p.layers.size(); ++i)
        if (p.layers[i].id == id) return i;
    return kNpos;
}

}  // namespace

int64_t videoSrcIndex(const Layer& l, double t) {
    const double idx = static_cast<double>(l.srcStart) + std::floor((t - static_cast<double>(l.inFrame)) * static_cast<double>(l.rate));
    const int64_t hi = l.srcCount > 0 ? l.srcCount - 1 : 0;
    if (!(idx > 0.0)) return 0;
    if (idx >= static_cast<double>(hi)) return hi;
    return static_cast<int64_t>(idx);
}

std::string videoFrameNameAt(const Layer& l, int64_t index) {
    const int64_t hi = l.srcCount > 0 ? l.srcCount - 1 : 0;
    if (index < 0) index = 0;
    if (index > hi) index = hi;
    char buf[32];
    std::snprintf(buf, sizeof buf, "/%06lld.jpg", static_cast<long long>(index));
    return l.image + buf;
}

std::string videoFrameName(const Layer& l, double t) { return videoFrameNameAt(l, videoSrcIndex(l, t)); }

namespace {

bool validImageName(const std::string& s) {
    if (s.empty() || s.size() > kMaxImageName) return false;
    if (s == "." || s == "..") return false;
    for (unsigned char ch : s) {
        const bool ok = (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ||
                        (ch >= 'a' && ch <= 'z') || ch == '_' || ch == '.' || ch == '-';
        if (!ok) return false;
    }
    return s.find("..") == std::string::npos;
}

bool validEffectType(const std::string& s) {
    if (s.empty() || s.size() > kMaxEffectType) return false;
    for (unsigned char ch : s)
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || ch == '_')) return false;
    return true;
}

// Pemeriksa satu track; mengembalikan pesan kesalahan atau nullptr jika sehat.
const char* checkTrack(const Track& t, bool opacityRange) {
    if (t.keys.empty()) return "track kosong";
    if (t.keys.size() > kMaxKeysPerTrack) return "terlalu banyak keyframe";
    int64_t prevFrame = 0;
    bool first = true;
    for (const Keyframe& k : t.keys) {
        if (k.frame < -kMaxKeyFrame || k.frame > kMaxKeyFrame) return "frame keyframe di luar batas";
        if (!first && k.frame <= prevFrame) return "keyframe tidak terurut ketat";
        first = false;
        prevFrame = k.frame;
        if (!std::isfinite(k.value)) return "nilai keyframe bukan angka";
        if (opacityRange && (k.value < 0.f || k.value > 1.f)) return "keyframe opacity di luar 0..1";
        if (static_cast<uint8_t>(k.interp) > static_cast<uint8_t>(Interp::Bezier))
            return "jenis interpolasi tidak dikenal";
        if (!(k.outX >= 0.f && k.outX <= 1.f) || !(k.inX >= 0.f && k.inX <= 1.f))
            return "handle x di luar 0..1";
        if (!(std::fabs(k.outY) <= kMaxHandleY) || !(std::fabs(k.inY) <= kMaxHandleY))
            return "handle y tidak valid";
    }
    return nullptr;
}

// --- Akses kanal (properti Transform atau parameter efek) pada sebuah Layer ---
struct Chan {
    std::vector<Track>* tracks = nullptr;
    float* base = nullptr;
    int trackId = 0;
};

float* propPtr(Transform& t, Prop p) {
    switch (p) {
        case Prop::PosX: return &t.posX;
        case Prop::PosY: return &t.posY;
        case Prop::PosZ: return &t.posZ;
        case Prop::ScaleX: return &t.scaleX;
        case Prop::ScaleY: return &t.scaleY;
        case Prop::Rotation: return &t.rotation;
        case Prop::Opacity: return &t.opacity;
        case Prop::AnchorX: return &t.anchorX;
        case Prop::AnchorY: return &t.anchorY;
        case Prop::AnchorZ: return &t.anchorZ;
        default: return nullptr;
    }
}

bool resolveChan(Layer& l, Prop ch, Chan* out) {
    const int v = static_cast<int>(ch);
    if (v < kPropCount) {
        out->tracks = &l.tracks;
        out->base = propPtr(l.transform, ch);
        out->trackId = v;
        if (ch == Prop::Time) {   // kanal waktu tidak punya nilai dasar; tampung ke penampung buangan
            static float sink = 0.f;
            out->base = &sink;
        }
        return out->base != nullptr;
    }
    if (v >= kEffectChannelBase) {
        const size_t fx = static_cast<size_t>(effectIndexOf(ch));
        const size_t p = static_cast<size_t>(effectParamOf(ch));
        if (fx >= l.effects.size() || p >= l.effects[fx].params.size()) return false;
        out->tracks = &l.effects[fx].tracks;
        out->base = &l.effects[fx].params[p];
        out->trackId = static_cast<int>(p);
        return true;
    }
    return false;
}

Track* findTrackMut(Chan& c) {
    for (Track& t : *c.tracks)
        if (static_cast<int>(t.prop) == c.trackId) return &t;
    return nullptr;
}

Keyframe* findKeyMut(Track& t, int64_t frame) {
    for (Keyframe& k : t.keys)
        if (k.frame == frame) return &k;
    return nullptr;
}

// Sisipkan/ganti keyframe dengan menjaga urutan track dan urutan frame.
void upsertKey(Chan& c, int64_t frame, float value) {
    Track* t = findTrackMut(c);
    if (!t) {
        Track nt;
        nt.prop = static_cast<Prop>(c.trackId);
        auto it = c.tracks->begin();
        while (it != c.tracks->end() && static_cast<int>(it->prop) < c.trackId) ++it;
        t = &*c.tracks->insert(it, std::move(nt));
    }
    for (Keyframe& k : t->keys)
        if (k.frame == frame) {
            k.value = value;
            return;
        }
    Keyframe nk;
    nk.frame = frame;
    nk.value = value;
    auto it = t->keys.begin();
    while (it != t->keys.end() && it->frame < frame) ++it;
    t->keys.insert(it, nk);
}

void eraseTrack(Chan& c, Track* t) {
    c.tracks->erase(c.tracks->begin() + (t - c.tracks->data()));
}
}  // namespace

bool validate(const Project& p, std::string* err) {
    auto fail = [&](const std::string& msg) {
        if (err) *err = msg;
        return false;
    };

    if (p.width <= 0 || p.height <= 0 || p.width > 16384 || p.height > 16384)
        return fail("ukuran kanvas tidak valid");
    if (p.fps < 1 || p.fps > 240) return fail("fps tidak valid");
    if (p.durationFrames <= 0) return fail("durasi tidak valid");
    if (p.nextId == 0) return fail("nextId tidak valid");

    std::unordered_map<LayerId, size_t> index;
    index.reserve(p.layers.size() * 2 + 1);

    for (size_t i = 0; i < p.layers.size(); ++i) {
        const Layer& l = p.layers[i];
        if (l.id == kRoot) return fail("layer memakai id 0");
        if (l.id >= p.nextId) return fail("id layer >= nextId");
        if (!index.emplace(l.id, i).second) return fail("id layer ganda");
        if (static_cast<uint8_t>(l.type) > static_cast<uint8_t>(LayerType::Text))
            return fail("tipe layer tidak dikenal");
        if (l.name.size() > kMaxNameLen) return fail("nama layer terlalu panjang");
        if (l.inFrame < 0) return fail("inFrame negatif");
        if (l.inFrame >= l.outFrame) return fail("rentang waktu layer kosong atau terbalik");
        if (!std::isfinite(l.width) || !std::isfinite(l.height) || l.width <= 0.f ||
            l.height <= 0.f || l.width > 100000.f || l.height > 100000.f)
            return fail("ukuran layer tidak valid");
        if (!l.transform.finite()) return fail("transformasi berisi NaN/tak hingga");
        if (l.transform.opacity < 0.f || l.transform.opacity > 1.f)
            return fail("opacity di luar 0..1");

        if (static_cast<uint8_t>(l.shape) >= static_cast<uint8_t>(ShapeKind::Count))
            return fail("bentuk tidak dikenal");
        if (!(l.cornerRadius >= 0.f && l.cornerRadius <= 100000.f)) return fail("radius sudut tidak valid");
        if (l.sides < 3 || l.sides > 64) return fail("jumlah sisi di luar 3..64");
        if (!(l.innerRatio >= 0.05f && l.innerRatio <= 1.f)) return fail("rasio bintang di luar 0.05..1");
        if (static_cast<uint8_t>(l.blend) >= static_cast<uint8_t>(BlendMode::Count))
            return fail("blend mode tidak dikenal");
        if (l.matte > 2) return fail("mode matte tidak dikenal");
        if (l.matte != 0 && (l.type == LayerType::Group || l.type == LayerType::Null || l.type == LayerType::Audio))
            return fail("matte pada layer yang tidak digambar");
        if (l.type == LayerType::Video || l.type == LayerType::Audio) {
            if (!validImageName(l.image)) return fail("nama klip video tidak valid");
            if (l.srcCount < 1 || l.srcCount > 10000000) return fail("jumlah frame video tidak valid");
            if (l.srcStart < -1000000000 || l.srcStart > 1000000000) return fail("awal video tidak valid");
            if (!(l.rate >= 0.05f && l.rate <= 20.f)) return fail("kecepatan video tidak valid");
        }
        if (l.type == LayerType::Text) {
            if (l.text.size() > kMaxTextLen) return fail("teks terlalu panjang");
            if (!(l.fontSize >= 4.f && l.fontSize <= 2000.f)) return fail("ukuran huruf tidak valid");
            if ((l.textStyle >> 4) != 0) return fail("gaya teks tidak valid");
        }
        if (l.type == LayerType::Image || l.type == LayerType::Text) {
            if (!validImageName(l.image)) return fail("nama gambar tidak valid");
        } else if (!l.image.empty() && !validImageName(l.image)) {
            return fail("nama gambar tidak valid");
        }

        int prevProp = -1;
        for (const Track& t : l.tracks) {
            const int pi = static_cast<int>(t.prop);
            if (pi < 0 || pi >= kPropCount) return fail("properti track tidak dikenal");
            if (pi <= prevProp) return fail("track tidak terurut atau ganda");
            prevProp = pi;
            if (const char* e = checkTrack(t, t.prop == Prop::Opacity)) return fail(e);
        }

        if (l.effects.size() > static_cast<size_t>(kMaxEffects)) return fail("terlalu banyak efek");
        for (const Effect& e : l.effects) {
            if (!validEffectType(e.type)) return fail("jenis efek tidak valid");
            if (e.params.size() > static_cast<size_t>(kMaxEffectParams)) return fail("terlalu banyak parameter efek");
            for (float v : e.params)
                if (!std::isfinite(v)) return fail("parameter efek bukan angka");
            int prevP = -1;
            for (const Track& t : e.tracks) {
                const int pi = static_cast<int>(t.prop);
                if (pi < 0 || static_cast<size_t>(pi) >= e.params.size()) return fail("track efek di luar parameter");
                if (pi <= prevP) return fail("track efek tidak terurut atau ganda");
                prevP = pi;
                if (const char* er = checkTrack(t, false)) return fail(er);
            }
        }
    }

    for (const Layer& l : p.layers) {
        LayerId cur = l.parent;
        size_t steps = 0;
        while (cur != kRoot) {
            if (cur == l.id) return fail("induk membentuk lingkaran");
            auto it = index.find(cur);
            if (it == index.end()) return fail("induk tidak ada");
            const Layer& parent = p.layers[it->second];
            if (parent.type == LayerType::Audio) return fail("induk tidak boleh layer suara");
            if (++steps > p.layers.size()) return fail("induk membentuk lingkaran");
            cur = parent.parent;
        }
    }
    return true;
}

Document::Document(Project p, size_t maxUndo) : project_(std::move(p)), maxUndo_(maxUndo) {}

const Layer* Document::find(LayerId id) const {
    size_t i = indexOf(project_, id);
    return i == kNpos ? nullptr : &project_.layers[i];
}

// Durasi proyek mengikuti isi: tumbuh saat layer diperpanjang/ditambah melewati ujung, menyusut saat dipendekkan.
// Batas bawah 1 detik supaya proyek kosong tetap punya garis waktu.
static void fitDuration(Project& p) {
    int64_t need = std::max<int64_t>(p.fps > 0 ? p.fps : 30, 1);
    for (const Layer& l : p.layers) need = std::max(need, l.outFrame);
    p.durationFrames = need;
}

bool Document::commit(Project&& candidate) {
    fitDuration(candidate);
    if (!validate(candidate, &lastError_)) return false;
    if (candidate == project_) return true;  // tidak ada perubahan nyata: jangan catat undo
    if (batch_) {
        if (!batchHas_) {  // edit pertama dalam gestur: simpan keadaan awal sebagai satu langkah undo
            batchBase_ = project_;
            batchHas_ = true;
        }
    } else {
        undo_.push_back(std::move(project_));
        if (undo_.size() > maxUndo_) undo_.erase(undo_.begin());
    }
    redo_.clear();
    project_ = std::move(candidate);
    ++revision_;
    return true;
}

LayerId Document::addLayer(LayerType type, const std::string& name, int64_t inFrame,
                           int64_t outFrame, LayerId parent) {
    Layer l;
    l.type = type;
    l.name = name;
    l.parent = parent;
    l.inFrame = inFrame;
    l.outFrame = outFrame;
    return addLayer(l);
}

LayerId Document::addLayer(const Layer& proto) {
    Project c = project_;
    Layer l = proto;
    l.id = c.nextId++;
    const LayerId id = l.id;
    c.layers.push_back(std::move(l));
    return commit(std::move(c)) ? id : 0;
}

// Pola umum: salin proyek, temukan layer, ubah, commit.
#define EDIT_LAYER(var)                                                         \
    Project c = project_;                                                       \
    size_t li_ = indexOf(c, id);                                                \
    if (li_ == kNpos) {                                                         \
        lastError_ = "layer tidak ditemukan";                                   \
        return false;                                                           \
    }                                                                           \
    Layer& var = c.layers[li_];

bool Document::setText(LayerId id, const std::string& image, float width, float height, const std::string& text,
                       float fontSize, uint8_t style) {
    EDIT_LAYER(l);
    if (l.type != LayerType::Text) {
        lastError_ = "bukan layer Teks";
        return false;
    }
    l.image = image;
    l.width = width;
    l.height = height;
    l.text = text;
    l.fontSize = fontSize;
    l.textStyle = style;
    return commit(std::move(c));
}

bool Document::setSize(LayerId id, float width, float height) {
    EDIT_LAYER(l);
    l.width = width;
    l.height = height;
    return commit(std::move(c));
}

bool Document::removeLayer(LayerId id) {
    const size_t ri = indexOf(project_, id);
    if (ri == kNpos) {
        lastError_ = "layer tidak ditemukan";
        return false;
    }
    Project c = project_;
    // Menghapus Grup ikut menghapus isinya; menghapus layer biasa hanya melepas tautan anak-anaknya.
    std::unordered_set<LayerId> dead{id};
    bool grew = true;
    while (grew) {
        grew = false;
        for (const Layer& l : c.layers) {
            if (dead.count(l.id) || !dead.count(l.parent)) continue;
            const size_t pi = indexOf(c, l.parent);
            if (pi != kNpos && c.layers[pi].type == LayerType::Group) {
                dead.insert(l.id);
                grew = true;
            }
        }
    }
    const LayerId up = project_.layers[ri].parent;
    for (Layer& l : c.layers)
        if (!dead.count(l.id) && l.parent == id) l.parent = up;
    c.layers.erase(std::remove_if(c.layers.begin(), c.layers.end(),
                                  [&](const Layer& l) { return dead.count(l.id) != 0; }),
                   c.layers.end());
    return commit(std::move(c));
}

// Kelompokkan beberapa layer: Grup baru diletakkan di pusat layer-layer itu (agar putar/skala grup berpusat di isinya);
// posisi anak dikompensasi (nilai dasar dan semua keyframe PosX/PosY) sehingga tampilan tidak berubah.
LayerId Document::groupLayers(const std::vector<LayerId>& ids, const std::string& name, double frame) {
    Project c = project_;
    std::vector<size_t> idx;
    for (LayerId id : ids) {
        const size_t i = indexOf(c, id);
        if (i == kNpos) { lastError_ = "layer tidak ditemukan"; return 0; }
        if (c.layers[i].type == LayerType::Audio) continue;     // suara tidak ikut dikelompokkan
        bool dup = false;
        for (size_t k : idx) if (k == i) dup = true;
        if (!dup) idx.push_back(i);
    }
    // buang layer yang leluhurnya juga terpilih (tetap anak leluhurnya)
    std::vector<size_t> top;
    for (size_t i : idx) {
        bool covered = false;
        LayerId cur = c.layers[i].parent;
        for (size_t s = 0; cur != kRoot && s <= c.layers.size(); ++s) {
            const size_t pi = indexOf(c, cur);
            if (pi == kNpos) break;
            for (size_t k : idx) if (k == pi) covered = true;
            cur = c.layers[pi].parent;
        }
        if (!covered) top.push_back(i);
    }
    if (top.empty()) { lastError_ = "tidak ada layer untuk dikelompokkan"; return 0; }
    bool sameParent = true;
    for (size_t i : top) if (c.layers[i].parent != c.layers[top[0]].parent) sameParent = false;
    Layer g;
    g.type = LayerType::Group;
    g.name = name;
    g.id = c.nextId++;
    g.inFrame = 0;
    int64_t out = project_.durationFrames;
    for (size_t i : top) out = std::max(out, c.layers[i].outFrame);
    g.outFrame = std::max<int64_t>(out, 1);
    g.width = g.height = 100.f;
    g.parent = sameParent ? c.layers[top[0]].parent : kRoot;
    float cx = 0, cy = 0;
    if (sameParent) {
        for (size_t i : top) {
            cx += evalProp(c.layers[i], Prop::PosX, frame);
            cy += evalProp(c.layers[i], Prop::PosY, frame);
        }
        cx /= static_cast<float>(top.size());
        cy /= static_cast<float>(top.size());
        g.transform.posX = cx;
        g.transform.posY = cy;
    }
    for (size_t i : top) {
        Layer& l = c.layers[i];
        l.parent = g.id;
        if (cx != 0.f || cy != 0.f) {
            l.transform.posX -= cx;
            l.transform.posY -= cy;
            for (Track& t : l.tracks) {
                if (t.prop != Prop::PosX && t.prop != Prop::PosY) continue;
                const float d = t.prop == Prop::PosX ? cx : cy;
                for (Keyframe& k : t.keys) k.value -= d;
            }
        }
    }
    const LayerId gid = g.id;
    c.layers.push_back(std::move(g));
    return commit(std::move(c)) ? gid : 0;
}

// Bongkar Grup: anak naik ke induk Grup. Posisi dikompensasi bila Grup hanya bergeser (tanpa skala/putar/opasitas/animasi).
bool Document::ungroup(LayerId id) {
    Project c = project_;
    const size_t gi = indexOf(c, id);
    if (gi == kNpos) { lastError_ = "layer tidak ditemukan"; return false; }
    if (c.layers[gi].type != LayerType::Group) { lastError_ = "bukan Grup"; return false; }
    const Layer g = c.layers[gi];
    bool plain = g.transform.scaleX == 1.f && g.transform.scaleY == 1.f && g.transform.rotation == 0.f &&
                 g.transform.opacity == 1.f && g.transform.anchorX == 0.f && g.transform.anchorY == 0.f && g.tracks.empty();
    for (Layer& l : c.layers) {
        if (l.parent != id) continue;
        l.parent = g.parent;
        if (plain && (g.transform.posX != 0.f || g.transform.posY != 0.f)) {
            l.transform.posX += g.transform.posX;
            l.transform.posY += g.transform.posY;
            for (Track& t : l.tracks) {
                if (t.prop != Prop::PosX && t.prop != Prop::PosY) continue;
                const float d = t.prop == Prop::PosX ? g.transform.posX : g.transform.posY;
                for (Keyframe& k : t.keys) k.value += d;
            }
        }
    }
    c.layers.erase(c.layers.begin() + static_cast<std::ptrdiff_t>(gi));
    return commit(std::move(c));
}

bool Document::moveLayer(LayerId id, size_t newIndex) {
    Project c = project_;
    size_t i = indexOf(c, id);
    if (i == kNpos) {
        lastError_ = "layer tidak ditemukan";
        return false;
    }
    if (newIndex >= c.layers.size()) {
        lastError_ = "indeks tujuan di luar jangkauan";
        return false;
    }
    auto b = c.layers.begin();
    if (i < newIndex)
        std::rotate(b + i, b + i + 1, b + newIndex + 1);
    else if (newIndex < i)
        std::rotate(b + newIndex, b + i, b + i + 1);
    return commit(std::move(c));
}

bool Document::setParent(LayerId id, LayerId newParent) {
    EDIT_LAYER(l);
    l.parent = newParent;
    return commit(std::move(c));
}

bool Document::setTransform(LayerId id, const Transform& t) {
    EDIT_LAYER(l);
    l.transform = t;
    return commit(std::move(c));
}

bool Document::setColor(LayerId id, uint32_t rgba) {
    EDIT_LAYER(l);
    l.color = rgba;
    return commit(std::move(c));
}

bool Document::renameLayer(LayerId id, const std::string& name) {
    EDIT_LAYER(l);
    l.name = name;
    return commit(std::move(c));
}

bool Document::shiftLayer(LayerId id, int64_t deltaFrames) {
    EDIT_LAYER(l);
    // Hindari overflow pada delta ekstrem: tolak terang-terangan.
    if (deltaFrames > (INT64_C(1) << 40) || deltaFrames < -(INT64_C(1) << 40)) {
        lastError_ = "pergeseran terlalu besar";
        return false;
    }
    l.inFrame += deltaFrames;
    l.outFrame += deltaFrames;
    // Animasi ikut bergeser (batas dicek validate).
    for (Track& t : l.tracks)
        for (Keyframe& k : t.keys) {
            k.frame += deltaFrames;
            if (t.prop == Prop::Time) k.value += static_cast<float>(deltaFrames);   // waktu-layer ikut bergeser
        }
    for (Effect& e : l.effects)
        for (Track& t : e.tracks)
            for (Keyframe& k : t.keys) k.frame += deltaFrames;
    return commit(std::move(c));
}

bool Document::trimLayer(LayerId id, int64_t newIn, int64_t newOut) {
    EDIT_LAYER(l);
    if (l.type == LayerType::Video || l.type == LayerType::Audio)   // isi video/suara tidak bergeser saat ujung depan dipotong
        l.srcStart += static_cast<int64_t>(std::llround(static_cast<double>(newIn - l.inFrame) * static_cast<double>(l.rate)));
    l.inFrame = newIn;
    l.outFrame = newOut;
    return commit(std::move(c));
}

LayerId Document::splitLayer(LayerId id, int64_t frame) {
    Project c = project_;
    size_t i = indexOf(c, id);
    if (i == kNpos) {
        lastError_ = "layer tidak ditemukan";
        return 0;
    }
    if (c.layers[i].type == LayerType::Group) {
        lastError_ = "Group tidak bisa dipotong";
        return 0;
    }
    if (frame <= c.layers[i].inFrame || frame >= c.layers[i].outFrame) {
        lastError_ = "titik potong harus berada di dalam layer";
        return 0;
    }
    // Potongan kanan mewarisi SEMUA properti (transformasi, warna, induk, nama, efek) dan seluruh track.
    // Kedua potongan memegang keyframe yang sama, jadi nilai di tiap frame tidak berubah sedikit pun.
    Layer right = c.layers[i];
    right.id = c.nextId++;
    if (right.type == LayerType::Video || right.type == LayerType::Audio)
        right.srcStart += static_cast<int64_t>(std::llround(static_cast<double>(frame - right.inFrame) * static_cast<double>(right.rate)));
    right.inFrame = frame;
    c.layers[i].outFrame = frame;
    const LayerId rid = right.id;
    c.layers.insert(c.layers.begin() + static_cast<std::ptrdiff_t>(i) + 1, std::move(right));
    return commit(std::move(c)) ? rid : 0;
}

// ---------------- Animasi ----------------

// Cari layer + kanal + track + keyframe pada salinan.
#define FIND_KEY(r, c, id, prop, frame)                                              \
    EDIT_LAYER(lay_);                                                                \
    Chan ch_;                                                                        \
    if (!resolveChan(lay_, prop, &ch_)) {                                            \
        lastError_ = "kanal tidak ada";                                              \
        return false;                                                                \
    }                                                                                \
    Track* trk_ = findTrackMut(ch_);                                                 \
    if (!trk_) {                                                                     \
        lastError_ = "properti tidak beranimasi";                                    \
        return false;                                                                \
    }                                                                                \
    Keyframe* key_ = findKeyMut(*trk_, frame);                                       \
    if (!key_) {                                                                     \
        lastError_ = "keyframe tidak ditemukan";                                     \
        return false;                                                                \
    }                                                                                \
    const size_t kidx_ = static_cast<size_t>(key_ - trk_->keys.data());

bool Document::setKey(LayerId id, Prop prop, int64_t frame, float value) {
    EDIT_LAYER(l);
    Chan ch;
    if (!resolveChan(l, prop, &ch)) {
        lastError_ = "kanal tidak ada";
        return false;
    }
    upsertKey(ch, frame, value);
    return commit(std::move(c));
}

bool Document::keyProps(LayerId id, int64_t frame, uint32_t propMask) {
    EDIT_LAYER(l);
    const Layer& orig = project_.layers[li_];
    for (int pi = 0; pi < kTransformProps; ++pi) {
        if (!(propMask & (1u << pi))) continue;
        Chan ch;
        resolveChan(l, static_cast<Prop>(pi), &ch);
        upsertKey(ch, frame, evalProp(orig, static_cast<Prop>(pi), static_cast<double>(frame)));
    }
    return commit(std::move(c));
}

bool Document::removeKey(LayerId id, Prop prop, int64_t frame) {
    FIND_KEY(r, c, id, prop, frame);
    const float lastValue = key_->value;
    trk_->keys.erase(trk_->keys.begin() + static_cast<std::ptrdiff_t>(kidx_));
    if (trk_->keys.empty()) {
        *ch_.base = lastValue;
        eraseTrack(ch_, trk_);
    }
    return commit(std::move(c));
}

bool Document::moveKey(LayerId id, Prop prop, int64_t frame, int64_t newFrame) {
    if (newFrame == frame) return true;
    FIND_KEY(r, c, id, prop, frame);
    if (findKeyMut(*trk_, newFrame)) {
        lastError_ = "sudah ada keyframe di frame tujuan";
        return false;
    }
    Keyframe moved = *key_;
    moved.frame = newFrame;
    trk_->keys.erase(trk_->keys.begin() + static_cast<std::ptrdiff_t>(kidx_));
    auto it = trk_->keys.begin();
    while (it != trk_->keys.end() && it->frame < newFrame) ++it;
    trk_->keys.insert(it, moved);
    return commit(std::move(c));
}

bool Document::editKey(LayerId id, Prop prop, int64_t frame, int64_t newFrame, float newValue) {
    FIND_KEY(r, c, id, prop, frame);
    if (newFrame != frame && findKeyMut(*trk_, newFrame)) {
        lastError_ = "sudah ada keyframe di frame tujuan";
        return false;
    }
    Keyframe moved = *key_;
    moved.frame = newFrame;
    moved.value = newValue;
    trk_->keys.erase(trk_->keys.begin() + static_cast<std::ptrdiff_t>(kidx_));
    auto it = trk_->keys.begin();
    while (it != trk_->keys.end() && it->frame < newFrame) ++it;
    trk_->keys.insert(it, moved);
    return commit(std::move(c));
}

bool Document::setKeyInterp(LayerId id, Prop prop, int64_t frame, Interp interp) {
    FIND_KEY(r, c, id, prop, frame);
    if (interp == Interp::Bezier && key_->interp != Interp::Bezier && kidx_ + 1 < trk_->keys.size()) {
        Keyframe& a = *key_;
        Keyframe& b = trk_->keys[kidx_ + 1];
        const float dv = b.value - a.value;
        a.outX = 1.f / 3.f;
        a.outY = dv / 3.f;
        b.inX = 1.f / 3.f;
        b.inY = -dv / 3.f;
    }
    key_->interp = interp;
    return commit(std::move(c));
}

bool Document::setKeyHandles(LayerId id, Prop prop, int64_t frame, float outX, float outY,
                             float inX, float inY) {
    FIND_KEY(r, c, id, prop, frame);
    if (kidx_ + 1 >= trk_->keys.size()) {
        lastError_ = "keyframe terakhir tidak punya segmen keluar";
        return false;
    }
    Keyframe& a = *key_;
    Keyframe& b = trk_->keys[kidx_ + 1];
    a.interp = Interp::Bezier;
    a.outX = outX;
    a.outY = outY;
    b.inX = inX;
    b.inY = inY;
    return commit(std::move(c));
}

bool Document::setKeyEase(LayerId id, Prop prop, int64_t frame, float x1, float y1, float x2,
                          float y2) {
    FIND_KEY(r, c, id, prop, frame);
    if (kidx_ + 1 >= trk_->keys.size()) {
        lastError_ = "keyframe terakhir tidak punya segmen keluar";
        return false;
    }
    Keyframe& a = *key_;
    Keyframe& b = trk_->keys[kidx_ + 1];
    easeToHandles(x1, y1, x2, y2, a.value, b.value, &a, &b);
    return commit(std::move(c));
}

namespace {
void toBezierLinear(Keyframe& a, Keyframe& b) {
    if (a.interp == Interp::Bezier) return;
    const float d = (b.value - a.value) / 3.f;
    a.interp = Interp::Bezier;
    a.outX = 1.f / 3.f;
    a.outY = d;
    b.inX = 1.f / 3.f;
    b.inY = -d;
}
}  // namespace

bool Document::easeKey(LayerId id, Prop prop, int64_t frame, int mode) {
    FIND_KEY(r, c, id, prop, frame);
    if (mode < 0 || mode > 4) {
        lastError_ = "mode ease tidak dikenal";
        return false;
    }
    auto& ks = trk_->keys;
    const size_t i = kidx_;
    const bool hasPrev = i > 0, hasNext = i + 1 < ks.size();
    if (!hasPrev && !hasNext) {
        lastError_ = "butuh minimal dua keyframe";
        return false;
    }
    if (hasPrev) toBezierLinear(ks[i - 1], ks[i]);
    if (hasNext) toBezierLinear(ks[i], ks[i + 1]);
    Keyframe& k = ks[i];
    float slope = 0.f;  // nilai per frame
    if (mode == 3) {
        if (hasPrev && hasNext)
            slope = (ks[i + 1].value - ks[i - 1].value) / static_cast<float>(ks[i + 1].frame - ks[i - 1].frame);
        else if (hasNext)
            slope = (ks[i + 1].value - k.value) / static_cast<float>(ks[i + 1].frame - k.frame);
        else
            slope = (k.value - ks[i - 1].value) / static_cast<float>(k.frame - ks[i - 1].frame);
    }
    const bool doIn = hasPrev && (mode == 0 || mode == 1 || mode == 3 || mode == 4);
    const bool doOut = hasNext && (mode == 0 || mode == 2 || mode == 3 || mode == 4);
    if (doIn) {
        Keyframe& p = ks[i - 1];
        const float dur = static_cast<float>(k.frame - p.frame);
        k.inX = 1.f / 3.f;
        k.inY = mode == 4 ? -(k.value - p.value) / 3.f : -slope * k.inX * dur;
    }
    if (doOut) {
        const Keyframe& n = ks[i + 1];
        const float dur = static_cast<float>(n.frame - k.frame);
        k.outX = 1.f / 3.f;
        k.outY = mode == 4 ? (n.value - k.value) / 3.f : slope * k.outX * dur;
    }
    return commit(std::move(c));
}

namespace {
// Salin track dengan menggeser `delta` frame; buang key di luar [lo, hi) dan track yang jadi kosong.
std::vector<Track> shiftedTracks(const std::vector<Track>& in, int64_t delta, int64_t lo, int64_t hi) {
    std::vector<Track> out;
    for (const Track& t : in) {
        Track n;
        n.prop = t.prop;
        for (Keyframe k : t.keys) {
            k.frame += delta;
            if (k.frame >= lo && k.frame < hi) n.keys.push_back(k);
        }
        if (!n.keys.empty()) out.push_back(std::move(n));
    }
    return out;
}
bool isTransformProp(Prop p) { return static_cast<int>(p) < 10; }
}  // namespace

bool Document::pasteStyle(LayerId id, const Layer& src, uint32_t mask) {
    EDIT_LAYER(l);
    const int64_t delta = l.inFrame - src.inFrame;
    auto replaceTracks = [&](auto pred) {
        std::vector<Track> keep;
        for (Track& t : l.tracks) if (!pred(t.prop)) keep.push_back(std::move(t));
        std::vector<Track> add;
        for (Track& t : shiftedTracks(src.tracks, delta, l.inFrame, l.outFrame))
            if (pred(t.prop)) add.push_back(std::move(t));
        for (Track& t : add) keep.push_back(std::move(t));
        std::sort(keep.begin(), keep.end(), [](const Track& a, const Track& b) { return a.prop < b.prop; });
        l.tracks = std::move(keep);
    };
    if ((mask & 1) && l.type == LayerType::Shape && src.type == LayerType::Shape) {
        l.color = src.color;
        l.shape = src.shape;
        l.cornerRadius = src.cornerRadius;
        l.sides = src.sides;
        l.innerRatio = src.innerRatio;
    }
    if (mask & 2) {
        l.blend = src.blend;
        l.transform.opacity = src.transform.opacity;
        replaceTracks([](Prop p) { return p == Prop::Opacity; });
    }
    if (mask & 4) {
        const float op = l.transform.opacity;
        l.transform = src.transform;
        l.transform.opacity = op;
        replaceTracks([](Prop p) { return isTransformProp(p) && p != Prop::Opacity; });
    }
    if ((mask & 8) && l.type != LayerType::Group && l.type != LayerType::Null && l.type != LayerType::Audio) {
        l.effects = src.effects;
        for (Effect& e : l.effects) e.tracks = shiftedTracks(e.tracks, delta, l.inFrame, l.outFrame);
    }
    return commit(std::move(c));
}

namespace {
// Skala waktu satu daftar key relatif terhadap `origin`; key yang jatuh di frame sama dibuang (yang awal dipertahankan).
void scaleKeys(std::vector<Track>& tracks, int64_t origin, double factor) {
    for (Track& t : tracks) {
        std::vector<Keyframe> out;
        for (Keyframe k : t.keys) {
            k.frame = origin + static_cast<int64_t>(std::llround(static_cast<double>(k.frame - origin) / factor));
            if (out.empty() || k.frame > out.back().frame) out.push_back(k);
        }
        t.keys = std::move(out);
    }
}
}  // namespace

bool Document::scaleLayerTime(LayerId id, double factor) {
    if (!(factor >= 0.05 && factor <= 20.0)) {
        lastError_ = "kecepatan di luar 0.05x-20x";
        return false;
    }
    EDIT_LAYER(l);
    const int64_t len = l.outFrame - l.inFrame;
    const int64_t nlen = std::max<int64_t>(1, static_cast<int64_t>(std::llround(static_cast<double>(len) / factor)));
    scaleKeys(l.tracks, l.inFrame, factor);
    for (Effect& e : l.effects) scaleKeys(e.tracks, l.inFrame, factor);
    l.outFrame = l.inFrame + nlen;
    if (l.type == LayerType::Video || l.type == LayerType::Audio) l.rate = static_cast<float>(std::clamp(static_cast<double>(l.rate) * factor, 0.05, 20.0));
    return commit(std::move(c));
}

bool Document::clearTrack(LayerId id, Prop prop, int64_t frame) {
    EDIT_LAYER(l);
    Chan ch;
    if (!resolveChan(l, prop, &ch)) {
        lastError_ = "kanal tidak ada";
        return false;
    }
    Track* t = findTrackMut(ch);
    if (!t) {
        lastError_ = "properti tidak beranimasi";
        return false;
    }
    *ch.base = evalTrack(*t, static_cast<double>(frame));
    eraseTrack(ch, t);
    return commit(std::move(c));
}

bool Document::setPropAt(LayerId id, Prop prop, int64_t frame, float value) {
    EDIT_LAYER(l);
    Chan ch;
    if (!resolveChan(l, prop, &ch)) {
        lastError_ = "kanal tidak ada";
        return false;
    }
    if (findTrackMut(ch))
        upsertKey(ch, frame, value);
    else
        *ch.base = value;
    return commit(std::move(c));
}

bool Document::setTransformAt(LayerId id, int64_t frame, const Transform& t) {
    EDIT_LAYER(l);
    const Transform cur = evalTransform(l, static_cast<double>(frame));
    for (int pi = 0; pi < kTransformProps; ++pi) {
        const Prop p = static_cast<Prop>(pi);
        const float nv = getProp(t, p);
        if (nv == getProp(cur, p)) continue;
        Chan ch;
        resolveChan(l, p, &ch);
        if (findTrackMut(ch))
            upsertKey(ch, frame, nv);
        else
            *ch.base = nv;
    }
    return commit(std::move(c));
}

// ---------------- Tampilan & efek ----------------

bool Document::setShape(LayerId id, ShapeKind kind, float cornerRadius, int sides, float innerRatio) {
    EDIT_LAYER(l);
    l.shape = kind;
    l.cornerRadius = cornerRadius;
    l.sides = sides;
    l.innerRatio = innerRatio;
    return commit(std::move(c));
}

bool Document::setHidden(LayerId id, bool hidden) {
    EDIT_LAYER(l);
    l.hidden = hidden;
    return commit(std::move(c));
}

bool Document::resetProp(LayerId id, Prop prop, float value) {
    EDIT_LAYER(l);
    Chan ch;
    if (!resolveChan(l, prop, &ch)) {
        lastError_ = "kanal tidak ada";
        return false;
    }
    for (size_t i = 0; i < ch.tracks->size(); ++i)
        if (static_cast<int>((*ch.tracks)[i].prop) == ch.trackId) {
            ch.tracks->erase(ch.tracks->begin() + static_cast<std::ptrdiff_t>(i));
            break;
        }
    *ch.base = value;
    return commit(std::move(c));
}

bool Document::setMatte(LayerId id, int mode) {
    EDIT_LAYER(l);
    if (mode < 0 || mode > 2) { lastError_ = "mode matte tidak dikenal"; return false; }
    if (mode != 0 && (l.type == LayerType::Group || l.type == LayerType::Null || l.type == LayerType::Audio)) {
        lastError_ = "matte hanya untuk layer yang digambar";
        return false;
    }
    l.matte = static_cast<uint8_t>(mode);
    return commit(std::move(c));
}

bool Document::setBlend(LayerId id, BlendMode mode) {
    EDIT_LAYER(l);
    l.blend = mode;
    return commit(std::move(c));
}

int Document::addEffect(LayerId id, const std::string& type, const std::vector<float>& params) {
    Project c = project_;
    size_t li = indexOf(c, id);
    if (li == kNpos) {
        lastError_ = "layer tidak ditemukan";
        return -1;
    }
    Layer& l = c.layers[li];
    if (l.type == LayerType::Group || l.type == LayerType::Null || l.type == LayerType::Audio) {
        lastError_ = "efek hanya untuk layer Shape/Image";
        return -1;
    }
    Effect e;
    e.type = type;
    e.params = params;
    l.effects.push_back(std::move(e));
    const int idx = static_cast<int>(l.effects.size()) - 1;
    return commit(std::move(c)) ? idx : -1;
}

bool Document::removeEffect(LayerId id, int index) {
    EDIT_LAYER(l);
    if (index < 0 || static_cast<size_t>(index) >= l.effects.size()) {
        lastError_ = "efek tidak ditemukan";
        return false;
    }
    l.effects.erase(l.effects.begin() + index);
    return commit(std::move(c));
}

bool Document::moveEffect(LayerId id, int index, int newIndex) {
    EDIT_LAYER(l);
    const int n = static_cast<int>(l.effects.size());
    if (index < 0 || index >= n || newIndex < 0 || newIndex >= n) {
        lastError_ = "indeks efek di luar jangkauan";
        return false;
    }
    auto b = l.effects.begin();
    if (index < newIndex)
        std::rotate(b + index, b + index + 1, b + newIndex + 1);
    else if (newIndex < index)
        std::rotate(b + newIndex, b + index, b + index + 1);
    return commit(std::move(c));
}

bool Document::setEffectEnabled(LayerId id, int index, bool enabled) {
    EDIT_LAYER(l);
    if (index < 0 || static_cast<size_t>(index) >= l.effects.size()) {
        lastError_ = "efek tidak ditemukan";
        return false;
    }
    l.effects[static_cast<size_t>(index)].enabled = enabled;
    return commit(std::move(c));
}

void Document::beginBatch() {
    endBatch();
    batch_ = true;
    batchHas_ = false;
}

void Document::endBatch() {
    if (batch_ && batchHas_) {
        undo_.push_back(std::move(batchBase_));
        if (undo_.size() > maxUndo_) undo_.erase(undo_.begin());
    }
    batch_ = false;
    batchHas_ = false;
}

bool Document::undo() {
    endBatch();
    if (undo_.empty()) return false;
    redo_.push_back(std::move(project_));
    project_ = std::move(undo_.back());
    undo_.pop_back();
    ++revision_;
    return true;
}

bool Document::redo() {
    endBatch();
    if (redo_.empty()) return false;
    undo_.push_back(std::move(project_));
    project_ = std::move(redo_.back());
    redo_.pop_back();
    ++revision_;
    return true;
}


// ---------- Preset animasi ----------
namespace {
struct PV { float t, mul, addW, addH, add; int ease; };   // nilai = rest*mul + addW*lebar + addH*tinggi + add; ease = untuk segmen yang BERAKHIR di titik ini
struct PTrack { Prop prop; std::vector<PV> pts; };
struct PresetDef { const char* id; const char* label; int cat; std::vector<PTrack> tracks; };

constexpr int kOut = 0, kIn = 1, kInOut = 2, kLin = 3;

const std::vector<PresetDef>& presets() {
    auto P = [](float t, float mul, float addW = 0, float addH = 0, float add = 0, int ease = kOut) {
        return PV{t, mul, addW, addH, add, ease};
    };
    static const std::vector<PresetDef> defs = {
        // ---- Masuk (cat 0) ----
        {"fade_in", "Muncul perlahan", 0, {{Prop::Opacity, {P(0, 0), P(1, 1, 0, 0, 0, kInOut)}}}},
        {"slide_left", "Geser dari kiri", 0, {{Prop::PosX, {P(0, 1, -0.45f), P(1, 1)}}}},
        {"slide_right", "Geser dari kanan", 0, {{Prop::PosX, {P(0, 1, 0.45f), P(1, 1)}}}},
        {"slide_up", "Geser dari atas", 0, {{Prop::PosY, {P(0, 1, 0, -0.4f), P(1, 1)}}}},
        {"slide_down", "Geser dari bawah", 0, {{Prop::PosY, {P(0, 1, 0, 0.4f), P(1, 1)}}}},
        {"zoom_in", "Zoom masuk", 0, {{Prop::ScaleX, {P(0, 0.3f), P(1, 1)}}, {Prop::ScaleY, {P(0, 0.3f), P(1, 1)}},
                                      {Prop::Opacity, {P(0, 0), P(0.6f, 1, 0, 0, 0, kInOut)}}}},
        {"pop", "Pop", 0, {{Prop::ScaleX, {P(0, 0), P(0.65f, 1.15f), P(1, 1, 0, 0, 0, kInOut)}},
                           {Prop::ScaleY, {P(0, 0), P(0.65f, 1.15f), P(1, 1, 0, 0, 0, kInOut)}}}},
        {"spin_in", "Putar masuk", 0, {{Prop::Rotation, {P(0, 1, 0, 0, -180), P(1, 1)}},
                                       {Prop::ScaleX, {P(0, 0.4f), P(1, 1)}}, {Prop::ScaleY, {P(0, 0.4f), P(1, 1)}},
                                       {Prop::Opacity, {P(0, 0), P(0.5f, 1, 0, 0, 0, kInOut)}}}},
        // ---- Keluar (cat 1) ----
        {"fade_out", "Menghilang perlahan", 1, {{Prop::Opacity, {P(0, 1), P(1, 0, 0, 0, 0, kInOut)}}}},
        {"slide_out_left", "Geser ke kiri", 1, {{Prop::PosX, {P(0, 1), P(1, 1, -0.45f, 0, 0, kIn)}}}},
        {"slide_out_right", "Geser ke kanan", 1, {{Prop::PosX, {P(0, 1), P(1, 1, 0.45f, 0, 0, kIn)}}}},
        {"slide_out_up", "Geser ke atas", 1, {{Prop::PosY, {P(0, 1), P(1, 1, 0, -0.4f, 0, kIn)}}}},
        {"slide_out_down", "Geser ke bawah", 1, {{Prop::PosY, {P(0, 1), P(1, 1, 0, 0.4f, 0, kIn)}}}},
        {"zoom_out", "Zoom keluar", 1, {{Prop::ScaleX, {P(0, 1), P(1, 0.3f, 0, 0, 0, kIn)}}, {Prop::ScaleY, {P(0, 1), P(1, 0.3f, 0, 0, 0, kIn)}},
                                        {Prop::Opacity, {P(0, 1), P(0.4f, 1), P(1, 0, 0, 0, 0, kInOut)}}}},
        {"shrink", "Mengecil", 1, {{Prop::ScaleX, {P(0, 1), P(0.35f, 1.12f, 0, 0, 0, kInOut), P(1, 0, 0, 0, 0, kIn)}},
                                   {Prop::ScaleY, {P(0, 1), P(0.35f, 1.12f, 0, 0, 0, kInOut), P(1, 0, 0, 0, 0, kIn)}}}},
        {"spin_out", "Putar keluar", 1, {{Prop::Rotation, {P(0, 1), P(1, 1, 0, 0, 180, kIn)}},
                                         {Prop::ScaleX, {P(0, 1), P(1, 0.4f, 0, 0, 0, kIn)}}, {Prop::ScaleY, {P(0, 1), P(1, 0.4f, 0, 0, 0, kIn)}},
                                         {Prop::Opacity, {P(0, 1), P(0.5f, 1), P(1, 0, 0, 0, 0, kInOut)}}}},
        // ---- Loop (cat 2): isi tracks dibuat dinamis di applyPreset ----
        {"pulse", "Berdenyut", 2, {}},
        {"wobble", "Bergoyang", 2, {}},
        {"float", "Melayang", 2, {}},
        {"spin", "Berputar", 2, {}},
    };
    return defs;
}

void easeParams(int e, float* x1, float* y1, float* x2, float* y2) {
    switch (e) {
        case kOut: *x1 = 0.2f; *y1 = 0.8f; *x2 = 0.2f; *y2 = 1.f; break;
        case kIn: *x1 = 0.7f; *y1 = 0.f; *x2 = 0.84f; *y2 = 0.f; break;
        default: *x1 = 0.65f; *y1 = 0.f; *x2 = 0.35f; *y2 = 1.f; break;
    }
}

Track* trackOf(Chan& c) { return findTrackMut(c); }
}  // namespace

int presetCount() { return static_cast<int>(presets().size()); }
const char* presetId(int i) { return presets()[static_cast<size_t>(i)].id; }
const char* presetLabel(int i) { return presets()[static_cast<size_t>(i)].label; }
int presetCategory(int i) { return presets()[static_cast<size_t>(i)].cat; }

bool Document::applyPreset(LayerId id, int preset, int64_t step) {
    EDIT_LAYER(l);
    if (preset < 0 || preset >= presetCount()) {
        lastError_ = "preset tidak dikenal";
        return false;
    }
    if (l.type == LayerType::Audio) {
        lastError_ = "preset animasi tidak berlaku untuk layer suara";
        return false;
    }
    const Layer orig = project_.layers[li_];
    const PresetDef& def = presets()[static_cast<size_t>(preset)];
    const int64_t len = orig.outFrame - orig.inFrame;
    if (len < 2) {
        lastError_ = "layer terlalu pendek";
        return false;
    }
    step = std::clamp<int64_t>(step, 2, std::max<int64_t>(2, len));
    const float pw = static_cast<float>(project_.width), ph = static_cast<float>(project_.height);

    // Hapus keyframe di dalam [f0,f1] pada satu kanal lalu kembalikan jejaknya (dibuat bila belum ada).
    auto clearWindow = [&](Chan& ch, int64_t f0, int64_t f1) {
        if (Track* t = trackOf(ch)) {
            t->keys.erase(std::remove_if(t->keys.begin(), t->keys.end(),
                                         [&](const Keyframe& k) { return k.frame >= f0 && k.frame <= f1; }),
                          t->keys.end());
        }
    };
    auto setEase = [&](Chan& ch, int64_t fa, int64_t fb, int ease) {
        Track* t = trackOf(ch);
        if (!t) return;
        Keyframe* a = nullptr;
        Keyframe* b = nullptr;
        for (Keyframe& k : t->keys) {
            if (k.frame == fa) a = &k;
            if (k.frame == fb) b = &k;
        }
        if (!a || !b) return;
        if (ease == kLin) {
            a->interp = Interp::Linear;
            return;
        }
        float x1, y1, x2, y2;
        easeParams(ease, &x1, &y1, &x2, &y2);
        easeToHandles(x1, y1, x2, y2, a->value, b->value, a, b);
    };
    auto dropEmpty = [&](Chan& ch) {   // jejak tanpa keyframe tidak sah
        Track* t = trackOf(ch);
        if (t && t->keys.empty()) eraseTrack(ch, t);
    };

    if (def.cat == 0 || def.cat == 1) {
        int64_t f0, f1;
        if (def.cat == 0) { f0 = orig.inFrame; f1 = std::min(orig.inFrame + step, orig.outFrame - 1); }
        else { f1 = orig.outFrame - 1; f0 = std::max(orig.inFrame, f1 - step); }
        if (f1 <= f0) {
            lastError_ = "layer terlalu pendek";
            return false;
        }
        for (const PTrack& pt : def.tracks) {
            Chan ch;
            if (!resolveChan(l, pt.prop, &ch)) continue;
            const float rest = evalProp(orig, pt.prop, static_cast<double>(def.cat == 0 ? f1 : f0));
            clearWindow(ch, f0, f1);
            std::vector<int64_t> frames;
            for (const PV& v : pt.pts) {
                const int64_t f = f0 + static_cast<int64_t>(std::llround(static_cast<double>(v.t) * static_cast<double>(f1 - f0)));
                upsertKey(ch, f, rest * v.mul + v.addW * pw + v.addH * ph + v.add);
                frames.push_back(f);
            }
            for (size_t k = 1; k < pt.pts.size(); ++k)
                if (frames[k] > frames[k - 1]) setEase(ch, frames[k - 1], frames[k], pt.pts[k].ease);
            dropEmpty(ch);
        }
    } else {
        // Loop: sepanjang layer, bolak-balik tiap `step` frame (rotasi: satu putaran penuh tiap 2*step).
        const std::string name = def.id;
        Prop prop = Prop::ScaleX;
        float amp = 0.08f;
        if (name == "wobble") { prop = Prop::Rotation; amp = 6.f; }
        else if (name == "float") { prop = Prop::PosY; amp = 0.02f * ph; }
        else if (name == "spin") { prop = Prop::Rotation; amp = 360.f; }
        int64_t n = len / step;
        if (n < 1) {
            lastError_ = "layer lebih pendek dari satu siklus";
            return false;
        }
        if (n > 400) n = 400;
        if (name != "spin" && (n % 2) == 1) --n;   // berakhir di nilai awal
        if (n < 2 && name != "spin") {
            lastError_ = "layer lebih pendek dari satu siklus penuh";
            return false;
        }
        std::vector<Prop> props = {prop};
        if (name == "pulse") props.push_back(Prop::ScaleY);
        for (Prop pr : props) {
            Chan ch;
            if (!resolveChan(l, pr, &ch)) continue;
            const float rest = evalProp(orig, pr, static_cast<double>(orig.inFrame));
            clearWindow(ch, orig.inFrame, orig.outFrame);
            std::vector<int64_t> frames;
            for (int64_t k = 0; k <= n; ++k) {
                const int64_t f = orig.inFrame + k * step;
                if (f >= orig.outFrame) break;
                float v = rest;
                if (name == "pulse") v = (k % 2) ? rest * (1.f + amp) : rest;
                else if (name == "wobble") v = (k % 2) ? rest + ((k / 1) % 4 == 1 ? amp : -amp) : rest;
                else if (name == "float") v = (k % 2) ? rest - amp : rest;
                else v = rest + amp * static_cast<float>(k) * 0.5f;   // spin: 180 derajat per langkah
                upsertKey(ch, f, v);
                frames.push_back(f);
            }
            for (size_t k = 1; k < frames.size(); ++k) setEase(ch, frames[k - 1], frames[k], name == "spin" ? kLin : kInOut);
            dropEmpty(ch);
        }
    }
    return commit(std::move(c));
}

}  // namespace studio
