// Tes otomatis model data + penyimpanan. Dijalankan di komputer (bukan di HP):
//   tests/run_tests.sh <folder-sementara>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>

#include "model/model.h"
#include "model/storage.h"

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

static std::string g_dir = ".";

static void test_basic_and_validation() {
    Document d;
    LayerId a = d.addLayer(LayerType::Shape, "a", 0, 100);
    CHECK(a == 1);
    CHECK(d.addLayer(LayerType::Shape, "bad", 50, 50) == 0);   // rentang kosong
    CHECK(d.addLayer(LayerType::Shape, "bad", 60, 10) == 0);   // rentang terbalik
    CHECK(d.addLayer(LayerType::Shape, "bad", -5, 10) == 0);   // negatif
    CHECK(d.addLayer(LayerType::Shape, "bad", 0, 10, 999) == 0);  // induk tidak ada
    CHECK(d.project().layers.size() == 1);

    Transform t;
    t.opacity = 1.5f;
    CHECK(!d.setTransform(a, t));
    t.opacity = 0.5f;
    t.posX = NAN;
    CHECK(!d.setTransform(a, t));
    t.posX = 3;
    CHECK(d.setTransform(a, t));
    CHECK(d.find(a)->transform == t);
    CHECK(!d.setTransform(12345, t));
}

static void test_split() {
    Document d;
    LayerId a = d.addLayer(LayerType::Shape, "a", 0, 100);
    Transform t;
    t.posX = 12.5f;
    t.posZ = 7;
    t.rotation = 33;
    t.opacity = 0.5f;
    CHECK(d.setTransform(a, t));
    CHECK(d.setColor(a, 0x11223344u));
    LayerId b = d.addLayer(LayerType::Shape, "b", 0, 50);
    LayerId r = d.splitLayer(a, 40);
    CHECK(r != 0);
    const Layer* L = d.find(a);
    const Layer* R = d.find(r);
    CHECK(L && R);
    CHECK(L->inFrame == 0 && L->outFrame == 40);
    CHECK(R->inFrame == 40 && R->outFrame == 100);
    CHECK(L->transform == t && R->transform == t);
    CHECK(L->color == 0x11223344u && R->color == 0x11223344u);
    CHECK(d.project().layers.size() == 3);
    CHECK(d.project().layers[0].id == a);
    CHECK(d.project().layers[1].id == r);  // potongan kanan tepat di atas aslinya
    CHECK(d.project().layers[2].id == b);
    CHECK(d.splitLayer(a, 40) == 0);   // di tepi
    CHECK(d.splitLayer(a, 0) == 0);
    CHECK(d.splitLayer(r, 100) == 0);
    CHECK(d.splitLayer(999, 5) == 0);

    LayerId g = d.addLayer(LayerType::Group, "g", 0, 100);
    CHECK(d.splitLayer(g, 50) == 0);   // Group tidak bisa dipotong
}

static void test_hierarchy_and_move() {
    Document d;
    LayerId g = d.addLayer(LayerType::Group, "g", 0, 100);
    LayerId c1 = d.addLayer(LayerType::Shape, "c1", 0, 100, g);
    LayerId c2 = d.addLayer(LayerType::Shape, "c2", 0, 100, g);
    LayerId g2 = d.addLayer(LayerType::Group, "g2", 0, 100, g);
    LayerId c3 = d.addLayer(LayerType::Shape, "c3", 0, 100, g2);
    CHECK(g && c1 && c2 && g2 && c3);

    CHECK(!d.setParent(g, g));    // diri sendiri
    CHECK(!d.setParent(g, g2));   // lingkaran
    CHECK(!d.setParent(g, c3));   // lingkaran (c3 keturunan g)
    CHECK(d.setParent(c2, kRoot));
    CHECK(d.find(c2)->parent == kRoot);

    // urutan Z
    size_t n = d.project().layers.size();
    CHECK(d.moveLayer(c1, n - 1));
    CHECK(d.project().layers.back().id == c1);
    CHECK(d.moveLayer(c1, 0));
    CHECK(d.project().layers.front().id == c1);
    CHECK(!d.moveLayer(c1, n));        // di luar jangkauan
    CHECK(d.moveLayer(c1, 0));         // no-op sah
    CHECK(d.project().layers.size() == n);

    // hapus Group membawa seluruh keturunannya (c1, g2, c3), c2 sudah keluar
    CHECK(d.removeLayer(g));
    CHECK(d.find(g) == nullptr && d.find(c1) == nullptr && d.find(g2) == nullptr &&
          d.find(c3) == nullptr);
    CHECK(d.find(c2) != nullptr);
    CHECK(d.project().layers.size() == 1);
    CHECK(!d.removeLayer(g));
}

