#include "model/scene.h"

#include "model/anim.h"

#include <cmath>
#include <unordered_map>

namespace studio {

Affine operator*(const Affine& A, const Affine& B) {
    Affine r;
    r.a = A.a * B.a + A.c * B.b;
    r.b = A.b * B.a + A.d * B.b;
    r.c = A.a * B.c + A.c * B.d;
    r.d = A.b * B.c + A.d * B.d;
    r.tx = A.a * B.tx + A.c * B.ty + A.tx;
    r.ty = A.b * B.tx + A.d * B.ty + A.ty;
    return r;
}

Affine affineFromTransform(const Transform& t) {
    const float rad = t.rotation * 3.14159265358979f / 180.f;
    const float cs = std::cos(rad), sn = std::sin(rad);
    Affine m;
    m.a = cs * t.scaleX;
    m.b = sn * t.scaleX;
    m.c = -sn * t.scaleY;
    m.d = cs * t.scaleY;
    m.tx = t.posX - (m.a * t.anchorX + m.c * t.anchorY);
    m.ty = t.posY - (m.b * t.anchorX + m.d * t.anchorY);
    return m;
}

bool invert(const Affine& m, Affine* out) {
    const float det = m.a * m.d - m.b * m.c;
    if (!(std::fabs(det) > 1e-12f)) return false;
    Affine r;
    r.a = m.d / det;
    r.b = -m.b / det;
    r.c = -m.c / det;
    r.d = m.a / det;
    r.tx = -(r.a * m.tx + r.c * m.ty);
    r.ty = -(r.b * m.tx + r.d * m.ty);
    *out = r;
    return true;
}

void resolveScene(const Project& p, double frame, LayerId overrideId, const Transform* overrideT,
                  std::vector<Resolved>* out) {
    const auto& L = p.layers;
    const size_t n = L.size();
    out->assign(n, Resolved{});

    std::unordered_map<LayerId, size_t> idx;
    idx.reserve(n * 2 + 1);
    for (size_t i = 0; i < n; ++i) idx[L[i].id] = i;

    std::vector<char> done(n, 0);
    std::vector<size_t> chain;

    for (size_t i = 0; i < n; ++i) {
        if (done[i]) continue;

        // Kumpulkan rantai ke atas sampai ketemu layer yang sudah selesai atau root.
        chain.clear();
        size_t cur = i;
        for (;;) {
            chain.push_back(cur);
            const LayerId par = L[cur].parent;
            if (par == kRoot) break;
            auto it = idx.find(par);
            if (it == idx.end() || done[it->second]) break;
            cur = it->second;
            if (chain.size() > n) break;  // pengaman jika data tidak valid
        }

        // Proses dari yang paling atas (induk) turun ke layer i.
        for (size_t k = chain.size(); k-- > 0;) {
            const size_t li = chain[k];
            const Layer& l = L[li];
            Transform t = evalTransform(l, frame);
            if (overrideT && l.id == overrideId) t = *overrideT;

            Affine parentWorld;
            bool parentVisible = true;
            float parentOpacity = 1.f;
            if (l.parent != kRoot) {
                auto it = idx.find(l.parent);
                if (it != idx.end() && done[it->second]) {
                    const Resolved& pr = (*out)[it->second];
                    parentWorld = pr.world;
                    // Grup mewariskan visibilitas & opasitas; induk biasa (Parent & Link) hanya mewariskan transform.
                    if (L[it->second].type == LayerType::Group) {
                        parentVisible = pr.visible;
                        parentOpacity = pr.opacity;
                    }
                }
            }
            Resolved& r = (*out)[li];
            r.world = parentWorld * affineFromTransform(t);
            r.visible = parentVisible && !l.hidden && frame >= l.inFrame && frame < l.outFrame;
            r.opacity = t.opacity * parentOpacity;
            done[li] = 1;
        }
    }
}

LayerId hitTest(const Project& p, const std::vector<Resolved>& scene, float x, float y) {
    const auto& L = p.layers;
    if (scene.size() != L.size()) return 0;
    for (size_t i = L.size(); i-- > 0;) {
        const Layer& l = L[i];
        const Resolved& r = scene[i];
        if (!r.visible || l.type == LayerType::Group || l.type == LayerType::Null || l.type == LayerType::Audio || r.opacity <= 0.001f) continue;
        Affine inv;
        if (!invert(r.world, &inv)) continue;
        float lx, ly;
        inv.apply(x, y, &lx, &ly);
        if (std::fabs(lx) <= l.width * 0.5f && std::fabs(ly) <= l.height * 0.5f) return l.id;
    }
    return 0;
}

Affine parentWorldOf(const Project& p, const std::vector<Resolved>& scene, LayerId id) {
    const auto& L = p.layers;
    if (scene.size() != L.size()) return Affine{};
    LayerId parent = kRoot;
    for (const Layer& l : L)
        if (l.id == id) {
            parent = l.parent;
            break;
        }
    if (parent == kRoot) return Affine{};
    for (size_t i = 0; i < L.size(); ++i)
        if (L[i].id == parent) return scene[i].world;
    return Affine{};
}

bool canvasDeltaToParent(const Affine& parentWorld, float dx, float dy, float* ox, float* oy) {
    Affine inv;
    if (!invert(parentWorld, &inv)) return false;
    *ox = inv.a * dx + inv.c * dy;  // hanya bagian linear (selisih, bukan titik)
    *oy = inv.b * dx + inv.d * dy;
    return true;
}

}  // namespace studio
