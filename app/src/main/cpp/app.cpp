#include "app.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "effects.h"
#include "model/anim.h"
#include "model/storage.h"

namespace studio {

namespace {
constexpr int64_t kAutosaveDebounceMs = 1000;

const uint32_t kPalette[] = {0xFF5A5AFFu, 0x5AFF8AFFu, 0x5AA0FFFFu, 0xFFD35AFFu,
                             0xD35AFFFFu, 0x5AFFF0FFu, 0xFF9A3CFFu};
constexpr int kPaletteN = sizeof(kPalette) / sizeof(kPalette[0]);
}  // namespace

Project makeDemoProject() {
    Document d;
    Layer bg;
    bg.name = "Latar";
    bg.inFrame = 0;
    bg.outFrame = 300;
    bg.color = 0x2A3A5AFFu;
    bg.width = 1000;
    bg.height = 1700;
    d.addLayer(bg);

    Layer grp;
    grp.name = "Grup";
    grp.type = LayerType::Group;
    grp.inFrame = 0;
    grp.outFrame = 300;
    grp.transform.posY = -300;
    grp.transform.rotation = 15;
    const LayerId g = d.addLayer(grp);

    Layer a;
    a.name = "Kotak A";
    a.parent = g;
    a.inFrame = 0;
    a.outFrame = 300;
    a.color = 0xFF5A5AFFu;
    a.width = a.height = 300;
    a.transform.posX = -200;
    d.addLayer(a);

    Layer b;
    b.name = "Kotak B";
    b.parent = g;
    b.inFrame = 60;
    b.outFrame = 240;
    b.color = 0x5AFF8AFFu;
    b.width = b.height = 200;
    b.transform.posX = 200;
    const LayerId bid = d.addLayer(b);
    d.setKey(bid, Prop::Rotation, 60, 0);   // contoh animasi: berputar dengan easing ease-in-out
    d.setKey(bid, Prop::Rotation, 240, 360);
    d.setKeyEase(bid, Prop::Rotation, 60, 0.42f, 0.f, 0.58f, 1.f);

    Layer bar;
    bar.name = "Batang";
    bar.inFrame = 30;
    bar.outFrame = 200;
    bar.color = 0xFFD35AFFu;
    bar.width = 700;
    bar.height = 160;
    bar.transform.posY = 500;
    d.addLayer(bar);

    return d.project();
}

void App::open(const std::string& path) {
    path_ = path;
    LoadResult r = loadProject(path);
    bool needSave = false;
    switch (r.status) {
        case LoadStatus::Ok:
            doc_ = Document(std::move(r.project));
            note_ = "Proyek dibuka";
            break;
        case LoadStatus::OkFromBackup:
            doc_ = Document(std::move(r.project));
            note_ = "Dibuka dari cadangan " + std::to_string(r.backupIndex);
            needSave = true;
            break;
        case LoadStatus::NotFound:
            doc_ = Document(makeDemoProject());
            note_ = "Proyek baru (demo)";
            needSave = true;
            break;
        case LoadStatus::Corrupt:
        default:
            doc_ = Document(makeDemoProject());
            note_ = "Semua file rusak -> demo baru";
            needSave = true;
            break;
    }
    frame_ = 0;
    playing_ = false;
    selected_ = 0;
    cancelDrag();
    lastRev_ = doc_.revision();
    dirty_ = needSave;
    dirtyAtMs_ = nowMs_;
}

void App::setViewSize(int w, int h) {
    viewW_ = w > 0 ? w : 1;
    viewH_ = h > 0 ? h : 1;
}

void App::tick(int64_t nowMs) {
    if (nowMs < nowMs_) {  // jam mundur (mis. Activity dibuat ulang): rebase semua acuan waktu
        playBaseMs_ = nowMs;
        dirtyAtMs_ = nowMs;
    }
    nowMs_ = nowMs;
    const int64_t dur = doc_.project().durationFrames;
    if (dur > 0 && frame_ >= dur) frame_ = dur - 1;   // durasi bisa menyusut saat layer dipendekkan
    if (playing_ && dur > 0) {
        const int64_t elapsed = (nowMs - playBaseMs_) * doc_.project().fps / 1000;
        frame_ = (playBaseFrame_ + (elapsed < 0 ? 0 : elapsed)) % dur;
    }
    if (doc_.revision() != lastRev_) {
        lastRev_ = doc_.revision();
        dirty_ = true;
        dirtyAtMs_ = nowMs;
    }
    if (dirty_ && !dragging_ && nowMs - dirtyAtMs_ >= kAutosaveDebounceMs) doSave();
}

bool App::doSave() {
    std::string err;
    saveOk_ = saveProject(path_, doc_.project(), &err);
    saveErr_ = err;
    if (saveOk_) {
        dirty_ = false;
        ++saveCount_;
    } else {
        dirtyAtMs_ = nowMs_;  // coba lagi nanti
    }
    return saveOk_;
}

void App::saveNow() {
    if (doc_.revision() != lastRev_) {
        lastRev_ = doc_.revision();
        dirty_ = true;
    }
    if (dirty_) doSave();
}

void App::sceneNow(std::vector<Resolved>* out) const {
    if (dragging_ && dragId_ != 0)
        resolveScene(doc_.project(), static_cast<double>(frame_), dragId_, &dragT_, out);
    else
        resolveScene(doc_.project(), static_cast<double>(frame_), 0, nullptr, out);
}

void App::togglePlay() {
    playing_ = !playing_;
    if (playing_) {
        playBaseFrame_ = frame_;
        playBaseMs_ = nowMs_;
    }
}

void App::seek(int64_t f) {
    const int64_t dur = doc_.project().durationFrames;
    frame_ = std::clamp<int64_t>(f, 0, dur > 0 ? dur - 1 : 0);
    playBaseFrame_ = frame_;
    playBaseMs_ = nowMs_;
}

void App::viewToCanvas(float vx, float vy, float* cx, float* cy) const {
    const Project& p = doc_.project();
    const float s = fitScale(p.width, p.height, viewW_, viewH_) * zoom_;
    *cx = (vx - viewW_ * 0.5f - panX_) / s;
    *cy = (vy - viewH_ * 0.5f - panY_) / s;
}

float App::viewScale() const {
    const Project& p = doc_.project();
    return fitScale(p.width, p.height, viewW_, viewH_) * zoom_;
}

void App::zoomAt(float factor, float fx, float fy) {
    if (!(factor > 0.f) || !std::isfinite(factor)) return;
    const float nz = std::clamp(zoom_ * factor, 0.25f, 6.f);
    const float k = nz / zoom_;
    // titik kanvas di bawah fokus tetap di bawah fokus
    panX_ = (fx - viewW_ * 0.5f) - ((fx - viewW_ * 0.5f) - panX_) * k;
    panY_ = (fy - viewH_ * 0.5f) - ((fy - viewH_ * 0.5f) - panY_) * k;
    zoom_ = nz;
}

void App::panBy(float dx, float dy) {
    if (!std::isfinite(dx) || !std::isfinite(dy)) return;
    const float lim = 4.f * static_cast<float>(std::max(viewW_, viewH_)) * zoom_;
    panX_ = std::clamp(panX_ + dx, -lim, lim);
    panY_ = std::clamp(panY_ + dy, -lim, lim);
}

void App::resetView() {
    zoom_ = 1.f;
    panX_ = panY_ = 0.f;
}

void App::copySelected() {
    const Layer* l = selected_ ? doc_.find(selected_) : nullptr;
    if (!l) { note_ = "Pilih layer dulu (ketuk)"; return; }
    if (l->type == LayerType::Group) { note_ = "Grup belum bisa disalin"; return; }
    clip_ = *l;
    hasClip_ = true;
    note_ = "Disalin: " + l->name;
}

void App::pasteLayer() {
    if (!hasClip_) { note_ = "Belum ada yang disalin"; return; }
    Layer n = clip_;
    n.id = 0;
    n.parent = kRoot;
    const int64_t delta = frame_ - n.inFrame;   // mulai dari playhead, durasi & animasi ikut bergeser
    n.inFrame += delta;
    n.outFrame += delta;
    for (Track& t : n.tracks) for (Keyframe& k : t.keys) k.frame += delta;
    for (Effect& e : n.effects) for (Track& t : e.tracks) for (Keyframe& k : t.keys) k.frame += delta;
    n.transform.posX += 30;
    n.transform.posY += 30;
    const LayerId id = doc_.addLayer(n);
    if (id) { selected_ = id; note_ = "Ditempel di playhead"; }
    else note_ = "Gagal menempel: " + doc_.lastError();
}

void App::pasteStyle(uint32_t mask) {
    if (!hasClip_) { note_ = "Belum ada yang disalin"; return; }
    if (!selected_ || !doc_.find(selected_)) { note_ = "Pilih layer tujuan dulu"; return; }
    if (mask == 0) return;
    if (doc_.pasteStyle(selected_, clip_, mask)) note_ = "Gaya ditempel";
    else note_ = "Gagal menempel gaya: " + doc_.lastError();
}

void App::speedCurve() {
    cancelDrag();
    const Layer* l = selected_ ? doc_.find(selected_) : nullptr;
    if (!l) { note_ = "Pilih layer dulu"; return; }
    if (!findTrack(*l, Prop::Time)) {
        const int64_t a = l->inFrame, b = l->outFrame;
        doc_.beginBatch();
        const bool ok = doc_.setKey(selected_, Prop::Time, a, static_cast<float>(a)) &&
                        doc_.setKey(selected_, Prop::Time, b, static_cast<float>(b));
        doc_.endBatch();
        if (!ok) { note_ = "Gagal membuat kurva kecepatan: " + doc_.lastError(); return; }
    }
    prop_ = Prop::Time;
    note_ = "Kurva kecepatan: curam = cepat, landai = lambat, datar = berhenti";
}

void App::scaleSelected(double factor) {
    cancelDrag();
    if (!selected_ || !doc_.find(selected_)) { note_ = "Pilih layer dulu"; return; }
    if (doc_.scaleLayerTime(selected_, factor)) note_ = "Kecepatan layer diubah";
    else note_ = "Gagal ubah kecepatan: " + doc_.lastError();
}

bool App::layerOrder(int* index, int* count) const {
    const auto& ls = doc_.project().layers;
    *count = static_cast<int>(ls.size());
    *index = -1;
    for (size_t i = 0; i < ls.size(); ++i)
        if (ls[i].id == selected_) *index = static_cast<int>(i);
    return *index >= 0;
}

bool App::gizmoPoints(float out[12]) const {
    const Layer* l = selected_ ? doc_.find(selected_) : nullptr;
    if (!l || l->type == LayerType::Group || l->type == LayerType::Null || l->type == LayerType::Audio) return false;
    std::vector<Resolved> sc;
    sceneNow(&sc);
    const Project& p = doc_.project();
    for (size_t i = 0; i < p.layers.size() && i < sc.size(); ++i) {
        if (p.layers[i].id != selected_) continue;
        if (!sc[i].visible) return false;
        const Affine& w = sc[i].world;
        const float hw = l->width * 0.5f, hh = l->height * 0.5f;
        w.apply(0, 0, &out[0], &out[1]);
        w.apply(-hw, -hh, &out[2], &out[3]);
        w.apply(hw, -hh, &out[4], &out[5]);
        w.apply(hw, hh, &out[6], &out[7]);
        w.apply(-hw, hh, &out[8], &out[9]);
        float tx, ty;
        w.apply(0, -hh, &tx, &ty);
        float dx = tx - out[0], dy = ty - out[1];
        const float n = std::hypot(dx, dy);
        const float off = 40.f / std::max(viewScale(), 1e-3f);
        if (n < 1e-4f) { dx = 0; dy = -1; } else { dx /= n; dy /= n; }
        out[10] = tx + dx * off;
        out[11] = ty + dy * off;
        return std::isfinite(out[10]) && std::isfinite(out[11]);
    }
    return false;
}

void App::touch(TouchAction action, float x, float y) {
    float cx, cy;
    viewToCanvas(x, y, &cx, &cy);
    switch (action) {
        case TouchAction::Down: {
            cancelDrag();
            // Gizmo layer terpilih lebih dulu: pegangan putar & sudut skala.
            {
                float g[12];
                const Layer* sl = selected_ ? doc_.find(selected_) : nullptr;
                if (sl && gizmoPoints(g)) {
                    const float s = std::max(viewScale(), 1e-3f);
                    const float r = 30.f / s;  // radius sentuh 30px
                    int kind = 0;
                    if (std::hypot(cx - g[10], cy - g[11]) <= r) kind = 1;
                    else
                        for (int k = 0; k < 4 && !kind; ++k)
                            if (std::hypot(cx - g[2 + k * 2], cy - g[3 + k * 2]) <= r) kind = 2;
                    if (kind) {
                        std::vector<Resolved> sc0;
                        resolveScene(doc_.project(), static_cast<double>(frame_), 0, nullptr, &sc0);
                        playing_ = false;
                        dragFrame_ = frame_;
                        dragging_ = true;
                        dragKind_ = kind;
                        dragId_ = selected_;
                        dragCx_ = g[0];
                        dragCy_ = g[1];
                        dragStartT_ = evalTransform(*sl, static_cast<double>(frame_));
                        dragT_ = dragStartT_;
                        dragRef_ = kind == 1 ? std::atan2(cy - g[1], cx - g[0]) : std::max(std::hypot(cx - g[0], cy - g[1]), 1e-3f);
                        break;
                    }
                }
            }
            dragKind_ = 0;
            std::vector<Resolved> sc;
            resolveScene(doc_.project(), static_cast<double>(frame_), 0, nullptr, &sc);
            const LayerId hit = hitTest(doc_.project(), sc, cx, cy);
            selected_ = hit;
            if (hit != 0) {
                const Layer* l = doc_.find(hit);
                if (l) {
                    playing_ = false;  // menyeret selalu pada frame yang diam
                    dragFrame_ = frame_;
                    dragging_ = true;
                    dragId_ = hit;
                    dragStartX_ = cx;
                    dragStartY_ = cy;
                    dragStartT_ = evalTransform(*l, static_cast<double>(frame_));
                    dragT_ = dragStartT_;
                    dragParentWorld_ = parentWorldOf(doc_.project(), sc, hit);
                }
            }
            break;
        }
        case TouchAction::Move: {
            if (!dragging_) break;
            if (dragKind_ == 1) {
                float da = std::atan2(cy - dragCy_, cx - dragCx_) - dragRef_;
                da = std::atan2(std::sin(da), std::cos(da));  // -pi..pi, tanpa lompatan 360°
                dragT_ = dragStartT_;
                dragT_.rotation = dragStartT_.rotation + da * 57.29578f;
                break;
            }
            if (dragKind_ == 2) {
                const float ratio = std::clamp(std::hypot(cx - dragCx_, cy - dragCy_) / dragRef_, 0.02f, 50.f);
                dragT_ = dragStartT_;
                dragT_.scaleX = dragStartT_.scaleX * ratio;
                dragT_.scaleY = dragStartT_.scaleY * ratio;
                break;
            }
            float ox, oy;
            if (!canvasDeltaToParent(dragParentWorld_, cx - dragStartX_, cy - dragStartY_, &ox, &oy))
                break;
            dragT_ = dragStartT_;
            dragT_.posX += ox;
            dragT_.posY += oy;
            break;
        }
        case TouchAction::Up: {
            if (dragging_ && !(dragT_ == dragStartT_)) doc_.setTransformAt(dragId_, dragFrame_, dragT_);
            cancelDrag();
            break;
        }
        case TouchAction::Cancel:
        default:
            cancelDrag();
            break;
    }
}

void App::selectProp(int ch) {
    if (ch >= 0 && (ch < kPropCount || (ch >= kEffectChannelBase && ch < kEffectChannelBase + kMaxEffects * kMaxEffectParams)))
        prop_ = static_cast<Prop>(ch);
}

static std::string propName(Prop p) {
    static const char* n[] = {"Posisi X", "Posisi Y", "Posisi Z", "Skala X", "Skala Y",
                              "Rotasi", "Opasitas", "Anchor X", "Anchor Y", "Anchor Z", "Waktu"};
    if (isEffectChannel(p))
        return "Efek " + std::to_string(effectIndexOf(p) + 1) + " param " + std::to_string(effectParamOf(p));
    return n[static_cast<int>(p)];
}

void App::keyMask(uint32_t mask) {
    cancelDrag();
    mask &= (1u << kTransformProps) - 1u;
    if (!selected_ || !doc_.find(selected_)) { note_ = "Pilih layer dulu (ketuk)"; return; }
    if (mask == 0) return;
    if (doc_.keyProps(selected_, frame_, mask)) note_ = "Keyframe ditambahkan";
    else note_ = "Gagal menambah keyframe: " + doc_.lastError();
}

void App::keyHere(bool both) {
    cancelDrag();
    const Layer* l = selected_ ? doc_.find(selected_) : nullptr;
    if (!l) {
        note_ = "Pilih layer dulu (ketuk)";
        return;
    }
    if (isEffectChannel(prop_)) {
        const bool ok = doc_.setKey(selected_, prop_, frame_, evalProp(*l, prop_, static_cast<double>(frame_)));
        note_ = ok ? "Kunci: " + propName(prop_) + " @" + std::to_string(frame_) : "Gagal kunci: " + doc_.lastError();
        return;
    }
    const uint32_t mask = both ? ((1u << static_cast<int>(Prop::PosX)) | (1u << static_cast<int>(Prop::PosY)))
                               : (1u << static_cast<int>(prop_));
    const bool ok = doc_.keyProps(selected_, frame_, mask);
    note_ = ok ? std::string("Kunci: ") + (both ? std::string("Posisi") : propName(prop_)) + " @" + std::to_string(frame_)
               : "Gagal kunci: " + doc_.lastError();
}

void App::keyEdit(int op, int64_t frame, int64_t newFrame, const float* v) {
    cancelDrag();
    if (!selected_ || !doc_.find(selected_)) {
        note_ = "Pilih layer dulu (ketuk)";
        return;
    }
    bool ok = false;
    switch (op) {
        case 0: ok = doc_.editKey(selected_, prop_, frame, newFrame, v[0]); break;
        case 1: ok = doc_.setKeyHandles(selected_, prop_, frame, v[0], v[1], v[2], v[3]); break;
        case 2: ok = doc_.removeKey(selected_, prop_, frame); break;
        case 3:
            if (newFrame >= 0 && newFrame <= 2) ok = doc_.setKeyInterp(selected_, prop_, frame, static_cast<Interp>(newFrame));
            break;
        case 4: ok = doc_.setKeyEase(selected_, prop_, frame, v[0], v[1], v[2], v[3]); break;
        case 5: ok = doc_.easeKey(selected_, prop_, frame, static_cast<int>(newFrame)); break;
        case 6: ok = doc_.moveKey(selected_, prop_, frame, newFrame); break;
        default: break;
    }
    if (!ok) note_ = "Gagal: " + doc_.lastError();
}

void App::graphData(std::vector<float>* out) const {
    out->clear();
    const Layer* l = selected_ ? doc_.find(selected_) : nullptr;
    if (!l) {
        out->assign({-1.f, static_cast<float>(frame_), 0.f, static_cast<float>(doc_.project().durationFrames)});
        return;
    }
    const Track* t = findTrack(*l, prop_);
    out->push_back(t ? static_cast<float>(t->keys.size()) : 0.f);
    out->push_back(static_cast<float>(frame_));
    out->push_back(evalProp(*l, prop_, static_cast<double>(frame_)));
    out->push_back(static_cast<float>(doc_.project().durationFrames));
    if (t)
        for (const Keyframe& k : t->keys) {
            out->push_back(static_cast<float>(k.frame));
            out->push_back(k.value);
            out->push_back(static_cast<float>(static_cast<int>(k.interp)));
            out->push_back(k.outX);
            out->push_back(k.outY);
            out->push_back(k.inX);
            out->push_back(k.inY);
        }
}

void App::selectLayer(LayerId id) {
    cancelDrag();
    selected_ = (id != 0 && doc_.find(id)) ? id : 0;
}

#define NEED_SEL(l) \
    const Layer* l = selected_ ? doc_.find(selected_) : nullptr; \
    if (!l) { note_ = "Pilih layer dulu (ketuk)"; return; }

void App::gesture(bool begin) {
    if (begin) doc_.beginBatch();
    else doc_.endBatch();
}

void App::setParam(int ch, float v) {
    cancelDrag();
    NEED_SEL(l)
    (void)l;
    if (!doc_.setPropAt(selected_, static_cast<Prop>(ch), frame_, v)) note_ = "Gagal ubah: " + doc_.lastError();
}

void App::addEffect(const std::string& type) {
    cancelDrag();
    NEED_SEL(l)
    (void)l;
    const EffectDef* d = findEffectDef(type);
    if (!d) { note_ = "Efek tidak dikenal"; return; }
    const int i = doc_.addEffect(selected_, type, defaultParams(*d));
    note_ = i >= 0 ? std::string("Efek: ") + d->label : "Gagal tambah efek: " + doc_.lastError();
}

void App::removeEffect(int i) {
    cancelDrag();
    NEED_SEL(l)
    (void)l;
    if (!doc_.removeEffect(selected_, i)) note_ = "Gagal hapus efek: " + doc_.lastError();
    if (isEffectChannel(prop_)) prop_ = Prop::PosX;
}

void App::moveEffect(int i, int to) {
    cancelDrag();
    NEED_SEL(l)
    (void)l;
    if (!doc_.moveEffect(selected_, i, to)) note_ = "Gagal pindah efek: " + doc_.lastError();
    if (isEffectChannel(prop_)) prop_ = Prop::PosX;
}

void App::setEffectEnabled(int i, bool on) {
    cancelDrag();
    NEED_SEL(l)
    (void)l;
    if (!doc_.setEffectEnabled(selected_, i, on)) note_ = "Gagal: " + doc_.lastError();
}

void App::setShape(int kind, float corner, int sides, float inner) {
    cancelDrag();
    NEED_SEL(l)
    (void)l;
    if (kind < 0 || kind >= static_cast<int>(ShapeKind::Count)) return;
    if (!doc_.setShape(selected_, static_cast<ShapeKind>(kind), corner, sides, inner))
        note_ = "Gagal bentuk: " + doc_.lastError();
}

void App::setBlend(int mode) {
    cancelDrag();
    NEED_SEL(l)
    (void)l;
    if (mode < 0 || mode >= static_cast<int>(BlendMode::Count)) return;
    if (!doc_.setBlend(selected_, static_cast<BlendMode>(mode))) note_ = "Gagal blend: " + doc_.lastError();
}

void App::setColor(uint32_t rgba) {
    cancelDrag();
    NEED_SEL(l)
    (void)l;
    if (!doc_.setColor(selected_, rgba)) note_ = "Gagal warna: " + doc_.lastError();
}

void App::addImage(const std::string& name, int pw, int ph) {
    cancelDrag();
    const Project& p = doc_.project();
    if (pw <= 0 || ph <= 0) { note_ = "Gambar tidak valid"; return; }
    Layer l;
    l.name = "Gambar " + std::to_string(++addCount_);
    l.type = LayerType::Image;
    l.image = name;
    l.inFrame = frame_;
    l.outFrame = frame_ + 90;
    const float s = std::min(p.width * 0.8f / static_cast<float>(pw), p.height * 0.8f / static_cast<float>(ph));
    l.width = static_cast<float>(pw) * s;
    l.height = static_cast<float>(ph) * s;
    const LayerId id = doc_.addLayer(l);
    if (id) { selected_ = id; note_ = "Gambar ditambah"; }
    else note_ = "Gagal tambah gambar: " + doc_.lastError();
}

// ---------- Pilih banyak ----------
void App::setMultiMode(bool on) {
    cancelDrag();
    multiMode_ = on;
    multi_.clear();
    if (on && selected_) multi_.push_back(selected_);
}

void App::toggleMulti(LayerId id) {
    cancelDrag();
    if (!multiMode_ || !doc_.find(id)) return;
    for (size_t i = 0; i < multi_.size(); ++i)
        if (multi_[i] == id) { multi_.erase(multi_.begin() + static_cast<std::ptrdiff_t>(i)); return; }
    multi_.push_back(id);
    selected_ = id;
}

bool App::isMulti(LayerId id) const {
    for (LayerId m : multi_) if (m == id) return true;
    return false;
}

void App::groupMulti() {
    cancelDrag();
    if (multi_.empty()) { note_ = "Pilih layer dulu"; return; }
    const LayerId gid = doc_.groupLayers(multi_, "Grup " + std::to_string(++addCount_), static_cast<double>(frame_));
    if (gid) {
        selected_ = gid;
        note_ = multi_.size() > 1 ? "Layer dikelompokkan" : "Layer dibungkus grup";
        multiMode_ = false;
        multi_.clear();
    } else {
        note_ = "Gagal mengelompokkan: " + doc_.lastError();
    }
}

void App::deleteMulti() {
    cancelDrag();
    if (multi_.empty()) return;
    doc_.beginBatch();
    for (LayerId id : multi_) if (doc_.find(id)) doc_.removeLayer(id);
    doc_.endBatch();
    note_ = "Dihapus";
    multiMode_ = false;
    multi_.clear();
    selected_ = 0;
}

void App::duplicateMulti() {
    cancelDrag();
    if (multi_.empty()) return;
    doc_.beginBatch();
    const std::vector<LayerId> src = multi_;
    for (LayerId id : src) {
        selected_ = id;
        command(Cmd::Duplicate);
    }
    doc_.endBatch();
    multiMode_ = false;
    multi_.clear();
}

void App::ungroupSelected() {
    cancelDrag();
    NEED_SEL(l)
    if (l->type != LayerType::Group) { note_ = "Yang dipilih bukan grup"; return; }
    if (doc_.ungroup(selected_)) { selected_ = 0; note_ = "Grup dibongkar"; }
    else note_ = "Gagal membongkar: " + doc_.lastError();
}

void App::addGroup() {
    cancelDrag();
    Layer g;
    g.name = "Grup " + std::to_string(++addCount_);
    g.type = LayerType::Group;
    g.inFrame = 0;
    g.outFrame = std::max<int64_t>(doc_.project().durationFrames, frame_ + 30);
    g.width = g.height = 100.f;
    const LayerId id = doc_.addLayer(g);
    if (id) { selected_ = id; note_ = "Grup ditambah — pilih layer lalu tautkan ke grup ini lewat Induk"; }
    else note_ = "Gagal tambah grup: " + doc_.lastError();
}

void App::groupSelected() {
    cancelDrag();
    NEED_SEL(l)
    const LayerId child = selected_;
    const int64_t childOut = l->outFrame;
    const LayerId oldParent = l->parent;
    Layer g;
    g.name = "Grup " + std::to_string(++addCount_);
    g.type = LayerType::Group;
    g.inFrame = 0;
    g.outFrame = std::max<int64_t>(doc_.project().durationFrames, childOut);
    g.width = g.height = 100.f;
    g.parent = oldParent;
    doc_.beginBatch();
    const LayerId gid = doc_.addLayer(g);
    const bool ok = gid && doc_.setParent(child, gid);
    doc_.endBatch();
    if (ok) { selected_ = gid; note_ = "Layer dikelompokkan"; }
    else note_ = "Gagal mengelompokkan: " + doc_.lastError();
}

void App::setParentOfSelected(LayerId parent) {
    cancelDrag();
    NEED_SEL(l)
    if (parent == selected_) { note_ = "Layer tidak bisa menjadi induk dirinya sendiri"; return; }
    if (parent != 0) {
        const Layer* pl = doc_.find(parent);
        if (!pl || pl->type == LayerType::Audio) { note_ = "Layer suara tidak bisa menjadi induk"; return; }
    }
    if (!doc_.setParent(selected_, parent)) note_ = "Gagal menautkan: " + doc_.lastError();
    else note_ = parent ? "Layer ditautkan ke induk" : "Layer dilepas dari induk";
}

// Baris pertama: id induk sekarang. Berikutnya: id|nama untuk tiap Grup yang boleh jadi induk (bukan dirinya/keturunannya).
std::string App::parentData() const {
    const Layer* sel = selected_ ? doc_.find(selected_) : nullptr;
    if (!sel) return "";
    const Project& p = doc_.project();
    std::string s = std::to_string(sel->parent) + "\n";
    for (size_t gi = p.layers.size(); gi-- > 0;) {
        const Layer& g = p.layers[gi];
        if (g.type == LayerType::Audio || g.id == sel->id) continue;
        bool desc = false;   // g keturunan layer terpilih?
        LayerId cur = g.parent;
        for (size_t k = 0; cur != 0 && k <= p.layers.size(); ++k) {
            if (cur == sel->id) { desc = true; break; }
            const Layer* c = doc_.find(cur);
            if (!c) break;
            cur = c->parent;
        }
        if (desc) continue;
        s += std::to_string(g.id) + "|" + g.name + "|" + std::to_string(static_cast<int>(g.type)) + "\n";
    }
    return s;
}


void App::applyPreset(int preset, double seconds) {
    cancelDrag();
    NEED_SEL(l)
    if (preset < 0 || preset >= presetCount()) { note_ = "Preset tidak dikenal"; return; }
    if (l->type == LayerType::Audio) { note_ = "Preset tidak berlaku untuk layer suara"; return; }
    double s = seconds;
    if (!(s > 0.05)) s = 0.05;
    if (s > 10.0) s = 10.0;
    int64_t step = static_cast<int64_t>(std::llround(s * doc_.project().fps));
    if (step < 1) step = 1;
    if (!doc_.applyPreset(selected_, preset, step)) note_ = "Preset gagal: " + doc_.lastError();
    else note_ = std::string("Preset: ") + presetLabel(preset);
}

void App::renameSelected(const std::string& name) {
    cancelDrag();
    NEED_SEL(l)
    (void)l;
    std::string n = name.substr(0, 60);
    if (n.empty()) return;
    if (!doc_.renameLayer(selected_, n)) note_ = "Gagal ganti nama: " + doc_.lastError();
}

void App::setHidden(LayerId id, bool hidden) {
    cancelDrag();
    if (!doc_.find(id)) return;
    if (!doc_.setHidden(id, hidden)) note_ = "Gagal: " + doc_.lastError();
}

void App::resetChannel(int ch, float value) {
    cancelDrag();
    NEED_SEL(l)
    (void)l;
    if (!doc_.resetProp(selected_, static_cast<Prop>(ch), value)) note_ = "Gagal atur ulang: " + doc_.lastError();
}

// Preset tersimpan: teks proyek satu-layer (format file proyek). Bagian yang disimpan dipilih saat menerapkan lewat mask:
// 1 warna & bentuk, 2 blend & opasitas, 4 transform & animasi, 8 efek.
std::string App::presetExport() const {
    const Layer* l = selected_ ? doc_.find(selected_) : nullptr;
    if (!l || l->type == LayerType::Audio) return "";
    Project p;
    p.width = doc_.project().width;
    p.height = doc_.project().height;
    p.fps = doc_.project().fps;
    p.durationFrames = std::max<int64_t>(doc_.project().durationFrames, l->outFrame);
    Layer c = *l;
    c.id = 1;
    c.parent = 0;
    c.matte = 0;
    c.hidden = false;
    p.layers.push_back(std::move(c));
    p.nextId = 2;
    return serialize(p);
}

void App::presetApply(const std::string& text, uint32_t mask) {
    cancelDrag();
    NEED_SEL(l)
    (void)l;
    Project p;
    std::string err;
    if (!deserialize(text, &p, &err) || p.layers.empty()) { note_ = "Preset rusak"; return; }
    if (mask == 0) { note_ = "Tidak ada bagian preset yang dipilih"; return; }
    if (doc_.pasteStyle(selected_, p.layers[0], mask)) note_ = "Preset diterapkan";
    else note_ = "Gagal menerapkan preset: " + doc_.lastError();
}

void App::setMatte(int mode) {
    cancelDrag();
    NEED_SEL(l)
    if (mode < 0 || mode > 2) return;
    const Project& p = doc_.project();
    size_t z = 0;
    for (size_t i = 0; i < p.layers.size(); ++i)
        if (p.layers[i].id == l->id) z = i;
    if (mode != 0 && z == 0) { note_ = "Masker butuh layer di bawahnya untuk dipotong"; return; }
    if (mode != 0) {
        const Layer& below = p.layers[z - 1];
        if (below.type == LayerType::Group || below.type == LayerType::Null || below.type == LayerType::Audio) {
            note_ = "Layer tepat di bawahnya harus bentuk, gambar, video, atau teks";
            return;
        }
        if (below.matte != 0) { note_ = "Layer di bawahnya sudah menjadi masker"; return; }
    }
    if (!doc_.setMatte(selected_, mode)) note_ = "Gagal matte: " + doc_.lastError();
    else note_ = mode == 0 ? "Masker dimatikan" : "Masker aktif: layer di bawah hanya terlihat lewat bentuk ini";
}

// "mode|namaLayerBawah|status": status 0 siap, 1 tak ada layer di bawah, 2 layer bawah tak bisa dipotong, 3 layer bawah juga masker,
// 4 layer ini tak bisa jadi masker. Kosong bila tidak ada layer terpilih.
std::string App::matteInfo() const {
    const Layer* l = selected_ ? doc_.find(selected_) : nullptr;
    if (!l) return "";
    const Project& p = doc_.project();
    size_t z = 0;
    for (size_t i = 0; i < p.layers.size(); ++i)
        if (p.layers[i].id == l->id) z = i;
    auto bad = [](const Layer& x) { return x.type == LayerType::Group || x.type == LayerType::Null || x.type == LayerType::Audio; };
    int status = 0;
    std::string below;
    if (bad(*l)) status = 4;
    else if (z == 0) status = 1;
    else {
        const Layer& b = p.layers[z - 1];
        below = b.name;
        if (bad(b)) status = 2;
        else if (b.matte != 0) status = 3;
    }
    return std::to_string(static_cast<int>(l->matte)) + "|" + below + "|" + std::to_string(status);
}

int App::matteMode() const {
    const Layer* l = selected_ ? doc_.find(selected_) : nullptr;
    return l ? static_cast<int>(l->matte) : -1;
}

std::string App::presetCatalog() {
    std::string s;
    for (int i = 0; i < presetCount(); ++i)
        s += std::string(presetId(i)) + "|" + std::to_string(presetCategory(i)) + "|" + presetLabel(i) + "\n";
    return s;
}

void App::addText(const std::string& image, int pw, int ph, const std::string& text, float fontSize, int style) {
    cancelDrag();
    if (pw <= 0 || ph <= 0 || text.empty()) { note_ = "Teks tidak valid"; return; }
    Layer l;
    size_t cut = std::min<size_t>(24, text.size());
    while (cut > 0 && cut < text.size() && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) --cut;   // jangan memotong di tengah huruf UTF-8
    std::string nm = text.substr(0, cut);
    for (char& ch : nm) if (ch == '\n') ch = ' ';
    l.name = nm;
    l.type = LayerType::Text;
    l.image = image;
    l.text = text;
    l.fontSize = fontSize;
    l.textStyle = static_cast<uint8_t>(style & 15);
    l.inFrame = frame_;
    l.outFrame = frame_ + 90;
    l.width = static_cast<float>(pw);
    l.height = static_cast<float>(ph);
    const LayerId id = doc_.addLayer(l);
    if (id) { selected_ = id; note_ = "Teks ditambah"; }
    else note_ = "Gagal tambah teks: " + doc_.lastError();
}

void App::setText(const std::string& image, int pw, int ph, const std::string& text, float fontSize, int style) {
    cancelDrag();
    NEED_SEL(l)
    (void)l;
    if (pw <= 0 || ph <= 0 || text.empty()) { note_ = "Teks tidak valid"; return; }
    if (!doc_.setText(selected_, image, static_cast<float>(pw), static_cast<float>(ph), text, fontSize,
                      static_cast<uint8_t>(style & 15)))
        note_ = "Gagal ubah teks: " + doc_.lastError();
}

std::string App::textData() const {
    const Layer* l = selected_ ? doc_.find(selected_) : nullptr;
    if (!l || l->type != LayerType::Text) return "";
    std::string t;
    for (char ch : l->text) {
        if (ch == '\\') t += "\\\\";
        else if (ch == '\n') t += "\\n";
        else t += ch;
    }
    return std::to_string(static_cast<double>(l->fontSize)) + "|" + std::to_string(static_cast<int>(l->textStyle)) + "|" + t;
}

void App::addVideo(const std::string& clip, int pw, int ph, int64_t frames) {
    cancelDrag();
    const Project& p = doc_.project();
    if (pw <= 0 || ph <= 0 || frames <= 0) { note_ = "Video tidak valid"; return; }
    Layer l;
    l.name = "Video " + std::to_string(++addCount_);
    l.type = LayerType::Video;
    l.image = clip;
    l.srcStart = 0;
    l.srcCount = frames;
    l.rate = 1.f;
    l.inFrame = frame_;
    l.outFrame = frame_ + frames;
    const float s = std::min(p.width / static_cast<float>(pw), p.height / static_cast<float>(ph));
    l.width = static_cast<float>(pw) * s;
    l.height = static_cast<float>(ph) * s;
    const LayerId id = doc_.addLayer(l);
    if (id) { selected_ = id; note_ = "Video ditambah"; }
    else note_ = "Gagal tambah video: " + doc_.lastError();
}

void App::addAudio(const std::string& clip, int64_t frames, int64_t inFrame, const std::string& name) {
    cancelDrag();
    if (frames <= 0) { note_ = "Suara tidak valid"; return; }
    Layer l;
    l.name = name.empty() ? "Suara " + std::to_string(++addCount_) : name.substr(0, 40);
    l.type = LayerType::Audio;
    l.image = clip;
    l.srcStart = 0;
    l.srcCount = frames;
    l.rate = 1.f;
    l.inFrame = inFrame >= 0 ? inFrame : frame_;
    l.outFrame = l.inFrame + frames;
    l.width = 100.f;
    l.height = 100.f;
    l.color = 0x4CAF7AFF;
    const LayerId id = doc_.addLayer(l);
    if (id) { selected_ = id; note_ = "Suara ditambah"; }
    else note_ = "Gagal tambah suara: " + doc_.lastError();
}

std::string App::audioData() const {
    std::string s;
    for (const Layer& l : doc_.project().layers) {
        if (l.type != LayerType::Audio) continue;
        s += std::to_string(l.id) + "|" + l.image + "|" + std::to_string(l.inFrame) + "|" + std::to_string(l.outFrame) + "|" +
             std::to_string(l.srcStart) + "|" + std::to_string(static_cast<double>(l.rate)) + "|" +
             std::to_string(static_cast<double>(evalTransform(l, static_cast<double>(frame_)).opacity)) + "\n";
    }
    return s;
}

float App::audioVolume(LayerId id, int64_t frame) const {
    const Layer* l = doc_.find(id);
    if (!l || l->type != LayerType::Audio) return 0.f;
    return evalTransform(*l, static_cast<double>(frame)).opacity;
}

void App::trimSelected(int64_t in, int64_t out) {
    cancelDrag();
    NEED_SEL(l)
    (void)l;
    if (!doc_.trimLayer(selected_, in, out)) note_ = "Gagal trim: " + doc_.lastError();
}

void App::shiftSelected(int64_t d) {
    cancelDrag();
    NEED_SEL(l)
    (void)l;
    d = std::max<int64_t>(d, -l->inFrame);   // boleh melewati ujung: durasi proyek ikut memanjang
    if (d == 0) return;
    if (!doc_.shiftLayer(selected_, d)) note_ = "Gagal geser: " + doc_.lastError();
}

void App::splitAt(int64_t frame) {
    cancelDrag();
    NEED_SEL(l)
    if (frame <= l->inFrame || frame >= l->outFrame) {
        note_ = "Playhead harus berada di dalam durasi layer";
        return;
    }
    const LayerId id = doc_.splitLayer(selected_, frame);
    if (id) {
        selected_ = id;
        note_ = "Layer dibelah di frame " + std::to_string(frame);
    } else {
        note_ = "Gagal membelah: " + doc_.lastError();
    }
}

void App::moveSelected(int idx) {
    cancelDrag();
    NEED_SEL(l)
    (void)l;
    if (idx < 0) return;
    if (!doc_.moveLayer(selected_, static_cast<size_t>(idx))) note_ = "Gagal pindah: " + doc_.lastError();
}

std::string App::layerState() const {
    const Layer* l = selected_ ? doc_.find(selected_) : nullptr;
    if (!l) return "";
    const Project& p = doc_.project();
    size_t z = 0;
    for (size_t i = 0; i < p.layers.size(); ++i)
        if (p.layers[i].id == l->id) z = i;
    std::string s;
    char buf[320];
    std::snprintf(buf, sizeof(buf), "L|%llu|%s|%d|%d|%g|%d|%g|%d|%u|%lld|%lld|%zu\n",
                  static_cast<unsigned long long>(l->id), l->name.c_str(), static_cast<int>(l->type),
                  static_cast<int>(l->shape), l->cornerRadius, l->sides, l->innerRatio,
                  static_cast<int>(l->blend), static_cast<unsigned>(l->color),
                  static_cast<long long>(l->inFrame), static_cast<long long>(l->outFrame), z);
    s += buf;
    {
        const Transform t = evalTransform(*l, static_cast<double>(frame_));
        std::snprintf(buf, sizeof(buf), "T|%g|%g|%g|%g|%g|%g|%g|%g|%g\n", static_cast<double>(t.posX),
                      static_cast<double>(t.posY), static_cast<double>(t.scaleX),
                      static_cast<double>(t.scaleY), static_cast<double>(t.rotation),
                      static_cast<double>(t.opacity), static_cast<double>(t.posZ),
                      static_cast<double>(t.anchorX), static_cast<double>(t.anchorY));
        s += buf;
    }
    {
        s += "K|";
        bool first = true;
        for (const Track& t : l->tracks) {
            s += (first ? "" : ",") + std::to_string(static_cast<int>(t.prop));
            first = false;
        }
        s += "\n";
    }
    for (size_t i = 0; i < l->effects.size(); ++i) {
        const Effect& e = l->effects[i];
        float v[kMaxEffectParams];
        evalEffectParams(e, layerTime(*l, static_cast<double>(frame_)), v);
        s += "F|" + std::to_string(i) + "|" + e.type + "|" + (e.enabled ? "1" : "0") + "|";
        for (size_t k = 0; k < e.params.size(); ++k) {
            std::snprintf(buf, sizeof(buf), "%s%g", k ? "," : "", v[k]);
            s += buf;
        }
        s += "|";
        bool first = true;
        for (const Track& t : e.tracks) {
            s += (first ? "" : ",") + std::to_string(static_cast<int>(t.prop));
            first = false;
        }
        s += "\n";
    }
    return s;
}

std::string App::timelineData() const {
    const Project& p = doc_.project();
    std::string s;
    for (size_t i = p.layers.size(); i-- > 0;) {
        const Layer& l = p.layers[i];
        // semua frame keyframe (gabungan seluruh kanal), unik & terurut
        std::vector<int64_t> all;
        auto addKeys = [&](const std::vector<Track>& ts) {
            for (const Track& t : ts)
                for (const Keyframe& k : t.keys) all.push_back(k.frame);
        };
        addKeys(l.tracks);
        for (const Effect& e : l.effects) addKeys(e.tracks);
        std::sort(all.begin(), all.end());
        all.erase(std::unique(all.begin(), all.end()), all.end());
        std::string ks;
        for (int64_t f : all) ks += (ks.empty() ? "" : ",") + std::to_string(f);
        // frame keyframe kanal yang sedang dipilih (untuk lane di bawah layer terpilih)
        std::string cs;
        if (l.id == selected_ && !multiMode_)
            if (const Track* t = findTrack(l, prop_))
                for (const Keyframe& k : t->keys) cs += (cs.empty() ? "" : ",") + std::to_string(k.frame);
        s += std::to_string(l.id) + "|" + (l.parent != 0 ? "› " : "") + (l.matte != 0 ? "◐ " : "") + l.name + "|" + std::to_string(l.inFrame) + "|" +
             std::to_string(l.outFrame) + "|" + std::to_string(static_cast<int>(l.type)) + "|" +
             std::to_string(l.parent) + "|" + (multiMode_ ? (isMulti(l.id) ? "2" : "0") : (l.id == selected_ ? "1" : "0")) + "|" + std::to_string(l.color) + "|" +
             ks + "|" + cs + "|" + (l.hidden ? "1" : "0") + "\n";
    }
    return s;
}

void App::normalizeSelection() {
    if (selected_ != 0 && !doc_.find(selected_)) selected_ = 0;
    for (size_t i = multi_.size(); i-- > 0;)
        if (!doc_.find(multi_[i])) multi_.erase(multi_.begin() + static_cast<std::ptrdiff_t>(i));
}

void App::command(Cmd cmd) {
    cancelDrag();
    switch (cmd) {
        case Cmd::AddShape: {
            Layer l;
            l.name = "Bentuk " + std::to_string(++addCount_);
            int64_t in = frame_;
            const int64_t out = in + 90;
            l.inFrame = in;
            l.outFrame = out;
            l.color = kPalette[addCount_ % kPaletteN];
            l.width = 260;
            l.height = 260;
            const LayerId id = doc_.addLayer(l);
            if (id) {
                selected_ = id;
                note_ = "Ditambah: " + l.name;
            } else {
                note_ = "Gagal tambah: " + doc_.lastError();
            }
            break;
        }
        case Cmd::Split:
            splitAt(frame_);
            break;
        case Cmd::Duplicate: {
            const Layer* src = selected_ ? doc_.find(selected_) : nullptr;
            if (!src) {
                note_ = "Pilih layer dulu (ketuk)";
                break;
            }
            if (src->type == LayerType::Group) {
                note_ = "Grup belum bisa digandakan";
                break;
            }
            Layer copy = *src;
            copy.name += " salinan";
            copy.matte = 0;
            copy.transform.posX += 40;
            copy.transform.posY += 40;
            const LayerId id = doc_.addLayer(copy);
            if (id) {
                selected_ = id;
                note_ = "Layer digandakan";
            } else {
                note_ = "Gagal menggandakan: " + doc_.lastError();
            }
            break;
        }
        case Cmd::Delete: {
            if (!selected_) {
                note_ = "Pilih layer dulu (ketuk)";
                break;
            }
            if (doc_.removeLayer(selected_)) {
                selected_ = 0;
                note_ = "Dihapus";
            } else {
                note_ = "Gagal hapus: " + doc_.lastError();
            }
            break;
        }
        case Cmd::Undo:
            note_ = doc_.undo() ? "Undo" : "Tidak ada yang di-undo";
            break;
        case Cmd::Redo:
            note_ = doc_.redo() ? "Redo" : "Tidak ada yang di-redo";
            break;
    }
    normalizeSelection();
}

std::string App::info() const {
    const Project& p = doc_.project();
    const Layer* sel = selected_ ? doc_.find(selected_) : nullptr;
    char buf[640];
    std::snprintf(buf, sizeof(buf),
                  "Frame %lld/%lld  |  layer %zu  |  undo %zu redo %zu\n"
                  "Terpilih: %s%s  [%s]\n"
                  "%s  |  %s",
                  static_cast<long long>(frame_), static_cast<long long>(p.durationFrames),
                  p.layers.size(), doc_.undoDepth(), doc_.redoDepth(),
                  sel ? sel->name.c_str() : "-", dragging_ ? " (menyeret)" : "", propName(prop_).c_str(),
                  note_.c_str(),
                  !saveOk_ ? "GAGAL SIMPAN" : (dirty_ ? "menyimpan..." : "tersimpan"));
    return buf;
}

}  // namespace studio
