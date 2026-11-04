// Tes matematika adegan: transformasi, hierarki, visibilitas, hit-test, seret.
#include <cmath>
#include <cstdio>
#include <random>

#include "model/model.h"
#include "model/scene.h"

using namespace studio;

static int g_fail = 0, g_checks = 0;
#define CHECK(c)                                                              \
    do {                                                                      \
        ++g_checks;                                                           \
        if (!(c)) {                                                           \
            ++g_fail;                                                         \
            std::fprintf(stderr, "GAGAL %s:%d  %s\n", __FILE__, __LINE__, #c); \
        }                                                                     \
    } while (0)

static bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

static void test_affine() {
    Transform t;
    Affine id = affineFromTransform(t);
    float x, y;
    id.apply(5, 7, &x, &y);
    CHECK(near(x, 5) && near(y, 7));

    // rotasi 90 derajat searah jarum jam di layar (Y ke bawah): (1,0) -> (0,1)
    t.rotation = 90;
    affineFromTransform(t).apply(1, 0, &x, &y);
    CHECK(near(x, 0) && near(y, 1));

    // anchor: titik anchor harus mendarat tepat di posisi
    Transform u;
    u.posX = 100;
    u.posY = 50;
    u.anchorX = 10;
    u.anchorY = -4;
    u.rotation = 37;
    u.scaleX = 2;
    u.scaleY = 0.5f;
    affineFromTransform(u).apply(10, -4, &x, &y);
    CHECK(near(x, 100) && near(y, 50));

    // skala dan urutan: skala dulu, baru rotasi, baru geser
    Transform s;
    s.scaleX = 3;
    s.posX = 10;
    s.rotation = 90;
    affineFromTransform(s).apply(1, 0, &x, &y);  // (1,0)->skala(3,0)->rot(0,3)->+(10,0)
    CHECK(near(x, 10) && near(y, 3));

    // komposisi: (A*B)(p) == A(B(p))
    Affine A = affineFromTransform(u), B = affineFromTransform(s);
    float x1, y1, x2, y2, tx, ty;
    B.apply(2, 3, &tx, &ty);
    A.apply(tx, ty, &x1, &y1);
    (A * B).apply(2, 3, &x2, &y2);
    CHECK(near(x1, x2) && near(y1, y2));

    // invers
    Affine inv;
    CHECK(invert(A, &inv));
    A.apply(8, -9, &tx, &ty);
    inv.apply(tx, ty, &x, &y);
    CHECK(near(x, 8) && near(y, -9));
    Transform zero;
    zero.scaleX = 0;
    CHECK(!invert(affineFromTransform(zero), &inv));
}

static void test_invert_random() {
    std::mt19937 rng(7);
    auto rf = [&](float lo, float hi) { return lo + (hi - lo) * (rng() % 100000) / 100000.f; };
    for (int i = 0; i < 2000; ++i) {
        Transform t;
        t.posX = rf(-500, 500);
        t.posY = rf(-500, 500);
        t.rotation = rf(-720, 720);
        t.scaleX = rf(0.2f, 4);
        t.scaleY = rf(0.2f, 4);
        t.anchorX = rf(-100, 100);
        t.anchorY = rf(-100, 100);
        Affine m = affineFromTransform(t), inv;
        CHECK(invert(m, &inv));
        float px = rf(-300, 300), py = rf(-300, 300), x, y, bx, by;
        m.apply(px, py, &x, &y);
        inv.apply(x, y, &bx, &by);
        CHECK(near(bx, px, 0.05f) && near(by, py, 0.05f));
    }
}

static void test_hierarchy_visibility() {
    Document d;
    Layer g;
    g.type = LayerType::Group;
    g.inFrame = 10;
    g.outFrame = 100;
    g.transform.posX = 100;
    g.transform.scaleX = g.transform.scaleY = 2;
    g.transform.opacity = 0.5f;
    LayerId gid = d.addLayer(g);

    Layer c;
    c.type = LayerType::Shape;
    c.parent = gid;
    c.inFrame = 0;
    c.outFrame = 200;  // lebih lebar dari induknya
    c.transform.posX = 10;
    c.transform.opacity = 0.5f;
    LayerId cid = d.addLayer(c);
    CHECK(gid && cid);

    std::vector<Resolved> sc;
    resolveScene(d.project(), 50, 0, nullptr, &sc);
    CHECK(sc.size() == 2);
    float x, y;
    sc[1].world.apply(0, 0, &x, &y);  // anak di (10,0) induk, induk skala 2 geser 100 -> 120
    CHECK(near(x, 120) && near(y, 0));
    CHECK(sc[0].visible && sc[1].visible);
    CHECK(near(sc[1].opacity, 0.25f));  // 0.5 * 0.5

    // induk di luar rentang menyembunyikan anak walau anak masih dalam rentangnya
    resolveScene(d.project(), 5, 0, nullptr, &sc);
    CHECK(!sc[0].visible && !sc[1].visible);
    resolveScene(d.project(), 100, 0, nullptr, &sc);  // out eksklusif
    CHECK(!sc[0].visible && !sc[1].visible);
    resolveScene(d.project(), 10, 0, nullptr, &sc);   // in inklusif
    CHECK(sc[0].visible && sc[1].visible);

    // override transformasi (pratinjau seret)
    Transform ov = d.find(cid)->transform;
    ov.posX = 30;
    resolveScene(d.project(), 50, cid, &ov, &sc);
    sc[1].world.apply(0, 0, &x, &y);
    CHECK(near(x, 160));  // 100 + 2*30
}

static void test_child_before_parent_in_order() {
    // anak boleh berada SEBELUM induk di urutan Z; hasil harus tetap benar
    Document d;
    Layer g;
    g.type = LayerType::Group;
    g.inFrame = 0;
    g.outFrame = 50;
    g.transform.posX = 200;
    LayerId gid = d.addLayer(g);
    Layer c;
    c.parent = gid;
    c.inFrame = 0;
    c.outFrame = 50;
    c.transform.posX = 5;
    LayerId cid = d.addLayer(c);
    CHECK(d.moveLayer(cid, 0));  // anak ke bawah, induk di atas
    CHECK(d.project().layers[0].id == cid);
    std::vector<Resolved> sc;
    resolveScene(d.project(), 1, 0, nullptr, &sc);
    float x, y;
    sc[0].world.apply(0, 0, &x, &y);
    CHECK(near(x, 205));
}

static void test_hit_test() {
    Document d;
    Layer a;
    a.width = 100;
    a.height = 100;
    a.outFrame = 100;
    a.transform.posX = 0;
    LayerId A = d.addLayer(a);
    Layer b = a;
    b.transform.posX = 50;  // menimpa sebagian A, berada di atasnya
    LayerId B = d.addLayer(b);
    std::vector<Resolved> sc;
    resolveScene(d.project(), 10, 0, nullptr, &sc);
    CHECK(hitTest(d.project(), sc, 20, 0) == B);    // tumpang tindih: yang atas menang
    CHECK(hitTest(d.project(), sc, -40, 0) == A);
    CHECK(hitTest(d.project(), sc, 200, 0) == 0);
    CHECK(hitTest(d.project(), sc, 0, 60) == 0);    // di luar tinggi

    // layer berputar 45 derajat: sudut kotak asli tidak lagi kena, titik di sumbu tetap kena
    Document d2;
    Layer r = a;
    r.transform.rotation = 45;
    LayerId R = d2.addLayer(r);
    resolveScene(d2.project(), 10, 0, nullptr, &sc);
    CHECK(hitTest(d2.project(), sc, 45, 45) == 0);   // sudut kotak asli (ke luar setelah rotasi)
    CHECK(hitTest(d2.project(), sc, 60, 0) == R);    // ujung belah ketupat
    CHECK(hitTest(d2.project(), sc, 0, 0) == R);

    // di luar rentang waktu / opacity 0 / bukan Shape: tidak kena
    resolveScene(d2.project(), 500, 0, nullptr, &sc);
    CHECK(hitTest(d2.project(), sc, 0, 0) == 0);
    Document d3;
    Layer z = a;
    z.transform.opacity = 0;
    d3.addLayer(z);
    Layer nl = a;
    nl.type = LayerType::Null;
    d3.addLayer(nl);
    resolveScene(d3.project(), 10, 0, nullptr, &sc);
    CHECK(hitTest(d3.project(), sc, 0, 0) == 0);

    // skala nol: tidak crash, tidak kena
    Document d4;
    Layer s0 = a;
    s0.transform.scaleX = 0;
    d4.addLayer(s0);
    resolveScene(d4.project(), 10, 0, nullptr, &sc);
    CHECK(hitTest(d4.project(), sc, 0, 0) == 0);
}

static void test_drag_delta() {
    // Seret layer di dalam Group yang diputar+diskalakan: layer harus mengikuti jari tepat.
    Document d;
    Layer g;
    g.type = LayerType::Group;
    g.outFrame = 100;
    g.transform.posX = 30;
    g.transform.posY = -20;
    g.transform.rotation = 33;
    g.transform.scaleX = 1.7f;
    g.transform.scaleY = 0.6f;
    LayerId gid = d.addLayer(g);
    Layer c;
    c.parent = gid;
    c.outFrame = 100;
    c.transform.posX = 12;
    c.transform.posY = 9;
    LayerId cid = d.addLayer(c);

    std::vector<Resolved> sc;
    resolveScene(d.project(), 1, 0, nullptr, &sc);
    float wx0, wy0;
    sc[1].world.apply(0, 0, &wx0, &wy0);

    const float dx = 40, dy = -25;  // jari bergeser 40,-25 di kanvas
    Affine pw = parentWorldOf(d.project(), sc, cid);
    float px, py;
    CHECK(canvasDeltaToParent(pw, dx, dy, &px, &py));
    Transform nt = d.find(cid)->transform;
    nt.posX += px;
    nt.posY += py;
    CHECK(d.setTransform(cid, nt));
    resolveScene(d.project(), 1, 0, nullptr, &sc);
    float wx1, wy1;
    sc[1].world.apply(0, 0, &wx1, &wy1);
    CHECK(near(wx1 - wx0, dx, 0.01f) && near(wy1 - wy0, dy, 0.01f));

    // layer di root: parentWorld identitas
    Document d2;
    Layer r;
    r.outFrame = 10;
    LayerId rid = d2.addLayer(r);
    resolveScene(d2.project(), 1, 0, nullptr, &sc);
    Affine id = parentWorldOf(d2.project(), sc, rid);
    CHECK(id.a == 1 && id.d == 1 && id.tx == 0 && id.b == 0 && id.c == 0);

    // induk tak bisa dibalik (skala 0): ditolak, tidak crash
    Affine bad;
    bad.a = 0;
    bad.d = 0;
    float ox, oy;
    CHECK(!canvasDeltaToParent(bad, 1, 1, &ox, &oy));
}

static void test_scene_fuzz() {
    // Pada proyek acak yang sah, resolveScene tidak boleh crash dan harus konsisten dengan
    // perhitungan rantai induk secara langsung.
    std::mt19937 rng(99);
    Document doc(Project{}, 1000);
    for (int step = 0; step < 3000; ++step) {
        std::vector<LayerId> groups;
        for (const Layer& l : doc.project().layers)
            if (l.type == LayerType::Group) groups.push_back(l.id);
        Layer l;
        l.type = static_cast<LayerType>(rng() % 3);
        l.inFrame = rng() % 50;
        l.outFrame = l.inFrame + 1 + rng() % 80;
        l.parent = (groups.empty() || rng() % 3 == 0) ? kRoot : groups[rng() % groups.size()];
        l.transform.posX = static_cast<float>(rng() % 400) - 200;
        l.transform.rotation = static_cast<float>(rng() % 360);
        l.transform.scaleX = 0.5f + static_cast<float>(rng() % 30) / 10.f;
        l.transform.opacity = static_cast<float>(rng() % 101) / 100.f;
        doc.addLayer(l);
        if (step % 7 == 0 && !doc.project().layers.empty())
            doc.moveLayer(doc.project().layers[rng() % doc.project().layers.size()].id,
                          rng() % doc.project().layers.size());
        if (step % 31 == 0) doc.splitLayer(doc.project().layers[rng() % doc.project().layers.size()].id,
                                           static_cast<int64_t>(rng() % 100));
    }
    const Project& p = doc.project();
    const int64_t frame = 25;
    std::vector<Resolved> sc;
    resolveScene(p, frame, 0, nullptr, &sc);
    CHECK(sc.size() == p.layers.size());

    // pembanding independen: jalan ke atas lewat rantai induk
    for (size_t i = 0; i < p.layers.size(); i += 5) {
        Affine m = affineFromTransform(p.layers[i].transform);
        bool vis = frame >= p.layers[i].inFrame && frame < p.layers[i].outFrame;
        float op = p.layers[i].transform.opacity;
        LayerId par = p.layers[i].parent;
        while (par != kRoot) {
            const Layer* pl = doc.find(par);
            m = affineFromTransform(pl->transform) * m;
            vis = vis && frame >= pl->inFrame && frame < pl->outFrame;
            op *= pl->transform.opacity;
            par = pl->parent;
        }
        CHECK(sc[i].visible == vis);
        CHECK(near(sc[i].opacity, op, 1e-4f));
        float x1, y1, x2, y2;
        m.apply(3, 4, &x1, &y1);
        sc[i].world.apply(3, 4, &x2, &y2);
        CHECK(near(x1, x2, 0.05f) && near(y1, y2, 0.05f));
    }
    std::printf("scene fuzz: %zu layer\n", p.layers.size());
}

int main() {
    test_affine();
    test_invert_random();
    test_hierarchy_visibility();
    test_child_before_parent_in_order();
    test_hit_test();
    test_drag_delta();
    test_scene_fuzz();
    std::printf("scene: %d pemeriksaan, %d gagal\n", g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
