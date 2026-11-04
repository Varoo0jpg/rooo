// Uji bentuk, blend mode, gambar, efek (parameter beranimasi), penyimpanan v4, fuzz.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>

#include "effects.h"
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

static Layer mk(const char* n, int64_t in, int64_t out) {
    Layer l;
    l.name = n; l.inFrame = in; l.outFrame = out;
    return l;
}

static void testShapeBlend() {
    Document d;
    const LayerId id = d.addLayer(mk("A", 0, 100));
    CHECK(d.setShape(id, ShapeKind::Star, 0, 7, 0.4f));
    CHECK(d.find(id)->shape == ShapeKind::Star && d.find(id)->sides == 7);
    CHECK(!d.setShape(id, ShapeKind::Star, 0, 2, 0.4f));     // sisi < 3
    CHECK(!d.setShape(id, ShapeKind::Star, 0, 65, 0.4f));
    CHECK(!d.setShape(id, ShapeKind::Star, 0, 5, 0.0f));     // rasio terlalu kecil
    CHECK(!d.setShape(id, ShapeKind::Star, -1, 5, 0.5f));    // radius negatif
    CHECK(!d.setShape(id, ShapeKind::Star, NAN, 5, 0.5f));
    CHECK(!d.setShape(id, ShapeKind::Count, 0, 5, 0.5f));
    CHECK(d.setShape(id, ShapeKind::RoundRect, 40, 5, 0.5f));
    CHECK(d.setBlend(id, BlendMode::Screen));
    CHECK(!d.setBlend(id, BlendMode::Count));
    CHECK(d.find(id)->blend == BlendMode::Screen);
    CHECK(!d.setShape(999, ShapeKind::Rect, 0, 5, 0.5f));
}

static void testImage() {
    Document d;
    Layer im = mk("gambar", 0, 100);
    im.type = LayerType::Image;
    CHECK(d.addLayer(im) == 0);                 // Image tanpa nama file ditolak
    for (const char* bad : {"../x.png", "a/b.png", "a\\b.png", "..", "a b.png", ".."})
        { im.image = bad; CHECK(d.addLayer(im) == 0); }
    im.image = "img_123.png";
    CHECK(d.addLayer(im) != 0);
    Layer sh = mk("s", 0, 10);
    sh.image = "../../etc/passwd";               // path berbahaya ditolak walau bukan Image
    CHECK(d.addLayer(sh) == 0);
    // simpan-muat menjaga nama
    Project back;
    CHECK(deserialize(serialize(d.project()), &back) && back == d.project());
    CHECK(back.layers[0].image == "img_123.png");
}

