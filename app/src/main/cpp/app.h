#pragma once
// Logika aplikasi (tanpa OpenGL/Android): proyek aktif, waktu putar, pilihan, seret, autosave.
// Dipisah dari engine render supaya bisa diuji otomatis di komputer.

#include <cstdint>
#include <string>
#include <vector>

#include "model/model.h"
#include "model/scene.h"

namespace studio {

// Pemetaan kanvas <-> layar: kanvas diskalakan agar muat di layar dan berada di tengah.
inline float fitScale(int canvasW, int canvasH, int viewW, int viewH) {
    const float sx = static_cast<float>(viewW) / static_cast<float>(canvasW);
    const float sy = static_cast<float>(viewH) / static_cast<float>(canvasH);
    return (sx < sy ? sx : sy) * 0.94f;
}

enum class Cmd : int { AddShape = 0, Split = 1, Delete = 2, Undo = 3, Redo = 4, Duplicate = 5 };
enum class TouchAction : int { Down = 0, Move = 1, Up = 2, Cancel = 3 };

class App {
public:
    // Membuka proyek dari path (dengan cadangan). Jika tidak ada / rusak total -> proyek demo.
    void open(const std::string& path);
    void setViewSize(int w, int h);

    // Dipanggil tiap frame dengan jam monoton (ms): memajukan putaran dan menjalankan autosave.
    void tick(int64_t nowMs);

    const Project& project() const { return doc_.project(); }
    int64_t frame() const { return frame_; }
    bool playing() const { return playing_; }
    LayerId selected() const { return selected_; }
    int viewW() const { return viewW_; }
    int viewH() const { return viewH_; }

    // Keadaan adegan pada frame sekarang (termasuk pratinjau seret).
    void sceneNow(std::vector<Resolved>* out) const;

    void togglePlay();
    void seek(int64_t frame);
    void touch(TouchAction action, float x, float y);

    // Salin / tempel (gaya Alight Motion). Clipboard hidup selama proses aplikasi.
    bool hasClip() const { return hasClip_; }
    std::string clipName() const { return clip_.name; }
    void copySelected();
    void pasteLayer();
    void pasteStyle(uint32_t mask);
    void speedCurve();                                 // siapkan kurva waktu (kecepatan) layer terpilih dan pilih kanalnya
    void addGroup();
    void groupSelected();                                // bungkus layer terpilih dalam Grup baru
    void setParentOfSelected(LayerId parent);            // 0 = lepas dari induk
    std::string parentData() const;
    // Preset animasi pada layer terpilih. seconds = lama jendela masuk/keluar atau setengah siklus loop.
    void applyPreset(int preset, double seconds);
    // Track matte layer terpilih (0 mati, 1 alfa layer di bawahnya, 2 kebalikan). matteMode() -1 bila tidak ada layer terpilih.
    // Pilih banyak: aktifkan mode, ketuk layer untuk menandai, lalu kelompokkan/hapus/gandakan sekaligus.
    void setMultiMode(bool on);
    bool multiMode() const { return multiMode_; }
    void toggleMulti(LayerId id);
    bool isMulti(LayerId id) const;
    int multiCount() const { return static_cast<int>(multi_.size()); }
    void groupMulti();
    void deleteMulti();
    void duplicateMulti();
    void ungroupSelected();
    void renameSelected(const std::string& name);
    void setHidden(LayerId id, bool hidden);
    void resetChannel(int ch, float value);              // hapus semua keyframe kanal lalu set nilai dasar
    std::string presetExport() const;                    // layer terpilih sebagai teks (kosong bila tidak bisa)
    void presetApply(const std::string& text, uint32_t mask);
    void setMatte(int mode);
    int matteMode() const;
    std::string matteInfo() const;
    static std::string presetCatalog();                  // baris "id|kategori|label" (kategori 0 masuk, 1 keluar, 2 loop)
    void addText(const std::string& image, int pw, int ph, const std::string& text, float fontSize, int style);
    void setText(const std::string& image, int pw, int ph, const std::string& text, float fontSize, int style);   // layer Teks terpilih
    std::string textData() const;                        // "ukuran|gaya|teks" (teks: \\n = baris baru, \\\\ = garis miring) bila layer Teks terpilih
    void addAudio(const std::string& clip, int64_t frames, int64_t inFrame, const std::string& name);   // klip suara sudah ada di folder media
    std::string audioData() const;                       // satu baris per layer suara: id|klip|in|out|srcStart|rate|volume
    float audioVolume(LayerId id, int64_t frame) const;
    void addVideo(const std::string& clip, int pw, int ph, int64_t frames);   // proxy video sudah ada di folder media
    void scaleSelected(double factor);                 // ubah kecepatan layer terpilih (durasi + keyframe)
    bool layerOrder(int* index, int* count) const;      // posisi layer terpilih (0 = paling bawah)

