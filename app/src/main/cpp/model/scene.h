#pragma once
// Matematika adegan 2D: transformasi, hierarki, visibilitas, hit-test.
// Murni C++ (tanpa OpenGL) supaya bisa diuji otomatis di komputer.
//
// Koordinat kanvas: titik (0,0) = TENGAH kanvas, sumbu X ke kanan, sumbu Y ke BAWAH (satuan piksel).
// Rotasi dalam derajat, positif = searah jarum jam di layar.
// Matriks lokal layer: L(p) = pos + Rotasi * Skala * (p - anchor)
// Posisi sebuah layer dinyatakan dalam ruang koordinat induknya (Group), atau kanvas jika di root.
// Bentuk Shape/Null berpusat di titik asal lokal (x dan y dari -lebar/2 .. +lebar/2).

#include <vector>

#include "model/model.h"

namespace studio {

struct Affine {
    float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
    // x' = a*x + c*y + tx ; y' = b*x + d*y + ty
    void apply(float x, float y, float* ox, float* oy) const {
        *ox = a * x + c * y + tx;
        *oy = b * x + d * y + ty;
    }
};

Affine operator*(const Affine& A, const Affine& B);  // (A*B)(p) = A(B(p))
Affine affineFromTransform(const Transform& t);
bool invert(const Affine& m, Affine* out);           // false jika tidak bisa dibalik

struct Resolved {
    bool visible = false;   // dalam rentang waktu, termasuk semua induknya
    float opacity = 1;      // sudah dikalikan dengan opacity semua induknya
    Affine world;           // lokal -> kanvas
};

// Hitung kondisi tiap layer pada frame tertentu. `out` sejajar indeksnya dengan project.layers.
// Transform tiap layer = nilai animasi pada frame itu (evalTransform).
// Jika overrideT != nullptr, layer overrideId memakai transformasi itu (untuk pratinjau seret).
void resolveScene(const Project& p, double frame, LayerId overrideId, const Transform* overrideT,
                  std::vector<Resolved>* out);

// Shape paling atas yang terlihat dan mengenai titik (x,y) di koordinat kanvas. 0 jika tidak ada.
LayerId hitTest(const Project& p, const std::vector<Resolved>& scene, float x, float y);

// Matriks dunia induk layer (identitas jika layer berada di root).
Affine parentWorldOf(const Project& p, const std::vector<Resolved>& scene, LayerId id);

// Ubah selisih (dx,dy) di koordinat kanvas menjadi selisih di ruang induk.
bool canvasDeltaToParent(const Affine& parentWorld, float dx, float dy, float* ox, float* oy);

}  // namespace studio