static void testEffects(const std::string& dir) {
    Document d;
    const LayerId id = d.addLayer(mk("A", 0, 300));
    const EffectDef* blur = findEffectDef("gaussian_blur");
    const EffectDef* glow = findEffectDef("glow");
    CHECK(blur && glow);
    CHECK(d.addEffect(id, "gaussian_blur", defaultParams(*blur)) == 0);
    CHECK(d.addEffect(id, "glow", defaultParams(*glow)) == 1);
    CHECK(d.addEffect(id, "Bad Type!", {1.f}) == -1);          // karakter tidak sah
    CHECK(d.addEffect(id, "x", std::vector<float>(33, 0.f)) == -1);
    CHECK(d.addEffect(id, "x", {NAN}) == -1);
    CHECK(d.addEffect(999, "glow", {}) == -1);
    const LayerId g = d.addLayer(LayerType::Group, "G", 0, 10);
    CHECK(d.addEffect(g, "glow", defaultParams(*glow)) == -1);  // Group tidak punya efek

    // animasi parameter lewat kanal
    const Prop radius = effectChannel(0, 0);
    CHECK(d.setKey(id, radius, 0, 0.f));
    CHECK(d.setKey(id, radius, 60, 50.f));
    CHECK(near(evalProp(*d.find(id), radius, 30), 25.f));
    CHECK(near(baseValue(*d.find(id), radius), 10.f));
    float p[kMaxEffectParams];
    evalEffectParams(d.find(id)->effects[0], 30, p);
    CHECK(near(p[0], 25.f));
    CHECK(d.find(id)->effects[0].tracks.size() == 1 && d.find(id)->tracks.empty());
    CHECK(d.setKeyEase(id, radius, 0, 0.42f, 0, 0.58f, 1));
    CHECK(near(evalProp(*d.find(id), radius, 30), 25.f, 1e-2));
    CHECK(d.editKey(id, radius, 60, 90, 80.f));
    CHECK(d.find(id)->effects[0].tracks[0].keys[1].frame == 90);
    CHECK(!d.setKey(id, effectChannel(0, 5), 0, 1.f));          // parameter tidak ada
    CHECK(!d.setKey(id, effectChannel(7, 0), 0, 1.f));          // efek tidak ada
    CHECK(d.setPropAt(id, effectChannel(1, 1), 10, 2.5f));      // tanpa track -> nilai dasar
    CHECK(near(baseValue(*d.find(id), effectChannel(1, 1)), 2.5f));
    CHECK(d.setPropAt(id, radius, 45, 7.f));                    // dengan track -> keyframe
    CHECK(d.find(id)->effects[0].tracks[0].keys.size() == 3);

    // urutan efek: track ikut efeknya
    const Layer before = *d.find(id);
    CHECK(d.moveEffect(id, 0, 1));
    CHECK(d.find(id)->effects[1].type == "gaussian_blur" && d.find(id)->effects[1].tracks.size() == 1);
    CHECK(d.find(id)->effects[0].type == "glow");
    CHECK(!d.moveEffect(id, 0, 5));
    CHECK(d.setEffectEnabled(id, 0, false) && !d.find(id)->effects[0].enabled);
    CHECK(d.removeEffect(id, 0));
    CHECK(d.find(id)->effects.size() == 1 && d.find(id)->effects[0].type == "gaussian_blur");
    CHECK(!d.removeEffect(id, 3));
    d.undo(); d.undo(); d.undo();
    (void)before;

    // hapus keyframe terakhir mengembalikan nilai dasar parameter
    {
        Document e;
        const LayerId q = e.addLayer(mk("Q", 0, 100));
        e.addEffect(q, "gaussian_blur", {12.f});
        e.setKey(q, effectChannel(0, 0), 5, 33.f);
        CHECK(e.removeKey(q, effectChannel(0, 0), 5));
        CHECK(e.find(q)->effects[0].tracks.empty() && near(e.find(q)->effects[0].params[0], 33.f));
        e.setKey(q, effectChannel(0, 0), 5, 1.f);
        CHECK(e.clearTrack(q, effectChannel(0, 0), 50));
        CHECK(near(e.find(q)->effects[0].params[0], 1.f));
    }

    // potong & geser menyertakan efek dan animasinya
    {
        Document e;
        const LayerId q = e.addLayer(mk("Q", 0, 100));
        e.addEffect(q, "glow", defaultParams(*glow));
        e.setKey(q, effectChannel(0, 1), 0, 0.f);
        e.setKey(q, effectChannel(0, 1), 80, 4.f);
        const LayerId r = e.splitLayer(q, 40);
        CHECK(r && e.find(r)->effects.size() == 1 && e.find(r)->effects[0].tracks.size() == 1);
        for (int f = 0; f < 100; f += 7) {
            const Layer* u = f < 40 ? e.find(q) : e.find(r);
            CHECK(near(evalProp(*u, effectChannel(0, 1), f), std::min(f, 80) * 4.0 / 80.0, 1e-4));
        }
        CHECK(e.shiftLayer(q, 50));
        CHECK(e.find(q)->effects[0].tracks[0].keys[0].frame == 50);
    }

    // penyimpanan v4 identik sampai bit terakhir
    Project back;
    std::string err;
    CHECK(deserialize(serialize(d.project()), &back, &err));
    CHECK(back == d.project());
    const std::string path = dir + "/fx_test.studio";
    for (const char* s : {"", ".bak1", ".bak2", ".bak3", ".tmp", ".corrupt"}) std::remove((path + s).c_str());
    CHECK(saveProject(path, d.project()));
    LoadResult r = loadProject(path);
    CHECK(r.status == LoadStatus::Ok && r.project == d.project());
    // pemotongan di titik mana pun ditolak
    std::string full = serialize(d.project());
    for (size_t n = 0; n < full.size(); n += 5) CHECK(!deserialize(full.substr(0, n), &back, nullptr));
}

