#pragma once
// Evaluasi animasi: nilai properti pada waktu tertentu. Murni C++ (tanpa OpenGL).

#include "model/model.h"

namespace studio {

// Nilai satu segmen Bezier antara k0 dan k1 pada frame (k0.frame <= frame <= k1.frame).
float evalSegment(const Keyframe& k0, const Keyframe& k1, double frame);

// Nilai track pada frame (konstan sebelum keyframe pertama dan sesudah yang terakhir).
float evalTrack(const Track& t, double frame);

// Track untuk sebuah kanal (properti Transform atau parameter efek); null jika tidak beranimasi.
const Track* findTrack(const Layer& l, Prop channel);

// Nilai dasar (statis) sebuah kanal; 0 jika kanal tidak ada.
float baseValue(const Layer& l, Prop channel);

// Waktu-layer pada frame proyek: bila ada kurva Waktu, hasil kurva itu; jika tidak, frame itu sendiri.
double layerTime(const Layer& l, double frame);

// Nilai kanal: dari track jika ada, jika tidak nilai dasar.
float evalProp(const Layer& l, Prop channel, double frame);

// Parameter efek pada frame (nilai dasar + animasi). out minimal kMaxEffectParams; sisanya 0.
void evalEffectParams(const Effect& e, double frame, float* out);

// Transform lengkap pada frame. Opacity dijepit ke 0..1 (kurva boleh overshoot).
Transform evalTransform(const Layer& l, double frame);

// Konversi preset gaya CSS cubic-bezier(x1,y1,x2,y2) menjadi handle untuk segmen v0 -> v1.
void easeToHandles(float x1, float y1, float x2, float y2, float v0, float v1, Keyframe* a,
                   Keyframe* b);

}  // namespace studio