static void test_shift_trim() {
    Document d;
    LayerId a = d.addLayer(LayerType::Shape, "a", 10, 20);
    CHECK(d.shiftLayer(a, 5));
    CHECK(d.find(a)->inFrame == 15 && d.find(a)->outFrame == 25);
    CHECK(!d.shiftLayer(a, -16));  // jadi negatif
    CHECK(d.find(a)->inFrame == 15);
    CHECK(!d.shiftLayer(a, INT64_C(1) << 50));
    CHECK(d.trimLayer(a, 16, 30));
    CHECK(!d.trimLayer(a, 30, 16));
}

static void test_undo_redo() {
    Document d;
    CHECK(!d.canUndo() && !d.canRedo());
    LayerId a = d.addLayer(LayerType::Shape, "a", 0, 10);
    LayerId b = d.addLayer(LayerType::Shape, "b", 0, 10);
    (void)a;
    (void)b;
    CHECK(d.project().layers.size() == 2);
    CHECK(d.undo());
    CHECK(d.project().layers.size() == 1);
    CHECK(d.undo());
    CHECK(d.project().layers.empty());
    CHECK(!d.undo());
    CHECK(d.redo());
    CHECK(d.redo());
    CHECK(d.project().layers.size() == 2);
    CHECK(!d.redo());
    CHECK(d.undo());
    CHECK(d.canRedo());
    d.addLayer(LayerType::Shape, "c", 0, 10);  // edit baru membuang redo
    CHECK(!d.canRedo());

    // batas undo
    Document small(Project{}, 3);
    for (int i = 0; i < 10; ++i) small.addLayer(LayerType::Shape, "x", 0, 10);
    CHECK(small.undoDepth() == 3);
}

static void test_serialize() {
    Project p;
    p.width = 1920;
    p.height = 1080;
    p.fps = 60;
    p.durationFrames = 1234567890123LL;
    Document d(p);
    const char* names[] = {"hello world", "100% legal \xE2\x9C\x93", "", "~", "a%b", "line1\nline2",
                           "tab\there", "\xF0\x9F\x8E\xAC clip", "a b  c"};
    for (const char* n : names) {
        LayerId id = d.addLayer(LayerType::Shape, n, 0, 10);
        CHECK(id != 0);
    }
    Transform t;
    t.posX = 0.1f;
    t.posY = 1e-7f;
    t.posZ = 123456.789f;
    t.scaleX = -0.0f;
    t.scaleY = 3.4e38f;
    t.rotation = 359.99997f;
    t.opacity = 0.333333343f;
    t.anchorX = 1e-40f;  // denormal
    t.anchorY = -42.5f;
    t.anchorZ = 7;
    CHECK(d.setTransform(1, t));

    std::string text = serialize(d.project());
    Project q;
    std::string err;
    CHECK(deserialize(text, &q, &err));
    CHECK(q == d.project());
    CHECK(serialize(q) == text);  // deterministik

    // kerusakan harus terdeteksi
    for (size_t pos : {size_t(0), size_t(10), text.size() / 2, text.size() - 12}) {
        std::string bad = text;
        bad[pos] = static_cast<char>(bad[pos] ^ 0x01);
        Project z;
        CHECK(!deserialize(bad, &z, nullptr));
    }
    Project z;
    CHECK(!deserialize(text.substr(0, text.size() / 2), &z, nullptr));
    CHECK(!deserialize(text.substr(0, text.size() - 1), &z, nullptr));
    CHECK(!deserialize("", &z, nullptr));
    CHECK(!deserialize("sampah", &z, nullptr));
}

static std::string P(const char* name) { return g_dir + "/" + name; }

static void wipe(const std::string& path) {
    for (const char* ext : {"", ".tmp", ".bak1", ".bak2", ".bak3", ".corrupt"})
        std::remove((path + ext).c_str());
}

static size_t layersIn(const std::string& file) {
    std::string text;
    Project p;
    if (!readWholeFile(file, &text) || !deserialize(text, &p, nullptr)) return size_t(-1);
    return p.layers.size();
}