static void testVideo() {
    Document d;
    Layer v = mk("V", 10, 110);
    v.type = LayerType::Video;
    v.image = "vid_abc";
    v.srcStart = 0; v.srcCount = 100; v.rate = 1.f;
    const LayerId id = d.addLayer(v);
    CHECK(id != 0);
    CHECK(videoSrcIndex(*d.find(id), 10) == 0 && videoSrcIndex(*d.find(id), 35) == 25);
    CHECK(videoSrcIndex(*d.find(id), 500) == 99 && videoSrcIndex(*d.find(id), -5) == 0);   // dijepit
    CHECK(videoFrameName(*d.find(id), 35) == "vid_abc/000025.jpg");
    // potong depan: isi tidak bergeser
    CHECK(d.trimLayer(id, 30, 110));
    CHECK(videoSrcIndex(*d.find(id), 40) == 30);
    // belah: kedua bagian menampilkan frame yang sama dengan sebelum dibelah
    const LayerId r = d.splitLayer(id, 70);
    CHECK(r != 0 && videoSrcIndex(*d.find(r), 80) == 70 && videoSrcIndex(*d.find(id), 50) == 40);
    // kurva waktu: setengah kecepatan
    CHECK(d.setKey(r, Prop::Time, 70, 70));
    CHECK(d.setKey(r, Prop::Time, 110, 90));
    CHECK(videoSrcIndex(*d.find(r), layerTime(*d.find(r), 110)) == 60 + 20);
    // kecepatan
    CHECK(d.scaleLayerTime(id, 2.0) && near(d.find(id)->rate, 2.0));
    // simpan & muat
    Project back;
    std::string err;
    CHECK(deserialize(serialize(d.project()), &back, &err) && back == d.project());
    // tidak valid
    Layer bad = v;
    bad.srcCount = 0;
    CHECK(d.addLayer(bad) == 0);
    bad = v; bad.rate = 50.f;
    CHECK(d.addLayer(bad) == 0);
    bad = v; bad.image = "a/b";
    CHECK(d.addLayer(bad) == 0);
}

static void testAudio() {
    Document d;
    Layer a = mk("A", 10, 110);
    a.type = LayerType::Audio;
    a.image = "vid_abc";
    a.srcStart = 0; a.srcCount = 100; a.rate = 1.f;
    const LayerId id = d.addLayer(a);
    CHECK(id != 0);
    CHECK(d.trimLayer(id, 30, 110) && d.find(id)->srcStart == 20);
    const LayerId r = d.splitLayer(id, 70);
    CHECK(r != 0 && d.find(r)->srcStart == 60 && d.find(r)->type == LayerType::Audio);
    CHECK(d.addEffect(id, "blur", {}) < 0);   // suara tidak punya efek
    Project back;
    std::string err;
    CHECK(deserialize(serialize(d.project()), &back, &err) && back == d.project());
    Layer bad = a;
    bad.image = "a/b";
    CHECK(d.addLayer(bad) == 0);
    bad = a; bad.srcCount = 0;
    CHECK(d.addLayer(bad) == 0);
}

static void testText() {
    Document d;
    Layer t = mk("Halo", 0, 90);
    t.type = LayerType::Text;
    t.image = "txt_ab12.png";
    t.text = "Halo dunia\nbaris 2 | %\\ ~ \xE2\x9C\x93";
    t.fontSize = 120.f; t.textStyle = 3 | (2 << 2);
    t.width = 400.f; t.height = 160.f;
    const LayerId id = d.addLayer(t);
    CHECK(id != 0);
    Project back;
    std::string err;
    CHECK(deserialize(serialize(d.project()), &back, &err) && back == d.project());
    CHECK(back.layers[0].text == t.text && near(back.layers[0].fontSize, 120.0) && back.layers[0].textStyle == t.textStyle);
    CHECK(d.setText(id, "txt_cd34.png", 500.f, 200.f, "Baru", 64.f, 1));
    CHECK(d.find(id)->text == "Baru" && d.find(id)->image == "txt_cd34.png" && near(d.find(id)->width, 500.0));
    CHECK(d.undo() && d.find(id)->text == t.text);
    CHECK(!d.setText(id, "a/b", 500.f, 200.f, "x", 64.f, 0));      // nama berkas tidak valid
    CHECK(!d.setText(id, "ok.png", 500.f, 200.f, "x", 1.f, 0));    // ukuran huruf terlalu kecil
    CHECK(!d.setText(id, "ok.png", 500.f, 200.f, std::string(3000, 'a'), 64.f, 0));
    CHECK(d.addEffect(id, "blur", {}) >= 0);                       // efek boleh dipakai pada teks
    Layer shape = mk("S", 0, 90);
    const LayerId sid = d.addLayer(shape);
    CHECK(!d.setText(sid, "ok.png", 10.f, 10.f, "x", 64.f, 0));    // bukan layer Teks
    Layer bad = t;
    bad.image = "";
    CHECK(d.addLayer(bad) == 0);
}

