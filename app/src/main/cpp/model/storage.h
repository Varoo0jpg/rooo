#pragma once
// Penyimpanan proyek yang dirancang agar tidak bisa merusak pekerjaan pengguna.
//
// - Format teks berversi + checksum CRC32: file rusak/terpotong pasti terdeteksi.
// - Simpan ke file sementara, tulis ulang dari disk untuk verifikasi, lalu rename atomik.
// - 3 cadangan bergilir (.bak1 terbaru). File utama yang rusak TIDAK menimpa cadangan sehat.
// - Saat membuka: coba file utama, lalu .bak1, .bak2, .bak3; yang pertama sehat dipakai.

#include <string>

#include "model/model.h"

namespace studio {

std::string serialize(const Project& p);
bool deserialize(const std::string& text, Project* out, std::string* err = nullptr);

bool saveProject(const std::string& path, const Project& p, std::string* err = nullptr);

enum class LoadStatus { Ok, OkFromBackup, NotFound, Corrupt };

struct LoadResult {
    LoadStatus status = LoadStatus::NotFound;
    Project project;
    int backupIndex = 0;  // 0 = file utama, 1..3 = cadangan yang dipakai
    std::string detail;   // alasan kegagalan file yang dilewati
};

LoadResult loadProject(const std::string& path);

// Utilitas file (dipakai juga oleh tes).
bool readWholeFile(const std::string& path, std::string* out);
bool writeWholeFileDurable(const std::string& path, const std::string& data);

}  // namespace studio