    // Tampilan kanvas: zoom (1 = pas layar) dan geser, plus garis bantu.
    float viewZoom() const { return zoom_; }
    float viewPanX() const { return panX_; }
    float viewPanY() const { return panY_; }
    bool showGrid() const { return grid_; }
    void zoomAt(float factor, float fx, float fy);   // fokus (fx,fy) dalam piksel layar tetap diam
    void panBy(float dx, float dy);
    void resetView();
    void toggleGrid() { grid_ = !grid_; }
    // Titik gizmo layer terpilih di koordinat kanvas: [0..1]=pusat, [2..9]=4 sudut (kiri-atas, kanan-atas, kanan-bawah,
    // kiri-bawah), [10..11]=pegangan putar. false jika tidak ada layer yang bisa digizmo.
    bool gizmoPoints(float out[12]) const;
    // Skala kanvas -> piksel layar (untuk ukuran pegangan yang konstan).
    float viewScale() const;
    void command(Cmd cmd);

    // --- Animasi pada layer terpilih ---
    void selectProp(int channel);  // kanal apa pun: properti Transform atau parameter efek
    int selectedProp() const { return static_cast<int>(prop_); }
    void selectLayer(LayerId id);
    // Ubah nilai kanal di frame sekarang (auto-key: jika kanal beranimasi, jadi keyframe).
    void setParam(int channel, float value);
    void gesture(bool begin);  // kelompokkan edit beruntun (seret slider) jadi satu undo
    // Tampilan & efek layer terpilih.
    void addEffect(const std::string& type);
    void removeEffect(int index);
    void moveEffect(int index, int newIndex);
    void setEffectEnabled(int index, bool enabled);
    void setShape(int kind, float corner, int sides, float inner);
    void setBlend(int mode);
    void setColor(uint32_t rgba);
    void addImage(const std::string& name, int pxW, int pxH);
    void trimSelected(int64_t newIn, int64_t newOut);
    void shiftSelected(int64_t delta);
    void moveSelected(int newIndex);
    void splitAt(int64_t frame);  // belah layer terpilih di frame tertentu (mis. di tengah)
    // Keadaan layer terpilih untuk UI (teks):
    //   baris 1: L|id|nama|tipe|bentuk|corner|sisi|inner|blend|warna|in|out|indeksZ
    //   baris 2: T|posX|posY|skalaX|skalaY|rotasi|opasitas (nilai pada frame sekarang)
    //   tiap efek: F|idx|type|enabled|nilai1,nilai2,...|indeksParamBerkeyframe,...
    // Kosong jika tidak ada layer terpilih.
    std::string layerState() const;
    // Satu baris per layer, urut dari atas: id|nama|in|out|tipe|parent|terpilih
    std::string timelineData() const;
    // Beri keyframe pada properti terpilih (atau Posisi X+Y jika both) di frame sekarang.
    void keyHere(bool both);
    void keyMask(uint32_t mask);                       // keyframe di playhead untuk sekumpulan properti Transform (bit = nomor Prop)
    // Operasi grafik pada keyframe di `frame`: 0 = pindah/ubah nilai (newFrame, v[0]),
    // 1 = handle segmen (v[0..3] = outX,outY,inX,inY), 2 = hapus, 3 = jenis interp (newFrame),
    // 4 = preset easing (v[0..3] = x1,y1,x2,y2).
    void keyEdit(int op, int64_t frame, int64_t newFrame, const float* v);
    // Data grafik: [nKeys (-1 = tidak ada layer), playhead, nilai sekarang, durasi, lalu per keyframe:
    // frame, value, interp, outX, outY, inX, inY].
    void graphData(std::vector<float>* out) const;
    void saveNow();  // simpan segera jika ada perubahan belum tersimpan

    std::string info() const;

    int fps() const { return doc_.project().fps; }
    const Document& document() const { return doc_; }

private:
    void viewToCanvas(float vx, float vy, float* cx, float* cy) const;
    void cancelDrag() { dragging_ = false; dragId_ = 0; dragKind_ = 0; }
    void normalizeSelection();
    bool doSave();

    Document doc_;
    std::string path_;
    std::string note_;

    int viewW_ = 1, viewH_ = 1;
    int64_t nowMs_ = 0;
    int64_t frame_ = 0;
    bool playing_ = false;
    int64_t playBaseFrame_ = 0;
    int64_t playBaseMs_ = 0;

    LayerId selected_ = 0;
    bool multiMode_ = false;
    std::vector<LayerId> multi_;
    int addCount_ = 0;

    bool dragging_ = false;
    LayerId dragId_ = 0;
    int64_t dragFrame_ = 0;
    Prop prop_ = Prop::PosX;
    float dragStartX_ = 0, dragStartY_ = 0;
    Transform dragStartT_;
    Transform dragT_;
    int dragKind_ = 0;           // 0 geser, 1 putar, 2 skala (gizmo)
    float dragRef_ = 0;          // sudut awal (putar) atau jarak awal (skala)
    float dragCx_ = 0, dragCy_ = 0;  // pusat layer di kanvas saat mulai seret
    Affine dragParentWorld_;

    Layer clip_;
    bool hasClip_ = false;
    float zoom_ = 1.f, panX_ = 0.f, panY_ = 0.f;
    bool grid_ = false;

    uint64_t lastRev_ = 0;
    bool dirty_ = false;
    int64_t dirtyAtMs_ = 0;
    bool saveOk_ = true;
    std::string saveErr_;
    int saveCount_ = 0;
};

// Proyek contoh: latar, sebuah Group berisi 2 kotak, dan satu batang.
Project makeDemoProject();

}  // namespace studio
