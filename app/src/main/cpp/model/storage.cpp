#include "model/storage.h"

#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace studio {

namespace {

constexpr size_t kMaxFileBytes = 64u * 1024u * 1024u;
constexpr size_t kMaxLayers = 1000000;
constexpr int kBackups = 3;

uint32_t crc32(const char* data, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        c ^= static_cast<uint8_t>(data[i]);
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

bool isPlain(unsigned char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
           ch == '_' || ch == '.';
}

std::string encodeName(const std::string& s) {
    if (s.empty()) return "~";
    static const char* hex = "0123456789ABCDEF";
    std::string o;
    for (unsigned char ch : s) {
        if (isPlain(ch)) {
            o += static_cast<char>(ch);
        } else {
            o += '%';
            o += hex[ch >> 4];
            o += hex[ch & 15];
        }
    }
    return o;
}

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool decodeName(const std::string& s, std::string* out) {
    out->clear();
    if (s == "~") return true;
    if (s.empty()) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(s[i]);
        if (ch == '%') {
            if (i + 2 >= s.size()) return false;
            int hi = hexVal(s[i + 1]), lo = hexVal(s[i + 2]);
            if (hi < 0 || lo < 0) return false;
            *out += static_cast<char>((hi << 4) | lo);
            i += 2;
        } else if (isPlain(ch)) {
            *out += static_cast<char>(ch);
        } else {
            return false;
        }
    }
    return true;
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

std::vector<std::string> splitTokens(const std::string& line) {
    std::vector<std::string> t;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && line[i] == ' ') ++i;
        size_t j = i;
        while (j < line.size() && line[j] != ' ') ++j;
        if (j > i) t.push_back(line.substr(i, j - i));
        i = j;
    }
    return t;
}

bool parseI64(const std::string& s, int64_t* v) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    long long x = std::strtoll(s.c_str(), &end, 10);
    if (errno != 0 || *end != '\0') return false;
    *v = x;
    return true;
}

bool parseU32(const std::string& s, uint32_t* v) {
    if (s.empty() || s[0] == '-') return false;
    char* end = nullptr;
    errno = 0;
    unsigned long long x = std::strtoull(s.c_str(), &end, 10);
    if (errno != 0 || *end != '\0' || x > 0xFFFFFFFFull) return false;
    *v = static_cast<uint32_t>(x);
    return true;
}

bool parseI32(const std::string& s, int32_t* v) {
    int64_t x;
    if (!parseI64(s, &x) || x < INT32_MIN || x > INT32_MAX) return false;
    *v = static_cast<int32_t>(x);
    return true;
}

bool parseF(const std::string& s, float* v) {
    if (s.empty()) return false;
    char* end = nullptr;
    float x = std::strtof(s.c_str(), &end);  // errno diabaikan: nilai denormal boleh
    if (*end != '\0') return false;
    *v = x;
    return true;
}

std::string backupPath(const std::string& path, int i) {
    return path + ".bak" + std::to_string(i);
}

bool fileExists(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

}  // namespace