static void testMatte() {
    Document d;
    Layer a = mk("A", 0, 90);
    Layer b = mk("B", 0, 90);
    const LayerId ia = d.addLayer(a), ib = d.addLayer(b);
    CHECK(d.setMatte(ib, 1) && d.find(ib)->matte == 1);
    CHECK(d.setMatte(ib, 2) && d.find(ib)->matte == 2);
    CHECK(!d.setMatte(ib, 3) && !d.setMatte(ib, -1));
    Project back;
    std::string err;
    CHECK(deserialize(serialize(d.project()), &back, &err) && back == d.project());
    CHECK(back.layers[1].matte == 2 && back.layers[0].matte == 0);
    CHECK(d.undo() && d.find(ib)->matte == 1);
    Layer g = mk("G", 0, 90);
    g.type = LayerType::Group;
    const LayerId ig = d.addLayer(g);
    CHECK(!d.setMatte(ig, 1));
    Layer bad = a;
    bad.type = LayerType::Null;
    bad.matte = 1;
    CHECK(d.addLayer(bad) == 0);
    (void)ia;
}

static void testGrouping() {
    Document d;
    Layer a = mk("A", 0, 90), b = mk("B", 0, 90), c = mk("C", 0, 90);
    a.transform.posX = 100; a.transform.posY = 40;
    b.transform.posX = 300; b.transform.posY = 80;
    const LayerId ia = d.addLayer(a), ib = d.addLayer(b), ic = d.addLayer(c);
    CHECK(d.setKey(ia, Prop::PosX, 0, 100.f) && d.setKey(ia, Prop::PosX, 10, 150.f));
    const size_t u0 = d.undoDepth();
    const LayerId gid = d.groupLayers({ia, ib}, "Grup X", 0);
    CHECK(gid != 0 && d.undoDepth() == u0 + 1 && validate(d.project()));
    CHECK(d.find(ia)->parent == gid && d.find(ib)->parent == gid && d.find(ic)->parent == 0);
    CHECK(near(d.find(gid)->transform.posX, 200.0) && near(d.find(gid)->transform.posY, 60.0));   // pusat kedua layer
    CHECK(near(d.find(ia)->transform.posX, -100.0) && near(d.find(ib)->transform.posX, 100.0));
    CHECK(near(d.find(ia)->tracks[0].keys[0].value, -100.0) && near(d.find(ia)->tracks[0].keys[1].value, -50.0));   // keyframe ikut dikompensasi
    // tampilan tidak berubah: posisi dunia sama dengan sebelumnya
    std::vector<Resolved> sc;
    resolveScene(d.project(), 0, 0, nullptr, &sc);
    size_t ai = 0;
    for (size_t i = 0; i < d.project().layers.size(); ++i) if (d.project().layers[i].id == ia) ai = i;
    CHECK(near(sc[ai].world.tx, 100.0) && near(sc[ai].world.ty, 40.0));
    // induk bukan Grup (Parent & Link): hanya transform diwarisi, bukan opasitas
    CHECK(d.setParent(ic, ib));
    CHECK(validate(d.project()));
    // hapus layer biasa tidak menghapus anaknya; hapus Grup menghapus isinya
    CHECK(d.removeLayer(ib) && d.find(ic) != nullptr && d.find(ic)->parent == gid);
    CHECK(d.undo());
    CHECK(d.ungroup(gid) && d.find(gid) == nullptr && d.find(ia)->parent == 0);
    CHECK(near(d.find(ia)->tracks[0].keys[1].value, 150.0) && near(d.find(ib)->transform.posX, 300.0));      // posisi kembali
    CHECK(!d.ungroup(ia));
    CHECK(d.groupLayers({}, "x", 0) == 0);
    CHECK(d.groupLayers({999}, "x", 0) == 0);
    const LayerId g2 = d.groupLayers({ia}, "Satu", 0);
    CHECK(g2 != 0 && d.removeLayer(g2) && d.find(ia) == nullptr);    // hapus Grup = hapus isinya
    CHECK(validate(d.project()));
}

