#pragma once
// Katalog efek (data-driven): tiap efek = potongan shader GLSL + daftar parameter.
// Murni C++ (tanpa OpenGL) supaya konsistensinya bisa diuji otomatis; engine render yang
// mengompilasi shader-nya. Antarmuka pengaturan efek dibuat OTOMATIS dari daftar parameter ini,
// jadi menambah efek baru cukup menambah satu entri di effects.cpp.

#include <string>
#include <vector>

namespace studio {

enum class ParamKind { Float, Int, Angle, Bool, Point, Color };

struct ParamDef {
    const char* id;
    const char* label;
    ParamKind kind;
    int comps;          // jumlah float: Float/Int/Angle/Bool=1, Point=2, Color=4 (rgba)
    float def[4];
    float min, max;     // batas slider (berlaku untuk tiap komponen)
};

struct EffectDef {
    const char* type;      // id stabil yang disimpan di file proyek
    const char* label;     // nama tampilan
    const char* category;
    int passes;            // jumlah lintasan render (uPass = 0..passes-1)
    const char* body;      // GLSL: mendefinisikan `vec4 effect(vec2 uv)`
    std::vector<ParamDef> params;

    int flatCount() const;                 // total float di model (jumlah comps)
    int offsetOf(int paramIndex) const;    // indeks float pertama parameter ke-n
};

const std::vector<EffectDef>& effectCatalog();
const EffectDef* findEffectDef(const std::string& type);
std::vector<float> defaultParams(const EffectDef& d);

// Kode lengkap fragment shader sebuah efek (header umum + body + main).
std::string effectShaderSource(const EffectDef& d);
// Vertex shader layar penuh untuk lintasan efek.
const char* effectVertexSource();

// Deskripsi katalog untuk UI (teks, satu baris per entri):
//   E|type|label|category|passes
//   P|indeksFlat|label|min|max|default|kind   (kind: 0 float,1 int,2 angle,3 bool)
std::string describeCatalog();

}  // namespace studio