namespace {
void writeTracks(std::string& out, const std::vector<Track>& tracks) {
    char buf[256];
    for (const Track& tr : tracks) {
        std::snprintf(buf, sizeof buf, "track %d %zu\n", static_cast<int>(tr.prop), tr.keys.size());
        out += buf;
        for (const Keyframe& k : tr.keys) {
            std::snprintf(buf, sizeof buf, "key %lld %.9g %d %.9g %.9g %.9g %.9g\n",
                          static_cast<long long>(k.frame), k.value, static_cast<int>(k.interp), k.outX,
                          k.outY, k.inX, k.inY);
            out += buf;
        }
    }
}

// Baca `n` track mulai dari lines[pos]; maxProp = batas atas nomor properti (eksklusif).
bool readTracks(const std::vector<std::string>& lines, size_t* pos, int64_t n, int maxProp,
                std::vector<Track>* out, std::string* why) {
    for (int64_t ti = 0; ti < n; ++ti) {
        if (*pos >= lines.size()) { *why = "track kurang"; return false; }
        auto th = splitTokens(lines[(*pos)++]);
        int32_t prop;
        int64_t nKeys;
        if (th.size() != 3 || th[0] != "track" || !parseI32(th[1], &prop) ||
            !parseI64(th[2], &nKeys) || prop < 0 || prop >= maxProp || nKeys < 1 ||
            static_cast<uint64_t>(nKeys) > lines.size()) {
            *why = "baris track rusak";
            return false;
        }
        Track tr;
        tr.prop = static_cast<Prop>(prop);
        tr.keys.reserve(static_cast<size_t>(nKeys));
        for (int64_t ki = 0; ki < nKeys; ++ki) {
            if (*pos >= lines.size()) { *why = "keyframe kurang"; return false; }
            auto tk = splitTokens(lines[(*pos)++]);
            Keyframe k;
            int32_t interp;
            if (tk.size() != 8 || tk[0] != "key" || !parseI64(tk[1], &k.frame) ||
                !parseF(tk[2], &k.value) || !parseI32(tk[3], &interp) || !parseF(tk[4], &k.outX) ||
                !parseF(tk[5], &k.outY) || !parseF(tk[6], &k.inX) || !parseF(tk[7], &k.inY) ||
                interp < 0 || interp > 2) {
                *why = "baris keyframe rusak";
                return false;
            }
            k.interp = static_cast<Interp>(interp);
            tr.keys.push_back(k);
        }
        out->push_back(std::move(tr));
    }
    return true;
}
}  // namespace

std::string serialize(const Project& p) {
    std::string out = "STUDIOPROJECT 7\n";
    char buf[768];
    std::snprintf(buf, sizeof buf, "canvas %d %d %d %lld %u\n", p.width, p.height, p.fps,
                  static_cast<long long>(p.durationFrames), p.nextId);
    out += buf;
    std::snprintf(buf, sizeof buf, "layers %zu\n", p.layers.size());
    out += buf;
    for (const Layer& l : p.layers) {
        const Transform& t = l.transform;
        std::snprintf(buf, sizeof buf,
                      "layer %u %d %u %lld %lld %u %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g "
                      "%.9g %.9g %.9g ",
                      l.id, static_cast<int>(l.type), l.parent, static_cast<long long>(l.inFrame),
                      static_cast<long long>(l.outFrame), l.color, t.posX, t.posY, t.posZ,
                      t.scaleX, t.scaleY, t.rotation, t.opacity, t.anchorX, t.anchorY, t.anchorZ,
                      l.width, l.height);
        out += buf;
        out += encodeName(l.name);
        out += ' ';
        out += std::to_string(l.tracks.size());
        out += '\n';
        std::snprintf(buf, sizeof buf, "style %d %.9g %d %.9g %d ", static_cast<int>(l.shape),
                      l.cornerRadius, l.sides, l.innerRatio, static_cast<int>(l.blend));
        out += buf;
        out += encodeName(l.image);
        out += '\n';
        if (l.type == LayerType::Video || l.type == LayerType::Audio) {
            std::snprintf(buf, sizeof buf, "video %lld %lld %.9g\n", static_cast<long long>(l.srcStart),
                          static_cast<long long>(l.srcCount), static_cast<double>(l.rate));
            out += buf;
        }
        if (l.type == LayerType::Text) {
            std::snprintf(buf, sizeof buf, "text %.9g %d ", static_cast<double>(l.fontSize), static_cast<int>(l.textStyle));
            out += buf;
            out += encodeName(l.text);
            out += '\n';
        }
        if (l.matte) out += "matte " + std::to_string(static_cast<int>(l.matte)) + "\n";
        if (l.hidden) out += "hidden 1\n";
        writeTracks(out, l.tracks);
        out += "effects " + std::to_string(l.effects.size()) + "\n";
        for (const Effect& e : l.effects) {
            out += "fx " + encodeName(e.type) + " " + (e.enabled ? "1" : "0") + " " +
                   std::to_string(e.params.size());
            for (float v : e.params) {
                std::snprintf(buf, sizeof buf, " %.9g", v);
                out += buf;
            }
            out += " " + std::to_string(e.tracks.size()) + "\n";
            writeTracks(out, e.tracks);
        }
    }
    std::snprintf(buf, sizeof buf, "crc %08x\n", crc32(out.data(), out.size()));
    out += buf;
    return out;
}