static void test_storage() {
    const std::string path = P("proj.studio");
    wipe(path);
    CHECK(loadProject(path).status == LoadStatus::NotFound);

    Document d;
    std::string err;
    for (int v = 1; v <= 4; ++v) {
        CHECK(d.addLayer(LayerType::Shape, "L", 0, 10) != 0);
        CHECK(saveProject(path, d.project(), &err));
    }
    // utama = 4 layer, bak1 = 3, bak2 = 2, bak3 = 1
    CHECK(layersIn(path) == 4);
    CHECK(layersIn(path + ".bak1") == 3);
    CHECK(layersIn(path + ".bak2") == 2);
    CHECK(layersIn(path + ".bak3") == 1);

    LoadResult r = loadProject(path);
    CHECK(r.status == LoadStatus::Ok && r.project.layers.size() == 4 && r.project == d.project());

    // file utama rusak -> pakai bak1
    CHECK(writeWholeFileDurable(path, "sampah"));
    r = loadProject(path);
    CHECK(r.status == LoadStatus::OkFromBackup && r.backupIndex == 1 && r.project.layers.size() == 3);

    // simpan lagi saat utama rusak: cadangan sehat TIDAK boleh tergeser keluar
    CHECK(d.addLayer(LayerType::Shape, "L", 0, 10) != 0);  // 5 layer
    CHECK(saveProject(path, d.project(), &err));
    CHECK(layersIn(path) == 5);
    CHECK(layersIn(path + ".bak1") == 3);
    CHECK(layersIn(path + ".bak2") == 2);
    CHECK(layersIn(path + ".bak3") == 1);

    // utama + bak1 rusak -> bak2
    CHECK(writeWholeFileDurable(path, "x"));
    CHECK(writeWholeFileDurable(path + ".bak1", "y"));
    r = loadProject(path);
    CHECK(r.status == LoadStatus::OkFromBackup && r.backupIndex == 2 && r.project.layers.size() == 2);

    // simulasi crash di tengah menyimpan: file utama hilang, cadangan ada
    std::remove(path.c_str());
    r = loadProject(path);
    CHECK(r.status == LoadStatus::OkFromBackup);

    // semua rusak
    CHECK(writeWholeFileDurable(path, "x"));
    CHECK(writeWholeFileDurable(path + ".bak2", "x"));
    CHECK(writeWholeFileDurable(path + ".bak3", "x"));
    r = loadProject(path);
    CHECK(r.status == LoadStatus::Corrupt);

    // menolak menyimpan proyek tidak valid dan tidak menyentuh file
    wipe(path);
    Project good;
    Document dg(good);
    dg.addLayer(LayerType::Shape, "ok", 0, 10);
    CHECK(saveProject(path, dg.project(), &err));
    Project broken = dg.project();
    broken.layers[0].outFrame = broken.layers[0].inFrame;  // tidak valid
    CHECK(!saveProject(path, broken, &err));
    CHECK(layersIn(path) == 1);
    wipe(path);
}

