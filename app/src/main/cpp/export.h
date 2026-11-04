// Konversi bingkai hasil render (RGBA) ke NV12 (YUV 4:2:0) untuk encoder video. Murni CPU, bisa diuji tanpa GPU.
#pragma once
#include <cstdint>
#include <vector>

namespace studio {

// rgba: w*h*4 (baris pertama = atas). w dan h harus genap. Keluaran: w*h bidang Y lalu w*h/2 bidang UV
// berselang-seling (U,V,U,V…). BT.709, rentang terbatas (16–235). Alpha dianggap sudah dikalikan ke RGB (premultiplied)
// sehingga hasilnya sama dengan menempel di atas latar hitam.
bool rgbaToNV12(const uint8_t* rgba, int w, int h, std::vector<uint8_t>* out);

}  // namespace studio