bool deserialize(const std::string& text, Project* out, std::string* err) {
    auto fail = [&](const std::string& m) {
        if (err) *err = m;
        return false;
    };
    if (text.size() > kMaxFileBytes) return fail("file terlalu besar");
    if (text.size() < 20 || text.back() != '\n') return fail("file terpotong");

    // Baris terakhir: "crc XXXXXXXX"; checksum meliputi semua yang ada sebelumnya.
    size_t prevNl = text.rfind('\n', text.size() - 2);
    size_t crcStart = (prevNl == std::string::npos) ? 0 : prevNl + 1;
    std::string crcLine = text.substr(crcStart, text.size() - crcStart - 1);
    auto ct = splitTokens(crcLine);
    if (ct.size() != 2 || ct[0] != "crc" || ct[1].size() != 8) return fail("baris crc tidak ada");
    char* end = nullptr;
    unsigned long want = std::strtoul(ct[1].c_str(), &end, 16);
    if (*end != '\0') return fail("crc tidak terbaca");
    if (crc32(text.data(), crcStart) != static_cast<uint32_t>(want)) return fail("checksum tidak cocok");

    std::vector<std::string> lines = splitLines(text.substr(0, crcStart));
    if (lines.size() < 3) return fail("header tidak lengkap");
    int version = 0;
    if (lines[0] == "STUDIOPROJECT 2") version = 2;       // tanpa animasi (file lama tetap bisa dibuka)
    else if (lines[0] == "STUDIOPROJECT 3") version = 3;
    else if (lines[0] == "STUDIOPROJECT 4") version = 4;
    else if (lines[0] == "STUDIOPROJECT 5") version = 5;
    else if (lines[0] == "STUDIOPROJECT 6") version = 6;
    else if (lines[0] == "STUDIOPROJECT 7") version = 7;
    else return fail("versi/format tidak dikenal");

    Project p;
    {
        auto t = splitTokens(lines[1]);
        if (t.size() != 6 || t[0] != "canvas") return fail("baris canvas rusak");
        if (!parseI32(t[1], &p.width) || !parseI32(t[2], &p.height) || !parseI32(t[3], &p.fps) ||
            !parseI64(t[4], &p.durationFrames) || !parseU32(t[5], &p.nextId))
            return fail("nilai canvas rusak");
    }
    size_t count = 0;
    {
        auto t = splitTokens(lines[2]);
        int64_t n;
        if (t.size() != 2 || t[0] != "layers" || !parseI64(t[1], &n) || n < 0 ||
            static_cast<uint64_t>(n) > kMaxLayers)
            return fail("baris layers rusak");
        count = static_cast<size_t>(n);
    }
    if (version == 2 && lines.size() != 3 + count) return fail("jumlah layer tidak cocok");

    p.layers.reserve(count);
    size_t pos = 3;
    for (size_t i = 0; i < count; ++i) {
        if (pos >= lines.size()) return fail("layer kurang");
        auto t = splitTokens(lines[pos++]);
        const size_t want = version == 2 ? 20 : 21;
        if (t.size() != want || t[0] != "layer") return fail("baris layer rusak");
        Layer l;
        int32_t type = 0;
        Transform& tr = l.transform;
        if (!parseU32(t[1], &l.id) || !parseI32(t[2], &type) || !parseU32(t[3], &l.parent) ||
            !parseI64(t[4], &l.inFrame) || !parseI64(t[5], &l.outFrame) ||
            !parseU32(t[6], &l.color) || !parseF(t[7], &tr.posX) || !parseF(t[8], &tr.posY) ||
            !parseF(t[9], &tr.posZ) || !parseF(t[10], &tr.scaleX) || !parseF(t[11], &tr.scaleY) ||
            !parseF(t[12], &tr.rotation) || !parseF(t[13], &tr.opacity) ||
            !parseF(t[14], &tr.anchorX) || !parseF(t[15], &tr.anchorY) ||
            !parseF(t[16], &tr.anchorZ) || !parseF(t[17], &l.width) ||
            !parseF(t[18], &l.height) || !decodeName(t[19], &l.name))
            return fail("nilai layer rusak");
        if (type < 0 || type > static_cast<int>(LayerType::Text)) return fail("tipe layer tidak dikenal");
        l.type = static_cast<LayerType>(type);

        if (version >= 3) {
            int64_t nTracks;
            if (!parseI64(t[20], &nTracks) || nTracks < 0 || nTracks > kPropCount)
                return fail("jumlah track rusak");
            if (version >= 4) {
                if (pos >= lines.size()) return fail("baris style kurang");
                auto ts = splitTokens(lines[pos++]);
                int32_t shape, blend;
                if (ts.size() != 7 || ts[0] != "style" || !parseI32(ts[1], &shape) ||
                    !parseF(ts[2], &l.cornerRadius) || !parseI32(ts[3], &l.sides) ||
                    !parseF(ts[4], &l.innerRatio) || !parseI32(ts[5], &blend) ||
                    !decodeName(ts[6], &l.image) || shape < 0 || shape >= static_cast<int>(ShapeKind::Count) ||
                    blend < 0 || blend >= static_cast<int>(BlendMode::Count))
                    return fail("baris style rusak");
                l.shape = static_cast<ShapeKind>(shape);
                l.blend = static_cast<BlendMode>(blend);
            }
            if (version >= 5 && (l.type == LayerType::Video || l.type == LayerType::Audio)) {
                if (pos >= lines.size()) return fail("baris video kurang");
                auto tv = splitTokens(lines[pos++]);
                if (tv.size() != 4 || tv[0] != "video" || !parseI64(tv[1], &l.srcStart) ||
                    !parseI64(tv[2], &l.srcCount) || !parseF(tv[3], &l.rate))
                    return fail("baris video rusak");
            }
            if (version >= 6 && l.type == LayerType::Text) {
                if (pos >= lines.size()) return fail("baris teks kurang");
                auto tt = splitTokens(lines[pos++]);
                int32_t st;
                if (tt.size() != 4 || tt[0] != "text" || !parseF(tt[1], &l.fontSize) || !parseI32(tt[2], &st) ||
                    st < 0 || st > 15 || !decodeName(tt[3], &l.text))
                    return fail("baris teks rusak");
                l.textStyle = static_cast<uint8_t>(st);
            }
            if (version >= 7 && pos < lines.size() && lines[pos].compare(0, 6, "matte ") == 0) {
                auto tm = splitTokens(lines[pos++]);
                int32_t mv;
                if (tm.size() != 2 || !parseI32(tm[1], &mv) || mv < 1 || mv > 2) return fail("baris matte rusak");
                l.matte = static_cast<uint8_t>(mv);
            }
            if (version >= 7 && pos < lines.size() && lines[pos] == "hidden 1") {
                ++pos;
                l.hidden = true;
            }
            std::string why;
            if (!readTracks(lines, &pos, nTracks, kPropCount, &l.tracks, &why)) return fail(why);
            if (version >= 4) {
                if (pos >= lines.size()) return fail("baris effects kurang");
                auto te = splitTokens(lines[pos++]);
                int64_t nFx;
                if (te.size() != 2 || te[0] != "effects" || !parseI64(te[1], &nFx) || nFx < 0 ||
                    nFx > kMaxEffects)
                    return fail("baris effects rusak");
                for (int64_t fi = 0; fi < nFx; ++fi) {
                    if (pos >= lines.size()) return fail("efek kurang");
                    auto tf = splitTokens(lines[pos++]);
                    Effect e;
                    int64_t nParams = 0, nFxTracks = 0;
                    if (tf.size() < 5 || tf[0] != "fx" || !decodeName(tf[1], &e.type) ||
                        (tf[2] != "0" && tf[2] != "1") || !parseI64(tf[3], &nParams) || nParams < 0 ||
                        nParams > kMaxEffectParams || tf.size() != 5 + static_cast<size_t>(nParams) ||
                        !parseI64(tf[4 + static_cast<size_t>(nParams)], &nFxTracks) || nFxTracks < 0 ||
                        nFxTracks > kMaxEffectParams)
                        return fail("baris efek rusak");
                    e.enabled = tf[2] == "1";
                    e.params.resize(static_cast<size_t>(nParams));
                    for (int64_t pi = 0; pi < nParams; ++pi)
                        if (!parseF(tf[4 + static_cast<size_t>(pi)], &e.params[static_cast<size_t>(pi)]))
                            return fail("parameter efek rusak");
                    if (!readTracks(lines, &pos, nFxTracks, kMaxEffectParams, &e.tracks, &why)) return fail(why);
                    l.effects.push_back(std::move(e));
                }
            }
        }
        p.layers.push_back(std::move(l));
    }
    if (pos != lines.size()) return fail("ada baris berlebih");

    std::string verr;
    if (!validate(p, &verr)) return fail("proyek tidak konsisten: " + verr);
    *out = std::move(p);
    return true;
}

