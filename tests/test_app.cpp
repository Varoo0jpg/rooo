// Uji logika aplikasi: demo, seret, potong, undo, autosave, buka ulang.
#include <cmath>
#include "model/anim.h"
#include <cstdio>
#include <cstdlib>
#include <string>

#include "app.h"
#include "model/storage.h"

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

static LayerId idByName(const Project& p, const char* n) {
    for (auto& l : p.layers) if (l.name == n) return l.id;
    return 0;
}

static bool near(double a, double b, double e) { return std::fabs(a - b) <= e; }

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : ".";
    std::string path = dir + "/app_test.studio";
    for (const char* suf : {"", ".bak1", ".bak2", ".bak3", ".tmp", ".corrupt"}) {
        std::remove((path + suf).c_str());
        std::remove((dir + "/app_anim_test.studio" + suf).c_str());
    }

    CHECK(validate(makeDemoProject()));

    App a;
    a.open(path);
    a.setViewSize(1080, 1920);  // fitScale = 0.94
    a.tick(0);
    CHECK(a.project().layers.size() == 5);
    a.tick(1500);  // autosave pertama (demo baru)
    CHECK(a.info().find("tersimpan") != std::string::npos);

    // ketuk "Batang" (frame 0 belum terlihat -> tidak kena), pindah ke frame 100
    a.seek(100);
    const float s = fitScale(1080, 1920, 1080, 1920);
    auto toView = [&](float cx, float cy, float* vx, float* vy) {
        *vx = cx * s + 540.f;
        *vy = cy * s + 960.f;
    };
    float vx, vy;
    toView(0, 500, &vx, &vy);
    a.touch(TouchAction::Down, vx, vy);
    CHECK(a.selected() == idByName(a.project(), "Batang"));
    // seret 100 px layar ke kanan, 50 ke bawah
    a.touch(TouchAction::Move, vx + 100 * s, vy + 50 * s);
    CHECK(a.project().layers[4].transform.posX == 0);  // belum di-commit saat menyeret
    std::vector<Resolved> sc;
    a.sceneNow(&sc);
    float wx, wy;
    sc[4].world.apply(0, 0, &wx, &wy);
    CHECK(std::fabs(wx - 100) < 0.01f && std::fabs(wy - 550) < 0.01f);
    a.touch(TouchAction::Up, vx + 100 * s, vy + 50 * s);
    CHECK(std::fabs(a.project().layers[4].transform.posX - 100) < 0.01f);
    CHECK(a.document().undoDepth() == 1);

    // seret anak dalam Group yang berputar: hasil dunia harus mengikuti jari
    a.seek(100);
    const Layer* ka = a.document().find(idByName(a.project(), "Kotak A"));
    std::vector<Resolved> sc2;
    a.sceneNow(&sc2);
    size_t ia = 0;
    for (size_t i = 0; i < a.project().layers.size(); ++i)
        if (a.project().layers[i].id == ka->id) ia = i;
    float ax, ay;
    sc2[ia].world.apply(0, 0, &ax, &ay);
    toView(ax, ay, &vx, &vy);
    a.touch(TouchAction::Down, vx, vy);
    CHECK(a.selected() == ka->id);
    a.touch(TouchAction::Move, vx + 60 * s, vy - 40 * s);
    a.touch(TouchAction::Up, vx + 60 * s, vy - 40 * s);
    std::vector<Resolved> sc3;
    a.sceneNow(&sc3);
    float bx, by;
    sc3[ia].world.apply(0, 0, &bx, &by);
    CHECK(std::fabs(bx - (ax + 60)) < 0.05f && std::fabs(by - (ay - 40)) < 0.05f);

    // potong / undo / redo / hapus
    const size_t before = a.project().layers.size();
    a.command(Cmd::Split);  // frame 100, Kotak A terpilih
    CHECK(a.project().layers.size() == before + 1);
    a.command(Cmd::Delete);  // hapus potongan kanan yang terpilih
    CHECK(a.project().layers.size() == before);
    a.command(Cmd::Undo);
    CHECK(a.project().layers.size() == before + 1);
    a.command(Cmd::Redo);
    CHECK(a.project().layers.size() == before);
    a.command(Cmd::Undo);  // kembali ke keadaan setelah potong
    a.command(Cmd::Undo);  // kembali sebelum potong
    CHECK(a.project().layers.size() == before);
    a.command(Cmd::Redo);
    a.command(Cmd::Redo);
    CHECK(a.selected() == 0);
    a.command(Cmd::Split);  // tidak ada pilihan: tidak mengubah apa pun
    CHECK(a.project().layers.size() == before);
    a.command(Cmd::AddShape);
    CHECK(a.project().layers.size() == before + 1);
    CHECK(a.selected() != 0);

    // tambah di akhir waktu tidak boleh membuat layer tidak sah
    a.seek(299);
    a.command(Cmd::AddShape);
    CHECK(validate(a.project()));
    CHECK(a.project().durationFrames == 389);   // durasi tumbuh mengikuti layer baru

    // putar: 30 fps, 1 detik = 30 frame, melingkar di 300
    a.seek(10);
    a.tick(10000);
    a.togglePlay();
    a.tick(11000);
    CHECK(a.frame() == 40);
    a.tick(10000 + 1000 * 10);  // 10 detik -> 300 frame -> kembali ke 10 + 300 % 300
    CHECK(a.frame() == (10 + 300) % a.project().durationFrames);
    a.togglePlay();

    // autosave lalu buka ulang: isi harus identik
    a.tick(20000);
    a.tick(21500);
    Project saved = a.project();
    App b;
    b.open(path);
    CHECK(b.project() == saved);
    CHECK(b.info().find("Proyek dibuka") != std::string::npos);

    // jam mundur tidak boleh membekukan autosave
    a.command(Cmd::AddShape);
    a.tick(100);
    a.tick(1200);
    App c;
    c.open(path);
    CHECK(c.project() == a.project());

    // file utama dirusak -> pulih dari cadangan
    {
        FILE* f = std::fopen(path.c_str(), "wb");
        std::fputs("sampah", f);
        std::fclose(f);
    }
    App d;
    d.open(path);
    CHECK(d.info().find("cadangan") != std::string::npos);
    CHECK(validate(d.project()));

    // --- animasi lewat App ---
    {
        App e;
        e.open(dir + "/app_anim_test.studio");
        e.setViewSize(1080, 1920);
        e.tick(0);
        e.seek(100);
        float px, py;
        toView(0, 500, &px, &py);
        e.touch(TouchAction::Down, px, py);
        e.touch(TouchAction::Up, px, py);
        const LayerId bar = e.selected();
        CHECK(bar == idByName(e.project(), "Batang"));
        e.selectProp(0);
        e.keyHere(true);  // kunci posisi X dan Y di frame 100
        const Layer* L = e.document().find(bar);
        CHECK(L->tracks.size() == 2);
        CHECK(e.document().undoDepth() == 1);  // kedua properti dalam SATU langkah undo
        std::vector<float> g;
        e.graphData(&g);
        CHECK(g.size() == 4 + 7 && g[0] == 1 && g[1] == 100);
        e.seek(150);
        e.touch(TouchAction::Down, px, py);
        e.touch(TouchAction::Move, px + 40 * s, py);
        e.touch(TouchAction::Up, px + 40 * s, py);
        L = e.document().find(bar);
        CHECK(L->tracks[0].keys.size() == 2);  // keyframe baru di frame 150 karena X beranimasi
        CHECK(near(evalProp(*L, Prop::PosX, 150), 40, 0.05));
        CHECK(near(evalProp(*L, Prop::PosX, 125), 20, 0.05));
        CHECK(near(evalProp(*L, Prop::PosX, 100), 0, 0.05));
        float v[4] = {75.f, 0, 0, 0};
        e.keyEdit(0, 150, 160, v);  // pindah frame 150 -> 160 dan nilai -> 75
        L = e.document().find(bar);
        CHECK(L->tracks[0].keys[1].frame == 160 && L->tracks[0].keys[1].value == 75.f);
        e.keyEdit(3, 100, 2, v);  // Bezier
        CHECK(e.document().find(bar)->tracks[0].keys[0].interp == Interp::Bezier);
        e.keyEdit(2, 160, 0, v);  // hapus
        CHECK(e.document().find(bar)->tracks[0].keys.size() == 1);
        CHECK(validate(e.project()));
    }

    {   // Paket A: efek, bentuk, blend, gambar, state UI
        App e;
        e.open(path + "_pa");
        e.setViewSize(1080, 1920);
        const LayerId bar = idByName(e.project(), "Batang");
        CHECK(e.layerState().empty());
        e.selectLayer(bar);
        CHECK(e.selected() == bar);
        e.addEffect("invert");
        e.addEffect("gaussian_blur");
        e.addEffect("tidak_ada");
        CHECK(e.document().find(bar)->effects.size() == 2);
        e.moveEffect(0, 1);
        CHECK(e.document().find(bar)->effects[0].type == "gaussian_blur");
        e.setEffectEnabled(0, false);
        CHECK(!e.document().find(bar)->effects[0].enabled);
        e.seek(50);
        e.setParam(static_cast<int>(effectChannel(0, 0)), 12.f);   // tanpa keyframe = nilai dasar
        CHECK(e.document().find(bar)->effects[0].params[0] == 12.f);
        e.selectProp(static_cast<int>(effectChannel(0, 0)));
        e.keyHere(false);
        CHECK(e.document().find(bar)->effects[0].tracks.size() == 1);
        e.seek(80);
        e.setParam(static_cast<int>(effectChannel(0, 0)), 30.f);   // sudah beranimasi -> keyframe baru
        CHECK(e.document().find(bar)->effects[0].tracks[0].keys.size() == 2);
        std::vector<float> g;
        e.graphData(&g);
        CHECK(g[0] == 2);
        const std::string st = e.layerState();
        CHECK(st.rfind("L|", 0) == 0 && st.find("\nT|") != std::string::npos);
        CHECK(st.find("F|0|gaussian_blur|0|") != std::string::npos);
        CHECK(st.find("F|1|invert|1|") != std::string::npos);
        e.setShape(4, 0.f, 7, 0.4f);
        CHECK(e.document().find(bar)->shape == ShapeKind::Star && e.document().find(bar)->sides == 7);
        e.setShape(99, 0.f, 7, 0.4f);  // diabaikan
        e.setBlend(2);
        CHECK(e.document().find(bar)->blend == BlendMode::Multiply);
        e.setBlend(99);
        e.setColor(0x11223344u);
        CHECK(e.document().find(bar)->color == 0x11223344u);
        e.trimSelected(40, 120);
        CHECK(e.document().find(bar)->inFrame == 40 && e.document().find(bar)->outFrame == 120);
        e.shiftSelected(10);
        CHECK(e.document().find(bar)->inFrame == 50);
        e.moveSelected(0);
        CHECK(e.project().layers[0].id == bar);
        e.addImage("img_1.bin", 400, 200);
        CHECK(e.document().find(e.selected())->type == LayerType::Image);
        CHECK(near(e.document().find(e.selected())->width, 864.0, 0.01));
        e.addImage("../x", 10, 10);  // nama tidak valid ditolak
        e.selectLayer(bar);
        e.removeEffect(0);
        CHECK(e.document().find(bar)->effects.size() == 1);
        const std::string tl = e.timelineData();
        CHECK(tl.find("Batang|") != std::string::npos);
        CHECK(validate(e.project()));
        {   // gestur = satu langkah undo
            const size_t u0 = e.document().undoDepth();
            e.gesture(true);
            e.setColor(0x01010101u);
            e.setColor(0x02020202u);
            e.setColor(0x03030303u);
            e.gesture(false);
            CHECK(e.document().undoDepth() == u0 + 1);
            CHECK(e.document().find(bar)->color == 0x03030303u);
            e.command(Cmd::Undo);
            CHECK(e.document().find(bar)->color == 0x11223344u);
            e.command(Cmd::Redo);
            CHECK(e.document().find(bar)->color == 0x03030303u);
            e.gesture(true);   // gestur tanpa perubahan tidak menambah undo
            e.gesture(false);
            CHECK(e.document().undoDepth() == u0 + 1);
            e.shiftSelected(-100000);  // dijepit, tidak gagal
            CHECK(e.document().find(bar)->inFrame >= 0);
        }
        {   // belah di tengah & duplikat
            e.selectLayer(bar);
            const Layer* L = e.document().find(bar);
            const int64_t mid = (L->inFrame + L->outFrame) / 2;
            const size_t n0 = e.project().layers.size();
            e.splitAt(L->inFrame);          // di tepi: ditolak
            CHECK(e.project().layers.size() == n0);
            e.splitAt(mid);
            CHECK(e.project().layers.size() == n0 + 1);
            e.command(Cmd::Duplicate);
            CHECK(e.project().layers.size() == n0 + 2);
            CHECK(e.document().find(e.selected())->name.find("salinan") != std::string::npos);
            CHECK(e.timelineData().find("|0|") != std::string::npos);
            CHECK(validate(e.project()));
        }
        e.selectLayer(9999);
        CHECK(e.selected() == 0);
        e.addEffect("invert");  // tanpa seleksi: diabaikan
    }

    // ---- Keyframe di timelineData + geser keyframe (op 6) ----
    {
        App k;
        k.open(dir + "/app_kf.studio");
        k.setViewSize(1080, 1920);
        const LayerId bid = idByName(k.project(), "Kotak B");
        k.selectLayer(bid);
        k.selectProp(static_cast<int>(Prop::Rotation));
        const std::string td = k.timelineData();
        // baris Kotak B: field ke-9 = semua keyframe, ke-10 = kanal terpilih; demo punya key rotasi di 60 dan 240?
        std::vector<std::string> lines;
        size_t s0 = 0;
        for (size_t i = 0; i <= td.size(); ++i)
            if (i == td.size() || td[i] == '\n') { if (i > s0) lines.push_back(td.substr(s0, i - s0)); s0 = i + 1; }
        std::string line;
        for (auto& l : lines) if (l.find("Kotak B") != std::string::npos) line = l;
        CHECK(!line.empty());
        size_t bars = 0;
        for (char ch : line) bars += ch == '|';
        CHECK(bars == 10);  // 11 field (terakhir: tersembunyi)
        const Layer* lb = k.document().find(bid);
        const Track* tr = findTrack(*lb, Prop::Rotation);
        CHECK(tr && tr->keys.size() >= 2);
        const int64_t f0 = tr->keys[0].frame;
        // geser key pertama +5 frame lewat op 6
        const float v6[4] = {0, 0, 0, 0};
        k.keyEdit(6, f0, f0 + 5, v6);
        const Track* tr2 = findTrack(*k.document().find(bid), Prop::Rotation);
        CHECK(tr2->keys[0].frame == f0 + 5);
        // tabrakan ditolak
        k.keyEdit(6, f0 + 5, tr2->keys[1].frame, v6);
        CHECK(findTrack(*k.document().find(bid), Prop::Rotation)->keys[0].frame == f0 + 5);
    }

    // ---- Salin / tempel / tempel gaya + zoom kanvas ----
    {
        App q;
        q.open(dir + "/app_clip.studio");
        q.setViewSize(1080, 1920);
        const LayerId ka = idByName(q.project(), "Kotak A");
        const LayerId kb = idByName(q.project(), "Kotak B");
        q.pasteLayer();
        CHECK(!q.hasClip());                      // belum ada yang disalin
        q.selectLayer(ka);
        q.copySelected();
        CHECK(q.hasClip());
        const size_t n0 = q.project().layers.size();
        q.seek(50);
        q.pasteLayer();
        CHECK(q.project().layers.size() == n0 + 1);
        const Layer* pl = q.document().find(q.selected());
        CHECK(pl && pl->inFrame == 50 && pl->outFrame - pl->inFrame == q.document().find(ka)->outFrame - q.document().find(ka)->inFrame);
        CHECK(pl->parent == kRoot);
        // tempel gaya: warna dari A ke B
        const uint32_t colA = q.document().find(ka)->color;
        q.selectLayer(kb);
        q.pasteStyle(1);
        CHECK(q.document().find(kb)->color == colA);
        q.pasteStyle(4);   // transform
        CHECK(near(q.document().find(kb)->transform.posX, q.document().find(ka)->transform.posX, 0.01));
        q.pasteStyle(0);   // mask kosong: tidak apa-apa
        CHECK(validate(q.project()));
        // kecepatan layer: 2x => durasi separuh, keyframe ikut diskalakan, tetap valid
        {
            q.selectLayer(ka);
            const Layer* la = q.document().find(ka);
            const int64_t in0 = la->inFrame, len0 = la->outFrame - la->inFrame;
            q.scaleSelected(2.0);
            la = q.document().find(ka);
            CHECK(la->inFrame == in0 && std::llabs((la->outFrame - la->inFrame) - len0 / 2) <= 1);
            CHECK(validate(q.project()));
            q.scaleSelected(0.5);
            CHECK(validate(q.project()));
            q.scaleSelected(100.0);   // di luar batas: ditolak, tidak mengubah apa pun
            CHECK(validate(q.project()));
            int idx = -1, cnt = 0;
            CHECK(q.layerOrder(&idx, &cnt) && idx >= 0 && idx < cnt);
        }
        // zoom
        CHECK(near(q.viewZoom(), 1.0, 1e-6));
        q.zoomAt(2.f, 540.f, 960.f);
        CHECK(near(q.viewZoom(), 2.0, 1e-6) && near(q.viewPanX(), 0.0, 1e-3) && near(q.viewPanY(), 0.0, 1e-3));
        q.zoomAt(2.f, 100.f, 100.f);   // fokus bukan di tengah: pan berubah supaya titik fokus tetap
        CHECK(q.viewPanX() > 0);
        q.zoomAt(1000.f, 540.f, 960.f);
        CHECK(near(q.viewZoom(), 6.0, 1e-6));
        q.resetView();
        CHECK(near(q.viewZoom(), 1.0, 1e-6) && q.viewPanX() == 0.f);
        q.toggleGrid();
        CHECK(q.showGrid());
    }

    // ---- Gizmo putar & skala ----
    {
        App g;
        g.open(dir + "/app_gizmo.studio");
        g.setViewSize(1080, 1920);
        g.seek(100);
        const float gs = fitScale(1080, 1920, 1080, 1920);
        auto view = [&](float cx, float cy, float* ox, float* oy) { *ox = cx * gs + 540.f; *oy = cy * gs + 960.f; };
        float pv[12];
        CHECK(!g.gizmoPoints(pv));  // belum ada yang terpilih
        float x0, y0;
        view(0, 500, &x0, &y0);
        g.touch(TouchAction::Down, x0, y0);
        g.touch(TouchAction::Up, x0, y0);
        const LayerId bid = g.selected();
        CHECK(bid != 0 && g.gizmoPoints(pv));
        const float before = g.document().find(bid)->transform.rotation;
        const size_t undo0 = g.document().undoDepth();
        // putar: pegang pegangan putar, seret ke posisi 90° searah jarum jam
        float hx, hy;
        view(pv[10], pv[11], &hx, &hy);
        const float dx = pv[10] - pv[0], dy = pv[11] - pv[1];
        float tx, ty;
        view(pv[0] - dy, pv[1] + dx, &tx, &ty);  // putar vektor (dx,dy) 90° searah jarum jam (Y ke bawah)
        g.touch(TouchAction::Down, hx, hy);
        g.touch(TouchAction::Move, tx, ty);
        CHECK(g.document().find(bid)->transform.rotation == before);  // belum commit
        g.touch(TouchAction::Up, tx, ty);
        CHECK(near(g.document().find(bid)->transform.rotation - before, 90.0, 0.5));
        CHECK(g.document().undoDepth() == undo0 + 1);
        CHECK(g.selected() == bid);
        // skala: seret sudut menjauh dari pusat 2x
        CHECK(g.gizmoPoints(pv));
        const float sc0 = g.document().find(bid)->transform.scaleX;
        float cx2, cy2, ex, ey;
        view(pv[2], pv[3], &cx2, &cy2);
        view(pv[0] + (pv[2] - pv[0]) * 2.f, pv[1] + (pv[3] - pv[1]) * 2.f, &ex, &ey);
        g.touch(TouchAction::Down, cx2, cy2);
        g.touch(TouchAction::Move, ex, ey);
        g.touch(TouchAction::Up, ex, ey);
        CHECK(near(g.document().find(bid)->transform.scaleX / sc0, 2.0, 0.01));
        CHECK(near(g.document().find(bid)->transform.scaleY / g.document().find(bid)->transform.scaleX, 1.0, 0.01));
    }


    // Kurva waktu (kecepatan) lewat App: dua keyframe = satu langkah undo
    {
        App g;
        g.open(std::string(argc > 1 ? argv[1] : ".") + "/speed_app.studio");
        const LayerId bid = idByName(g.project(), "Batang");
        g.selectLayer(bid);
        const size_t u0 = g.document().undoDepth();
        g.speedCurve();
        CHECK(g.document().undoDepth() == u0 + 1);
        const Layer* l = g.document().find(bid);
        CHECK(findTrack(*l, Prop::Time) != nullptr && findTrack(*l, Prop::Time)->keys.size() == 2);
        const double mid = double(l->inFrame + (l->outFrame - l->inFrame) / 2);
        CHECK(near(layerTime(*l, mid), mid, 1e-3));   // awal: 1x
        g.speedCurve();                               // kedua kali tidak menambah apa pun
        CHECK(g.document().undoDepth() == u0 + 1);
        CHECK(validate(g.project()));
    }

    // Grup & induk
    {
        App g;
        g.open(std::string(argc > 1 ? argv[1] : ".") + "/group_app.studio");
        const LayerId bid = idByName(g.project(), "Batang");
        g.selectLayer(bid);
        const size_t u0 = g.document().undoDepth();
        const size_t n0 = g.project().layers.size();
        g.groupSelected();
        CHECK(g.project().layers.size() == n0 + 1);
        CHECK(g.document().undoDepth() == u0 + 1);               // bungkus = satu langkah undo
        const LayerId gid = g.document().find(bid)->parent;
        CHECK(gid != 0 && g.document().find(gid)->type == LayerType::Group);
        CHECK(validate(g.project()));
        g.setParentOfSelected(0);                                // yang terpilih sekarang = grup; tanpa induk, tetap sah
        g.selectLayer(bid);
        g.setParentOfSelected(0);
        CHECK(g.document().find(bid)->parent == 0);
        g.setParentOfSelected(gid);
        CHECK(g.document().find(bid)->parent == gid);
        g.selectLayer(gid);
        g.setParentOfSelected(bid);                              // induk bukan Grup: ditolak
        CHECK(g.document().find(gid)->parent == 0);
        g.setParentOfSelected(gid);                              // dirinya sendiri: ditolak
        CHECK(g.document().find(gid)->parent == 0);
        g.addGroup();
        // grup tidak boleh dijadikan anak keturunannya sendiri
        const std::string pd = g.parentData();
        CHECK(!pd.empty());
        CHECK(validate(g.project()));
    }

    // Preset animasi lewat App
    {
        App g;
        g.open(std::string(argc > 1 ? argv[1] : ".") + "/preset_app.studio");
        const LayerId bid = idByName(g.project(), "Batang");
        const std::string cat = App::presetCatalog();
        CHECK(!cat.empty() && cat.find("fade_in|0|") == 0);
        g.selectLayer(0);
        const size_t u0 = g.document().undoDepth();
        g.applyPreset(0, 0.5);                                   // tanpa pilihan: tidak ada perubahan
        CHECK(g.document().undoDepth() == u0);
        g.selectLayer(bid);
        g.applyPreset(-1, 0.5);
        g.applyPreset(100000, 0.5);
        CHECK(g.document().undoDepth() == u0);
        g.applyPreset(0, 0.5);
        CHECK(g.document().undoDepth() == u0 + 1);               // satu langkah undo
        CHECK(validate(g.project()));
        g.applyPreset(3, 0.0);                                   // durasi tidak valid: dijepit, tetap sah
        g.applyPreset(15, 99.0);
        CHECK(validate(g.project()));
        g.command(Cmd::Undo);
        CHECK(validate(g.project()));
    }

    // Pilih banyak, masker, dan induk bebas
    {
        App g;
        g.open(std::string(argc > 1 ? argv[1] : ".") + "/multi_app.studio");
        const size_t n0 = g.project().layers.size();
        CHECK(g.multiCount() == 0 && !g.multiMode());
        g.selectLayer(g.project().layers[0].id);
        g.setMultiMode(true);
        CHECK(g.multiMode() && g.multiCount() == 1);
        g.toggleMulti(g.project().layers[1].id);
        g.toggleMulti(g.project().layers[2].id);
        CHECK(g.multiCount() == 3);
        g.toggleMulti(g.project().layers[2].id);
        CHECK(g.multiCount() == 2);
        const std::string tl = g.timelineData();
        CHECK(tl.find("|2|") != std::string::npos);              // baris yang ditandai
        const size_t u0 = g.document().undoDepth();
        g.groupMulti();
        CHECK(g.project().layers.size() == n0 + 1);
        CHECK(g.document().undoDepth() == u0 + 1 && !g.multiMode());
        CHECK(validate(g.project()));
        g.ungroupSelected();
        CHECK(g.project().layers.size() == n0 && validate(g.project()));
        // induk bebas: bentuk boleh jadi induk
        const LayerId a = g.project().layers[0].id, b = g.project().layers[1].id;
        g.selectLayer(a);
        g.setParentOfSelected(b);
        CHECK(g.document().find(a)->parent == b && validate(g.project()));
        g.selectLayer(b);
        g.setParentOfSelected(a);                                // melingkar: ditolak
        CHECK(g.document().find(b)->parent == 0);
        // masker
        g.selectLayer(g.project().layers[0].id);
        g.setMatte(1);                                           // paling bawah: tidak ada layer di bawahnya
        CHECK(g.matteMode() == 0);
        g.selectLayer(g.project().layers[3].id);
        g.setMatte(1);
        CHECK(g.matteMode() == 1 && g.matteInfo().rfind("1|", 0) == 0);
        g.setMatte(0);
        CHECK(g.matteMode() == 0);
        // duplikat masker tidak ikut menjadi masker
        g.setMatte(2);
        g.command(Cmd::Duplicate);
        CHECK(g.matteMode() == 0);
        // hapus banyak
        g.setMultiMode(true);
        g.toggleMulti(g.project().layers[0].id);
        const size_t n1 = g.project().layers.size();
        g.deleteMulti();
        CHECK(g.project().layers.size() < n1 && validate(g.project()));
    }

    // Preset tersimpan (salin permanen gaya layer)
    {
        App g;
        g.open(std::string(argc > 1 ? argv[1] : ".") + "/saved_preset_app.studio");
        const LayerId src = g.project().layers[3].id, dst = g.project().layers[4].id;
        g.selectLayer(src);
        g.addEffect("gaussian_blur");
        g.setBlend(2);
        const std::string txt = g.presetExport();
        CHECK(!txt.empty() && txt.rfind("STUDIOPROJECT", 0) == 0);
        g.selectLayer(dst);
        const size_t u0 = g.document().undoDepth();
        g.presetApply(txt, 2 | 8);                                // blend + efek
        CHECK(g.document().undoDepth() == u0 + 1);
        CHECK(g.document().find(dst)->effects.size() == 1 && g.document().find(dst)->blend == g.document().find(src)->blend);
        g.presetApply("bukan preset", 15);                        // rusak: tidak mengubah apa pun
        CHECK(g.document().undoDepth() == u0 + 1);
        g.selectLayer(0);
        CHECK(g.presetExport().empty());
        CHECK(validate(g.project()));
    }

    std::printf("app: %d pemeriksaan, %d gagal\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