static void testReset() {
    Document d;
    Layer a = mk("A", 0, 90);
    a.transform.rotation = 30.f;
    const LayerId id = d.addLayer(a);
    CHECK(d.setKey(id, Prop::Rotation, 0, 0.f) && d.setKey(id, Prop::Rotation, 30, 720.f));
    CHECK(d.setKey(id, Prop::PosX, 0, 5.f));
    const size_t u0 = d.undoDepth();
    CHECK(d.resetProp(id, Prop::Rotation, 0.f));
    CHECK(d.undoDepth() == u0 + 1);
    for (const Track& t : d.find(id)->tracks) CHECK(t.prop != Prop::Rotation);      // semua keyframe hilang
    CHECK(near(d.find(id)->transform.rotation, 0.0));
    CHECK(near(evalProp(*d.find(id), Prop::Rotation, 20), 0.0));
    CHECK(d.find(id)->tracks.size() == 1);                                           // kanal lain tidak tersentuh
    CHECK(d.undo() && near(evalProp(*d.find(id), Prop::Rotation, 30), 720.0));
    CHECK(!d.resetProp(id, static_cast<Prop>(300), 0.f));                            // kanal efek yang tidak ada
    CHECK(d.addEffect(id, "blur", {}) >= 0);
    CHECK(d.setHidden(id, true) && d.find(id)->hidden);
    Project back;
    std::string err;
    CHECK(deserialize(serialize(d.project()), &back, &err) && back == d.project() && back.layers[0].hidden);
    std::vector<Resolved> sc;
    resolveScene(d.project(), 10, 0, nullptr, &sc);
    CHECK(!sc[0].visible);
    CHECK(d.setHidden(id, false));
    resolveScene(d.project(), 10, 0, nullptr, &sc);
    CHECK(sc[0].visible);
}

static void testPresets() {
    for (int pi = 0; pi < presetCount(); ++pi) {
        Document d;
        Layer a = mk("P", 30, 150);
        a.width = 200.f; a.height = 100.f;
        const LayerId id = d.addLayer(a);
        const size_t u0 = d.undoDepth();
        CHECK(d.applyPreset(id, pi, 15));
        CHECK(d.undoDepth() == u0 + 1);
        CHECK(validate(d.project()));
        const Layer& l = *d.find(id);
        const int cat = presetCategory(pi);
        if (cat == 0) {   // masuk: di akhir jendela semua kanal kembali ke nilai awal layer
            CHECK(near(evalProp(l, Prop::PosX, 45), 0.0) && near(evalProp(l, Prop::PosY, 45), 0.0));
            CHECK(near(evalProp(l, Prop::Opacity, 45), 1.0) && near(evalProp(l, Prop::ScaleX, 45), 1.0, 1e-3));
            CHECK(near(evalProp(l, Prop::Rotation, 45), 0.0, 1e-3));
            CHECK(near(evalProp(l, Prop::PosX, 100), 0.0) && near(evalProp(l, Prop::Opacity, 140), 1.0));
        } else if (cat == 1) {   // keluar: sebelum jendela normal
            CHECK(near(evalProp(l, Prop::Opacity, 60), 1.0) && near(evalProp(l, Prop::ScaleX, 60), 1.0, 1e-3));
            CHECK(near(evalProp(l, Prop::PosX, 60), 0.0) && near(evalProp(l, Prop::Rotation, 60), 0.0, 1e-3));
        } else {
            CHECK(l.tracks.size() >= 1);
        }
        // dipakai dua kali: menggantikan, tetap sah
        CHECK(d.applyPreset(id, pi, 15) && validate(d.project()));
        // simpan & muat
        Project back;
        std::string err;
        CHECK(deserialize(serialize(d.project()), &back, &err) && back == d.project());
        CHECK(d.undo() && d.undo() == true);   // dua kali tempel = dua langkah
    }
    // masuk + keluar bersamaan di layer yang sama
    {
        Document d;
        Layer a = mk("Q", 0, 120);
        const LayerId id = d.addLayer(a);
        int fi = -1, fo = -1;
        for (int i = 0; i < presetCount(); ++i) {
            if (std::string(presetId(i)) == "fade_in") fi = i;
            if (std::string(presetId(i)) == "fade_out") fo = i;
        }
        CHECK(fi >= 0 && fo >= 0 && d.applyPreset(id, fi, 20) && d.applyPreset(id, fo, 20));
        const Layer& l = *d.find(id);
        CHECK(near(evalProp(l, Prop::Opacity, 0), 0.0) && near(evalProp(l, Prop::Opacity, 20), 1.0) &&
              near(evalProp(l, Prop::Opacity, 60), 1.0) && near(evalProp(l, Prop::Opacity, 119), 0.0, 1e-3));
        CHECK(!d.applyPreset(id, 999, 20) && !d.applyPreset(999999, 0, 20));
    }
    // suara tidak bisa diberi preset
    {
        Document d;
        Layer a = mk("A", 0, 100);
        a.type = LayerType::Audio; a.image = "aud_x"; a.srcCount = 100;
        const LayerId id = d.addLayer(a);
        CHECK(!d.applyPreset(id, 0, 10));
    }
}