bool readWholeFile(const std::string& path, std::string* out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out->clear();
    char buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
        out->append(buf, n);
        if (out->size() > kMaxFileBytes) {
            std::fclose(f);
            return false;
        }
    }
    bool ok = !std::ferror(f);
    std::fclose(f);
    return ok;
}

bool writeWholeFileDurable(const std::string& path, const std::string& data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = (std::fflush(f) == 0) && ok;
    ok = (::fsync(::fileno(f)) == 0) && ok;
    ok = (std::fclose(f) == 0) && ok;
    return ok;
}

bool saveProject(const std::string& path, const Project& p, std::string* err) {
    auto fail = [&](const std::string& m) {
        if (err) *err = m;
        return false;
    };
    std::string verr;
    if (!validate(p, &verr)) return fail("menolak menyimpan proyek tidak valid: " + verr);

    const std::string text = serialize(p);
    const std::string tmp = path + ".tmp";

    if (!writeWholeFileDurable(tmp, text)) {
        std::remove(tmp.c_str());
        return fail("gagal menulis file sementara");
    }
    std::string readBack;
    if (!readWholeFile(tmp, &readBack) || readBack != text) {
        std::remove(tmp.c_str());
        return fail("verifikasi baca-ulang gagal");
    }

    if (fileExists(path)) {
        std::string cur;
        Project tmpProject;
        const bool mainHealthy = readWholeFile(path, &cur) && deserialize(cur, &tmpProject);
        if (mainHealthy) {
            // Geser cadangan: bak2->bak3, bak1->bak2, utama->bak1.
            for (int i = kBackups - 1; i >= 1; --i)
                std::rename(backupPath(path, i).c_str(), backupPath(path, i + 1).c_str());
            std::rename(path.c_str(), backupPath(path, 1).c_str());
        } else {
            // File utama rusak: karantina, JANGAN gilir ke cadangan (cadangan sehat tetap utuh).
            std::rename(path.c_str(), (path + ".corrupt").c_str());
        }
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) return fail("rename gagal");
    return true;
}

LoadResult loadProject(const std::string& path) {
    LoadResult r;
    bool anyExisted = false;
    for (int i = 0; i <= kBackups; ++i) {
        const std::string f = (i == 0) ? path : backupPath(path, i);
        std::string text;
        if (!readWholeFile(f, &text)) continue;
        anyExisted = true;
        Project p;
        std::string e;
        if (deserialize(text, &p, &e)) {
            r.project = std::move(p);
            r.status = (i == 0) ? LoadStatus::Ok : LoadStatus::OkFromBackup;
            r.backupIndex = i;
            return r;
        }
        r.detail += f + ": " + e + "; ";
    }
    r.status = anyExisted ? LoadStatus::Corrupt : LoadStatus::NotFound;
    return r;
}

}  // namespace studio