static void test_fuzz() {
    std::mt19937 rng(20261011);
    Document doc(Project{}, 1000000);
    int accepted = 0, rejected = 0;

    for (int step = 0; step < 20000; ++step) {
        const Project before = doc.project();
        const size_t depth0 = doc.undoDepth();
        const uint64_t rev0 = doc.revision();
        const int op = static_cast<int>(rng() % 12);
        bool ok = false;

        auto pickId = [&]() -> LayerId {
            const auto& ls = doc.project().layers;
            if (ls.empty() || rng() % 8 == 0) return static_cast<LayerId>(rng() % 60);  // kadang ngawur
            return ls[rng() % ls.size()].id;
        };
        auto pickGroupOrRoot = [&]() -> LayerId {
            std::vector<LayerId> gs;
            for (const Layer& l : doc.project().layers)
                if (l.type == LayerType::Group) gs.push_back(l.id);
            if (gs.empty() || rng() % 3 == 0) return rng() % 6 == 0 ? static_cast<LayerId>(rng() % 60) : kRoot;
            return gs[rng() % gs.size()];
        };

        switch (op) {
            case 0:
            case 1: {
                LayerType ty = static_cast<LayerType>(rng() % 3);
                int64_t in = static_cast<int64_t>(rng() % 200) - 5;
                int64_t out = in + static_cast<int64_t>(rng() % 100) - 10;
                ok = doc.addLayer(ty, "n" + std::to_string(step), in, out, pickGroupOrRoot()) != 0;
                break;
            }
            case 2: ok = doc.removeLayer(pickId()); break;
            case 3: ok = doc.moveLayer(pickId(), rng() % (doc.project().layers.size() + 2)); break;
            case 4: ok = doc.setParent(pickId(), pickGroupOrRoot()); break;
            case 5: {
                Transform t;
                t.posX = static_cast<float>(rng() % 2000) - 1000;
                t.rotation = static_cast<float>(rng() % 720);
                t.opacity = (rng() % 12 == 0) ? 2.0f : static_cast<float>(rng() % 101) / 100.f;
                if (rng() % 15 == 0) t.scaleX = NAN;
                ok = doc.setTransform(pickId(), t);
                break;
            }
            case 6: ok = doc.shiftLayer(pickId(), static_cast<int64_t>(rng() % 120) - 60); break;
            case 7: {
                int64_t in = static_cast<int64_t>(rng() % 150);
                ok = doc.trimLayer(pickId(), in, in + static_cast<int64_t>(rng() % 80) - 10);
                break;
            }
            case 8:
            case 9: ok = doc.splitLayer(pickId(), static_cast<int64_t>(rng() % 220)) != 0; break;
            case 10: {
                bool did = (rng() % 2) ? doc.undo() : doc.redo();
                (void)did;
                break;
            }
            case 11: {
                float w = (rng() % 10 == 0) ? -5.f : static_cast<float>(rng() % 500 + 1);
                ok = doc.setSize(pickId(), w, static_cast<float>(rng() % 500 + 1));
                break;
            }
        }

        std::string err;
        const bool healthy = validate(doc.project(), &err);
        if (!healthy) std::fprintf(stderr, "langkah %d op %d tidak sehat: %s\n", step, op, err.c_str());
        CHECK(healthy);

        if (op != 10) {
            if (ok) ++accepted; else ++rejected;
            if (!ok) {
                CHECK(doc.project() == before);  // penolakan tidak boleh mengubah apa pun
                CHECK(doc.revision() == rev0);
            }

            // Properti undo: jika operasi menambah satu entri undo, undo->redo harus presisi.
            if (ok && doc.undoDepth() == depth0 + 1 && step % 5 == 0) {
                const Project after = doc.project();
                CHECK(doc.undo());
                CHECK(doc.project() == before);
                CHECK(doc.redo());
                CHECK(doc.project() == after);
            }
        }

        if (step % 997 == 0) {  // bolak-balik serialisasi pada keadaan acak
            Project q;
            const std::string text = serialize(doc.project());
            CHECK(deserialize(text, &q, nullptr));
            CHECK(q == doc.project());
        }
    }
    std::printf("fuzz: %d diterima, %d ditolak, %zu layer di akhir\n", accepted, rejected,
                doc.project().layers.size());
    CHECK(accepted > 3000 && rejected > 1000);  // pastikan kedua jalur benar-benar teruji
}

static void test_proto_size_revision() {
    Document d;
    CHECK(d.revision() == 0);
    Layer proto;
    proto.name = "proto";
    proto.inFrame = 5;
    proto.outFrame = 50;
    proto.width = 321;
    proto.height = 123;
    proto.color = 0xAABBCCDDu;
    proto.transform.posX = -77;
    proto.id = 9999;  // harus diabaikan
    LayerId id = d.addLayer(proto);
    CHECK(id == 1);
    const Layer* l = d.find(id);
    CHECK(l && l->width == 321 && l->height == 123 && l->color == 0xAABBCCDDu);
    CHECK(l->transform.posX == -77 && l->name == "proto");
    CHECK(d.undoDepth() == 1);  // satu operasi = satu entri undo
    CHECK(d.revision() == 1);

    CHECK(!d.setSize(id, 0, 10));
    CHECK(!d.setSize(id, 10, -1));
    CHECK(!d.setSize(id, NAN, 10));
    CHECK(!d.setSize(id, 1e9f, 10));
    CHECK(d.revision() == 1);
    CHECK(d.setSize(id, 50, 60));
    CHECK(d.revision() == 2);
    CHECK(d.setSize(id, 50, 60));  // tanpa perubahan nyata
    CHECK(d.revision() == 2 && d.undoDepth() == 2);
    CHECK(d.undo() && d.revision() == 3);
    CHECK(d.redo() && d.revision() == 4);

    // ukuran ikut tersimpan
    Project q;
    CHECK(deserialize(serialize(d.project()), &q, nullptr));
    CHECK(q.layers[0].width == 50 && q.layers[0].height == 60);
}

int main(int argc, char** argv) {
    if (argc > 1) g_dir = argv[1];
    test_proto_size_revision();
    test_basic_and_validation();
    test_split();
    test_hierarchy_and_move();
    test_shift_trim();
    test_undo_redo();
    test_serialize();
    test_storage();
    test_fuzz();
    std::printf("%d pemeriksaan, %d gagal\n", g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
