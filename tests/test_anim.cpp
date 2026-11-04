// Uji animasi: interpolasi, operasi keyframe, penyimpanan v3 (+ baca v2), fuzz.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

#include "model/anim.h"
#include "model/model.h"
#include "model/scene.h"
#include "model/storage.h"

using namespace studio;

static int g_checks = 0, g_fail = 0;
#define CHECK(c)                                                      \
    do {                                                              \
        ++g_checks;                                                   \
        if (!(c)) {                                                   \
            ++g_fail;                                                 \
            std::printf("GAGAL %s:%d  %s\n", __FILE__, __LINE__, #c); \
        }                                                             \
    } while (0)
static bool near(double a, double b, double e = 1e-4) { return std::fabs(a - b) <= e; }

static uint32_t crc32(const char* d, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        c ^= static_cast<uint8_t>(d[i]);
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

// Pembanding independen: Bezier dievaluasi dengan sampling padat + interpolasi linear x->y.
static double refBezier(const Keyframe& a, const Keyframe& b, double frame) {
    const double dt = double(b.frame - a.frame);
    const double u = (frame - a.frame) / dt;
    const double p1x = a.outX, p2x = 1.0 - b.inX;
    const double v0 = a.value, v1 = b.value, p1y = v0 + a.outY, p2y = v1 + b.inY;
    double px = 0, py = v0;
    const int N = 40000;
    for (int i = 1; i <= N; ++i) {
        double s = double(i) / N, m = 1 - s;
        double x = 3 * m * m * s * p1x + 3 * m * s * s * p2x + s * s * s;
        double y = m * m * m * v0 + 3 * m * m * s * p1y + 3 * m * s * s * p2y + s * s * s * v1;
        if (x >= u) {
            double t = (x - px) > 1e-15 ? (u - px) / (x - px) : 1.0;
            return py + (y - py) * t;
        }
        px = x;
        py = y;
    }
    return v1;
}

static void testEval() {
    Track t;
    t.prop = Prop::PosX;
    Keyframe a, b, c;
    a.frame = 10; a.value = 0;
    b.frame = 20; b.value = 100;
    c.frame = 40; c.value = 50;
    t.keys = {a, b, c};
    CHECK(near(evalTrack(t, 0), 0));       // sebelum keyframe pertama
    CHECK(near(evalTrack(t, 10), 0));
    CHECK(near(evalTrack(t, 15), 50));     // linear
    CHECK(near(evalTrack(t, 20), 100));
    CHECK(near(evalTrack(t, 30), 75));
    CHECK(near(evalTrack(t, 99), 50));     // sesudah terakhir
    CHECK(near(evalTrack(t, 12.5), 25));   // frame pecahan

    t.keys[0].interp = Interp::Hold;
    CHECK(near(evalTrack(t, 19.99), 0));
    CHECK(near(evalTrack(t, 20), 100));

    // Bezier dengan handle setara linear = linear
    t.keys[0].interp = Interp::Bezier;
    t.keys[0].outX = 1.f / 3; t.keys[0].outY = 100.f / 3;
    t.keys[1].inX = 1.f / 3; t.keys[1].inY = -100.f / 3;
    for (double f = 10; f <= 20; f += 0.5) CHECK(near(evalTrack(t, f), (f - 10) * 10, 1e-3));

    // Preset ease-in-out CSS: titik tengah tepat 0.5
    Keyframe k0, k1;
    k0.frame = 0; k0.value = 0; k1.frame = 100; k1.value = 1;
    easeToHandles(0.42f, 0, 0.58f, 1, 0, 1, &k0, &k1);
    CHECK(near(evalSegment(k0, k1, 50), 0.5, 1e-4));
    CHECK(near(evalSegment(k0, k1, 0), 0.0));
    CHECK(near(evalSegment(k0, k1, 100), 1.0));
    CHECK(evalSegment(k0, k1, 20) < 0.2);  // lambat di awal
    // Overshoot (back-ease): melewati di bawah 0 lalu ke 1
    easeToHandles(0.36f, 0, 0.66f, -0.56f, 0, 1, &k0, &k1);
    double mn = 1e9, mx = -1e9;
    for (int f = 0; f <= 100; ++f) {
        double v = evalSegment(k0, k1, f);
        mn = std::min(mn, v); mx = std::max(mx, v);
    }
    CHECK(mn < -0.02 && mx <= 1.0001);  // easeInBack: menyelam di bawah 0 dulu, tidak melewati 1
    CHECK(near(evalSegment(k0, k1, 100), 1.0));

    // Acak: bandingkan dengan pembanding independen
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> u01(0, 1), uy(-300, 300), uv(-200, 200);
    for (int i = 0; i < 300; ++i) {
        Keyframe x, y;
        x.frame = 5; y.frame = 5 + 1 + (rng() % 200);
        x.value = uv(rng); y.value = uv(rng);
        x.interp = Interp::Bezier;
        x.outX = u01(rng); x.outY = uy(rng); y.inX = u01(rng); y.inY = uy(rng);
        double f = x.frame + (y.frame - x.frame) * double(u01(rng));
        double got = evalSegment(x, y, f), want = refBezier(x, y, f);
        CHECK(std::fabs(got - want) < 0.05 + 1e-4 * std::fabs(want));
    }
    // Kasus tepi handle: outX=0,inX=0 dan outX=1,inX=1 tidak boleh NaN
    for (float ox : {0.f, 1.f})
        for (float ix : {0.f, 1.f}) {
            Keyframe x, y;
            x.frame = 0; y.frame = 10; y.value = 5;
            x.interp = Interp::Bezier; x.outX = ox; y.inX = ix; x.outY = 3; y.inY = -3;
            for (double f = 0; f <= 10; f += 0.25) CHECK(std::isfinite(evalSegment(x, y, f)));
        }
}

static Layer mkLayer(const char* n, int64_t in, int64_t out) {
    Layer l;
    l.name = n; l.inFrame = in; l.outFrame = out;
    return l;
}

static void testTimeMap() {
    Document d;
    const LayerId id = d.addLayer(mkLayer("T", 0, 100));
    CHECK(d.setKey(id, Prop::PosX, 0, 0));
    CHECK(d.setKey(id, Prop::PosX, 100, 100));          // PosX = frame (garis lurus)
    CHECK(near(evalTransform(*d.find(id), 50).posX, 50, 1e-3));
    CHECK(d.setKey(id, Prop::Time, 0, 0));
    CHECK(d.setKey(id, Prop::Time, 100, 50));           // 0.5x
    CHECK(near(layerTime(*d.find(id), 100), 50, 1e-3));
    CHECK(near(evalTransform(*d.find(id), 100).posX, 50, 1e-3));
    CHECK(near(evalTransform(*d.find(id), 50).posX, 25, 1e-3));
    CHECK(near(evalProp(*d.find(id), Prop::PosX, 100), 50, 1e-3));
    CHECK(near(evalProp(*d.find(id), Prop::Time, 100), 50, 1e-3));
    // parameter efek ikut waktu-layer dihitung engine lewat layerTime (cek rumusnya saja)
    CHECK(validate(d.project()));
    CHECK(d.shiftLayer(id, 10));
    const Track* tt = findTrack(*d.find(id), Prop::Time);
    CHECK(tt && tt->keys[0].frame == 10 && near(tt->keys[0].value, 10, 1e-6) && near(tt->keys[1].value, 60, 1e-6));
    CHECK(near(evalTransform(*d.find(id), 110).posX, 50, 1e-3));
    CHECK(d.undo() && d.redo());
    CHECK(d.clearTrack(id, Prop::Time, 10));
    CHECK(findTrack(*d.find(id), Prop::Time) == nullptr);
    CHECK(near(layerTime(*d.find(id), 33), 33, 1e-9));
}

static void testOps() {
    Document d;
    const LayerId id = d.addLayer(mkLayer("A", 0, 100));
    CHECK(d.setKey(id, Prop::PosX, 10, 0));
    CHECK(d.setKey(id, Prop::PosX, 30, 200));
    CHECK(d.setKey(id, Prop::PosX, 20, 50));  // sisip di tengah, terurut
    const Layer* l = d.find(id);
    CHECK(l->tracks.size() == 1 && l->tracks[0].keys.size() == 3);
    CHECK(l->tracks[0].keys[1].frame == 20);
    CHECK(d.setKey(id, Prop::Opacity, 0, 1.f));
    CHECK(d.find(id)->tracks.size() == 2 && d.find(id)->tracks[0].prop == Prop::PosX);  // terurut prop
    CHECK(!d.setKey(id, Prop::Opacity, 5, 1.5f));  // di luar 0..1 ditolak
    CHECK(!d.setKey(id, Prop::PosY, 5, NAN));
    CHECK(!d.setKey(999, Prop::PosX, 5, 1));

    // ganti nilai di frame yang sama tidak menambah keyframe
    CHECK(d.setKey(id, Prop::PosX, 20, 60));
    CHECK(d.find(id)->tracks[0].keys.size() == 3 && d.find(id)->tracks[0].keys[1].value == 60);

    // handle & easing
    CHECK(d.setKeyHandles(id, Prop::PosX, 10, 0.5f, 10, 0.5f, -10));
    CHECK(d.find(id)->tracks[0].keys[0].interp == Interp::Bezier);
    CHECK(d.find(id)->tracks[0].keys[0].outX == 0.5f && d.find(id)->tracks[0].keys[1].inY == -10);  // masuk milik keyframe berikutnya
    CHECK(!d.setKeyHandles(id, Prop::PosX, 30, 0.5f, 0, 0.5f, 0));  // keyframe terakhir
    CHECK(!d.setKeyHandles(id, Prop::PosX, 10, 1.5f, 0, 0.3f, 0));  // x di luar 0..1
    CHECK(!d.setKeyHandles(id, Prop::PosX, 10, 0.5f, NAN, 0.3f, 0));
    CHECK(d.setKeyEase(id, Prop::PosX, 10, 0.42f, 0, 0.58f, 1));
    CHECK(!d.setKeyEase(id, Prop::PosX, 30, 0.42f, 0, 0.58f, 1));  // keyframe terakhir
    CHECK(d.setKeyInterp(id, Prop::PosX, 20, Interp::Hold));
    CHECK(!d.setKeyInterp(id, Prop::PosX, 21, Interp::Hold));  // tidak ada keyframe di 21

    // Linear -> Bezier: bentuk kurva tidak berubah (handle setara garis lurus)
    {
        Document e;
        const LayerId q = e.addLayer(mkLayer("Q", 0, 100));
        e.setKey(q, Prop::PosX, 0, 0);
        e.setKey(q, Prop::PosX, 40, 80);
        const float before = evalProp(*e.find(q), Prop::PosX, 13);
        CHECK(e.setKeyInterp(q, Prop::PosX, 0, Interp::Bezier));
        CHECK(near(evalProp(*e.find(q), Prop::PosX, 13), before, 1e-3));
        // editKey: satu langkah undo
        const size_t ud = e.undoDepth();
        CHECK(e.editKey(q, Prop::PosX, 40, 60, 10));
        CHECK(e.undoDepth() == ud + 1);
        CHECK(e.find(q)->tracks[0].keys[1].frame == 60 && e.find(q)->tracks[0].keys[1].value == 10);
        CHECK(!e.editKey(q, Prop::PosX, 60, 0, 5));  // bentrok dengan keyframe di 0
        CHECK(e.editKey(q, Prop::PosX, 60, -5, 5));  // melompati: terurut ulang
        CHECK(e.find(q)->tracks[0].keys[0].frame == -5);
    }

    // pindah
    CHECK(d.moveKey(id, Prop::PosX, 20, 25));
    CHECK(d.find(id)->tracks[0].keys[1].frame == 25);
    CHECK(!d.moveKey(id, Prop::PosX, 25, 30));  // bentrok
    CHECK(d.moveKey(id, Prop::PosX, 25, 5));    // melompati keyframe lain: urutan dipulihkan
    CHECK(d.find(id)->tracks[0].keys[0].frame == 5 && d.find(id)->tracks[0].keys[1].frame == 10);

    // hapus sampai kosong: nilai dasar = nilai keyframe terakhir yang dihapus
    CHECK(d.removeKey(id, Prop::PosX, 5));
    CHECK(d.removeKey(id, Prop::PosX, 10));
    CHECK(d.removeKey(id, Prop::PosX, 30));
    CHECK(d.find(id)->tracks.size() == 1);  // tinggal opacity
    CHECK(near(d.find(id)->transform.posX, 200));
    CHECK(!d.removeKey(id, Prop::PosX, 30));

    // clearTrack: nilai dasar = nilai pada frame
    CHECK(d.setKey(id, Prop::PosY, 0, 0));
    CHECK(d.setKey(id, Prop::PosY, 10, 100));
    CHECK(d.clearTrack(id, Prop::PosY, 5));
    CHECK(near(d.find(id)->transform.posY, 50));
    CHECK(d.find(id)->tracks.size() == 1);

    // setTransformAt: properti beranimasi -> keyframe; lainnya -> nilai dasar
    Transform t = evalTransform(*d.find(id), 40);
    t.opacity = 0.25f;  // beranimasi (ada track)
    t.posX = 321;       // tidak beranimasi
    CHECK(d.setTransformAt(id, 40, t));
    CHECK(near(d.find(id)->transform.posX, 321));
    CHECK(near(evalProp(*d.find(id), Prop::Opacity, 40), 0.25));
    CHECK(d.find(id)->tracks[0].keys.size() == 2);
    const size_t rev = d.revision();
    CHECK(d.setTransformAt(id, 40, evalTransform(*d.find(id), 40)));  // tanpa perubahan
    CHECK(d.revision() == rev);

    // keyProps: beberapa properti sekaligus, satu undo, tampilan tidak berubah
    {
        Document e;
        Layer ql = mkLayer("K", 0, 50);
        ql.transform.posX = 12; ql.transform.rotation = 33;
        const LayerId q = e.addLayer(ql);
        const size_t ud = e.undoDepth();
        CHECK(e.keyProps(q, 7, (1u << int(Prop::PosX)) | (1u << int(Prop::Rotation))));
        CHECK(e.undoDepth() == ud + 1 && e.find(q)->tracks.size() == 2);
        CHECK(near(evalProp(*e.find(q), Prop::Rotation, 100), 33));
        CHECK(!e.keyProps(999, 7, 1));
    }

    // geser layer: keyframe ikut
    Layer sh = mkLayer("S", 10, 60);
    const LayerId sid = d.addLayer(sh);
    d.setKey(sid, Prop::Rotation, 10, 0);
    d.setKey(sid, Prop::Rotation, 50, 90);
    CHECK(d.shiftLayer(sid, 100));
    CHECK(d.find(sid)->tracks[0].keys[0].frame == 110 && d.find(sid)->tracks[0].keys[1].frame == 150);
    CHECK(near(evalProp(*d.find(sid), Prop::Rotation, 130), 45));
    CHECK(!d.shiftLayer(sid, -1000));  // key jadi negatif besar? (-890 masih sah) -> lihat bawah
}

static void testSplit() {
    Document d;
    const LayerId id = d.addLayer(mkLayer("A", 0, 100));
    d.setKey(id, Prop::PosX, 0, 0);
    d.setKey(id, Prop::PosX, 100, 300);
    d.setKeyEase(id, Prop::PosX, 0, 0.42f, 0, 0.58f, 1);
    const Layer before = *d.find(id);
    const LayerId rid = d.splitLayer(id, 40);
    CHECK(rid != 0);
    for (int f = 0; f < 100; f += 3) {
        const Layer* use = f < 40 ? d.find(id) : d.find(rid);
        CHECK(near(evalProp(*use, Prop::PosX, f), evalProp(before, Prop::PosX, f), 1e-6));
    }
    d.undo();
    CHECK(*d.find(id) == before);
}

static void testStorage(const std::string& dir) {
    Document d;
    const LayerId g = d.addLayer(LayerType::Group, "G", 0, 200);
    Layer a = mkLayer("anim ~ %", 0, 200);
    a.parent = g;
    const LayerId id = d.addLayer(a);
    d.setKey(id, Prop::PosX, 0, -123.456f);
    d.setKey(id, Prop::PosX, 77, 0.1f);
    d.setKey(id, Prop::Opacity, 0, 0.f);
    d.setKey(id, Prop::Opacity, 50, 1.f);
    d.setKeyHandles(id, Prop::PosX, 0, 0.123456789f, -77.7f, 0.9f, 1234.5f);
    d.setKeyInterp(id, Prop::Opacity, 0, Interp::Hold);

    Project back;
    std::string err;
    CHECK(deserialize(serialize(d.project()), &back, &err));
    CHECK(back == d.project());  // identik sampai bit terakhir

    const std::string path = dir + "/anim_test.studio";
    for (const char* s : {"", ".bak1", ".bak2", ".bak3", ".tmp", ".corrupt"}) std::remove((path + s).c_str());
    CHECK(saveProject(path, d.project()));
    LoadResult r = loadProject(path);
    CHECK(r.status == LoadStatus::Ok && r.project == d.project());

    // File v2 lama (tanpa animasi) tetap terbaca
    Project plain;
    Document pd;
    pd.addLayer(mkLayer("lama", 0, 50));
    pd.addLayer(mkLayer("lama2", 5, 60));
    std::string v4 = serialize(pd.project());
    auto downgrade = [&](int version) {
        size_t crcAt = v4.rfind("crc ");
        std::string src = v4.substr(0, crcAt), body;
        size_t pos = 0;
        while (pos < src.size()) {
            size_t nl = src.find('\n', pos);
            std::string line = src.substr(pos, nl - pos);
            pos = nl + 1;
            if (line == "STUDIOPROJECT 7") line = "STUDIOPROJECT " + std::to_string(version);
            else if (line.rfind("style ", 0) == 0 || line.rfind("effects ", 0) == 0) continue;
            else if (version == 2 && line.rfind("layer ", 0) == 0) line = line.substr(0, line.size() - 2);  // buang " 0"
            body += line + "\n";
        }
        char buf[32];
        std::snprintf(buf, sizeof buf, "crc %08x\n", crc32(body.data(), body.size()));
        return body + buf;
    };
    std::string v2 = downgrade(2);
    {
        Project v3p;
        CHECK(deserialize(downgrade(3), &v3p, &err));
        CHECK(v3p == pd.project());
    }
    Project old;
    CHECK(deserialize(v2, &old, &err));
    CHECK(old == pd.project());

    // Kerusakan: ubah satu karakter di bagian keyframe -> checksum menolak
    std::string text = serialize(d.project());
    size_t k = text.find("key ");
    text[k + 6] = text[k + 6] == '1' ? '2' : '1';
    CHECK(!deserialize(text, &back, &err));
    // Truncation di setiap titik harus ditolak tanpa crash
    std::string full = serialize(d.project());
    for (size_t n = 0; n < full.size(); n += 7) CHECK(!deserialize(full.substr(0, n), &back, nullptr));
}

static void testValidation() {
    Document d;
    const LayerId id = d.addLayer(mkLayer("A", 0, 10));
    Project p = d.project();
    Track t;
    t.prop = Prop::PosX;
    Keyframe a, b;
    a.frame = 5; b.frame = 5;  // tidak naik ketat
    t.keys = {a, b};
    p.layers[0].tracks = {t};
    CHECK(!validate(p));
    t.keys[1].frame = 6;
    p.layers[0].tracks = {t};
    CHECK(validate(p));
    t.keys[1].outX = -0.1f;
    p.layers[0].tracks = {t};
    CHECK(!validate(p));
    t.keys[1].outX = 0.5f;
    Track t2 = t;  // prop ganda
    p.layers[0].tracks = {t, t2};
    CHECK(!validate(p));
    p.layers[0].tracks = {Track{Prop::PosX, {}}};  // kosong
    CHECK(!validate(p));
    (void)id;
}

static void testFuzz() {
    std::mt19937 rng(12345);
    Document d;
    std::vector<LayerId> ids;
    int accepted = 0, rejected = 0, checks = 0;
    auto rf = [&](float lo, float hi) { return lo + (hi - lo) * float(rng() % 100001) / 100000.f; };
    for (int step = 0; step < 12000; ++step) {
        const Project snapshot = d.project();
        const size_t rev = d.revision();
        bool ok = false;
        const LayerId id = ids.empty() || rng() % 30 == 0 ? LayerId(rng() % 40) : ids[rng() % ids.size()];
        const Prop pr = static_cast<Prop>(rng() % (kPropCount + (rng() % 20 == 0 ? 3 : 0)));
        const int64_t fr = int64_t(rng() % 300) - (rng() % 10 == 0 ? 50 : 0);
        switch (rng() % 15) {
            case 0: { LayerId n = d.addLayer(mkLayer("f", int64_t(rng() % 50), 50 + int64_t(rng() % 250))); if (n) ids.push_back(n); ok = n != 0; break; }
            case 1: case 2: case 3: ok = d.setKey(id, pr, fr, rf(-500, 500) * (rng() % 50 == 0 ? NAN : 1.f)); break;
            case 4: ok = d.removeKey(id, pr, fr); break;
            case 5: ok = d.moveKey(id, pr, fr, int64_t(rng() % 300)); break;
            case 6: ok = d.setKeyInterp(id, pr, fr, static_cast<Interp>(rng() % 3)); break;
            case 7: ok = d.setKeyHandles(id, pr, fr, rf(-0.2f, 1.2f), rf(-900, 900), rf(-0.2f, 1.2f), rf(-900, 900)); break;
            case 8: ok = d.setKeyEase(id, pr, fr, rf(0, 1), rf(-1, 2), rf(0, 1), rf(-1, 2)); break;
            case 9: ok = d.clearTrack(id, pr, fr); break;
            case 10: { Transform t; t.posX = rf(-100, 100); t.opacity = rf(-0.1f, 1.1f); t.rotation = rf(-90, 90); ok = d.setTransformAt(id, fr, t); break; }
            case 11: ok = d.shiftLayer(id, int64_t(rng() % 100) - 50); break;
            case 12: { LayerId n = d.splitLayer(id, fr); if (n) ids.push_back(n); ok = n != 0; break; }
            case 14: ok = d.editKey(id, pr, fr, int64_t(rng() % 300) - 20, rf(-300, 300)); break;
            case 13: { int w = rng() % 3; ok = w == 0 ? d.undo() : w == 1 ? d.redo() : d.removeLayer(id); break; }
        }
        ok ? ++accepted : ++rejected;
        CHECK(validate(d.project()));
        if (!ok && d.revision() == rev) CHECK(d.project() == snapshot);
        // evaluasi selalu terhingga dan opacity 0..1
        if (step % 200 == 0) {
            for (const Layer& l : d.project().layers)
                for (double f : {-10.0, 0.0, 33.3, 150.0, 400.0}) {
                    Transform t = evalTransform(l, f);
                    CHECK(t.finite() && t.opacity >= 0.f && t.opacity <= 1.f);
                    ++checks;
                }
            Project back;
            CHECK(deserialize(serialize(d.project()), &back) && back == d.project());
            std::vector<Resolved> sc;
            resolveScene(d.project(), 77.5, 0, nullptr, &sc);
            CHECK(sc.size() == d.project().layers.size());
        }
    }
    std::printf("fuzz anim: %d diterima, %d ditolak, %zu layer di akhir, %d evaluasi\n", accepted, rejected,
                d.project().layers.size(), checks);
}

static void testEaseKey() {
    Document d;
    const LayerId id = d.addLayer(mkLayer("E", 0, 100));
    CHECK(d.setKey(id, Prop::PosX, 0, 0));
    CHECK(d.setKey(id, Prop::PosX, 20, 100));
    CHECK(d.setKey(id, Prop::PosX, 40, 0));
    CHECK(d.easeKey(id, Prop::PosX, 20, 0));  // Easy Ease
    const Track& t = d.find(id)->tracks[0];
    CHECK(t.keys[0].interp == Interp::Bezier && t.keys[1].interp == Interp::Bezier);
    CHECK(near(t.keys[1].inY, 0) && near(t.keys[1].outY, 0));
    CHECK(near(evalTrack(t, 20), 100));
    CHECK(evalTrack(t, 10) > 50 && evalTrack(t, 10) < 100);  // melambat menuju puncak (sisi masuk datar)
    CHECK(d.easeKey(id, Prop::PosX, 20, 3));  // Auto Bezier: puncak datar (tetangga sama)
    CHECK(near(d.find(id)->tracks[0].keys[1].outY, 0));
    CHECK(d.setKey(id, Prop::PosX, 60, 300));
    CHECK(d.easeKey(id, Prop::PosX, 40, 3));  // ada kemiringan
    CHECK(d.find(id)->tracks[0].keys[2].outY > 0);
    CHECK(d.easeKey(id, Prop::PosX, 40, 4));  // Linear kembali
    CHECK(near(evalTrack(d.find(id)->tracks[0], 50), 150, 0.5));
    CHECK(!d.easeKey(id, Prop::PosX, 41, 0));
    CHECK(!d.easeKey(id, Prop::PosX, 40, 9));
    Document s;
    const LayerId q = s.addLayer(mkLayer("S", 0, 100));
    s.setKey(q, Prop::PosX, 0, 5);
    CHECK(!s.easeKey(q, Prop::PosX, 0, 0));  // satu keyframe saja
}

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : ".";
    testEval();
    testOps();
    testTimeMap();
    testSplit();
    testStorage(dir);
    testValidation();
    testEaseKey();
    testFuzz();
    std::printf("anim: %d pemeriksaan, %d gagal\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