static void testFuzz() {
    std::mt19937 rng(2024);
    Document d;
    std::vector<LayerId> ids;
    const auto& cat = effectCatalog();
    int acc = 0, rej = 0;
    auto rf = [&](float lo, float hi) { return lo + (hi - lo) * float(rng() % 100001) / 100000.f; };
    for (int step = 0; step < 8000; ++step) {
        const Project snap = d.project();
        const size_t rev = d.revision();
        bool ok = false;
        const LayerId id = ids.empty() || rng() % 25 == 0 ? LayerId(rng() % 30) : ids[rng() % ids.size()];
        const int fx = int(rng() % 4), pi = int(rng() % 6);
        switch (rng() % 12) {
            case 0: { Layer l = mk("f", rng() % 50, 60 + rng() % 200); l.type = rng() % 8 == 0 ? LayerType::Image : LayerType::Shape; if (l.type == LayerType::Image) l.image = "i.png"; LayerId n = d.addLayer(l); if (n) ids.push_back(n); ok = n != 0; break; }
            case 1: case 2: { const EffectDef& e = cat[rng() % cat.size()]; ok = d.addEffect(id, e.type, defaultParams(e)) >= 0; break; }
            case 3: ok = d.removeEffect(id, fx); break;
            case 4: ok = d.moveEffect(id, fx, int(rng() % 4)); break;
            case 5: ok = d.setEffectEnabled(id, fx, rng() % 2); break;
            case 6: case 7: ok = d.setKey(id, effectChannel(fx, pi), int64_t(rng() % 200), rf(-50, 50) * (rng() % 40 == 0 ? NAN : 1.f)); break;
            case 8: ok = d.setPropAt(id, effectChannel(fx, pi), int64_t(rng() % 200), rf(-5, 5)); break;
            case 9: ok = d.setShape(id, static_cast<ShapeKind>(rng() % 6), rf(-5, 50), int(rng() % 70), rf(0, 1.1f)); break;
            case 10: ok = d.setBlend(id, static_cast<BlendMode>(rng() % 14)); break;
            case 11: { int w = rng() % 4; ok = w == 0 ? d.undo() : w == 1 ? d.redo() : w == 2 ? d.splitLayer(id, int64_t(rng() % 200)) != 0 : d.shiftLayer(id, int64_t(rng() % 40) - 20); break; }
        }
        ok ? ++acc : ++rej;
        CHECK(validate(d.project()));
        if (!ok && d.revision() == rev) CHECK(d.project() == snap);
        if (step % 100 == 0) {
            Project back;
            CHECK(deserialize(serialize(d.project()), &back) && back == d.project());
            for (const Layer& l : d.project().layers)
                for (size_t e = 0; e < l.effects.size(); ++e) {
                    float p[kMaxEffectParams];
                    evalEffectParams(l.effects[e], 12.5, p);
                    for (float v : p) CHECK(std::isfinite(v));
                }
        }
    }
    std::printf("fuzz fx: %d diterima, %d ditolak, %zu layer\n", acc, rej, d.project().layers.size());
}

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : ".";
    testShapeBlend();
    testImage();
    testVideo();
    testAudio();
    testText();
    testMatte();
    testReset();
    testGrouping();
    testPresets();
    testEffects(dir);
    testFuzz();
    std::printf("fx: %d pemeriksaan, %d gagal\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
