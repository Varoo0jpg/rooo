#include "model/anim.h"

#include <algorithm>
#include <cmath>

namespace studio {

namespace {

// Bezier kubik 1D dengan titik kontrol p0..p3 pada parameter s.
inline double bez(double p0, double p1, double p2, double p3, double s) {
    const double m = 1.0 - s;
    return m * m * m * p0 + 3.0 * m * m * s * p1 + 3.0 * m * s * s * p2 + s * s * s * p3;
}

// Cari s sehingga x(s) = u. x(s) monoton karena p1,p2 di 0..1; bisection menjamin hasil.
double solveS(double x1, double x2, double u) {
    if (u <= 0.0) return 0.0;
    if (u >= 1.0) return 1.0;
    double lo = 0.0, hi = 1.0, s = u;
    for (int i = 0; i < 6; ++i) {  // Newton cepat, dijaga tetap di dalam [lo,hi]
        const double x = bez(0, x1, x2, 1, s) - u;
        if (std::fabs(x) < 1e-9) return s;
        if (x > 0) hi = s; else lo = s;
        const double m = 1.0 - s;
        const double d = 3.0 * m * m * x1 + 6.0 * m * s * (x2 - x1) + 3.0 * s * s * (1.0 - x2);
        if (std::fabs(d) < 1e-6) break;
        const double ns = s - x / d;
        if (!(ns > lo && ns < hi)) break;
        s = ns;
    }
    for (int i = 0; i < 40; ++i) {
        s = 0.5 * (lo + hi);
        const double x = bez(0, x1, x2, 1, s);
        if (x < u) lo = s; else hi = s;
    }
    return 0.5 * (lo + hi);
}

}  // namespace

float evalSegment(const Keyframe& k0, const Keyframe& k1, double frame) {
    if (k0.interp == Interp::Hold) return frame >= static_cast<double>(k1.frame) ? k1.value : k0.value;
    const double dt = static_cast<double>(k1.frame - k0.frame);
    double u = (frame - static_cast<double>(k0.frame)) / dt;
    u = std::clamp(u, 0.0, 1.0);
    const double v0 = k0.value, v1 = k1.value;
    if (k0.interp == Interp::Linear) return static_cast<float>(v0 + (v1 - v0) * u);
    const double s = solveS(k0.outX, 1.0 - k1.inX, u);
    return static_cast<float>(bez(v0, v0 + k0.outY, v1 + k1.inY, v1, s));
}

float evalTrack(const Track& t, double frame) {
    const auto& k = t.keys;
    if (k.empty()) return 0.f;
    if (frame <= static_cast<double>(k.front().frame)) return k.front().value;
    if (frame >= static_cast<double>(k.back().frame)) return k.back().value;
    // Pertama yang frame-nya > frame -> segmen [i-1, i]
    size_t lo = 0, hi = k.size();
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (static_cast<double>(k[mid].frame) > frame) hi = mid; else lo = mid + 1;
    }
    return evalSegment(k[lo - 1], k[lo], frame);
}

const Track* findTrack(const Layer& l, Prop channel) {
    if (isEffectChannel(channel)) {
        const size_t fx = static_cast<size_t>(effectIndexOf(channel));
        if (fx >= l.effects.size()) return nullptr;
        const Prop pid = static_cast<Prop>(effectParamOf(channel));
        for (const Track& t : l.effects[fx].tracks)
            if (t.prop == pid) return &t;
        return nullptr;
    }
    for (const Track& t : l.tracks)
        if (t.prop == channel) return &t;
    return nullptr;
}

float baseValue(const Layer& l, Prop channel) {
    if (isEffectChannel(channel)) {
        const size_t fx = static_cast<size_t>(effectIndexOf(channel));
        const size_t p = static_cast<size_t>(effectParamOf(channel));
        if (fx >= l.effects.size() || p >= l.effects[fx].params.size()) return 0.f;
        return l.effects[fx].params[p];
    }
    if (static_cast<int>(channel) >= kPropCount) return 0.f;
    return getProp(l.transform, channel);
}

double layerTime(const Layer& l, double frame) {
    for (const Track& t : l.tracks)
        if (t.prop == Prop::Time) return static_cast<double>(evalTrack(t, frame));
    return frame;
}

float evalProp(const Layer& l, Prop channel, double frame) {
    if (channel == Prop::Time) {
        const Track* tt = findTrack(l, channel);
        return tt ? evalTrack(*tt, frame) : static_cast<float>(frame);
    }
    const Track* t = findTrack(l, channel);
    return t ? evalTrack(*t, layerTime(l, frame)) : baseValue(l, channel);
}

void evalEffectParams(const Effect& e, double frame, float* out) {
    for (int i = 0; i < kMaxEffectParams; ++i) out[i] = 0.f;
    for (size_t i = 0; i < e.params.size() && i < static_cast<size_t>(kMaxEffectParams); ++i)
        out[i] = e.params[i];
    for (const Track& t : e.tracks) {
        const size_t p = static_cast<size_t>(t.prop);
        if (p < static_cast<size_t>(kMaxEffectParams)) out[p] = evalTrack(t, frame);
    }
}

Transform evalTransform(const Layer& l, double frame) {
    Transform t = l.transform;
    const double lf = layerTime(l, frame);
    for (const Track& tr : l.tracks)
        if (tr.prop != Prop::Time) setProp(t, tr.prop, evalTrack(tr, lf));
    t.opacity = std::clamp(t.opacity, 0.f, 1.f);
    return t;
}

void easeToHandles(float x1, float y1, float x2, float y2, float v0, float v1, Keyframe* a,
                   Keyframe* b) {
    const float dv = v1 - v0;
    a->interp = Interp::Bezier;
    a->outX = x1;
    a->outY = y1 * dv;
    b->inX = 1.f - x2;
    b->inY = (y2 - 1.f) * dv;
}

}  // namespace studio
