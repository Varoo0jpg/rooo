# Vlab

Editor video mobile (Android) dengan UI terinspirasi Alight Motion, dibangun di atas engine
rendering native sendiri (C++/OpenGL ES), diarahkan untuk mendukung alur kerja editing
animanga (AMV/MMV) dengan tool setara software desktop.

> Nama kode internal sebelumnya: *Studio Editor*. Nama project (`Vlab`) adalah identitas
> publik; namespace kode (`com.mystudio.editor`) belum diganti — lihat bagian **Rebranding**
> di bawah untuk kenapa itu sengaja ditunda.

## Status Project (per revisi ini)

**Jauh lebih matang dari deskripsi lama.** README versi sebelumnya menyebut hasil "Fase 0"
sebagai satu kotak berputar — itu sudah sangat tertinggal dari kondisi kode sebenarnya.
Status terverifikasi sejauh ini:

| Bagian | Status | Cara diverifikasi |
|---|---|---|
| Model data proyek (layer, transform, grouping, matte) | ✅ Solid | Fuzz test host, 0 gagal dari puluhan ribu pemeriksaan |
| Sistem animasi/keyframe | ✅ Solid | Fuzz test host, 129.703 pemeriksaan, 0 gagal |
| Resolusi scene (hierarki, parenting) | ✅ Solid | Test host, 0 gagal |
| 81 efek GLSL (termasuk 25 blend mode) | ✅ Solid | Dikompilasi & dirender di GPU asli (Mesa llvmpipe), 0 gagal |
| Export (NV12 render + mux) | ✅ Solid | Test host, 0 gagal |
| Jembatan JNI (83 fungsi) | ✅ Solid | Cross-check statis: cocok 100% Kotlin ⇄ C++ |
| Build Android (Gradle/Kotlin) | ⚠️ **Belum terverifikasi** | Belum pernah di-build dengan Android SDK sungguhan |
| Performa di device nyata (S24 / Honor Pad 10) | ⚠️ **Belum terverifikasi** | Menunggu build Android pertama berhasil |

## Prinsip Desain

Mengikuti arahan awal project:

1. **UI familiar, bukan tiruan.** Pola interaksi (rail kiri, panel layer, timeline bawah)
   terinspirasi Alight Motion untuk memudahkan adaptasi pengguna — bukan hasil reverse-engineer
   atau penyalinan aset. Kredit UI ke Alight Motion dicantumkan eksplisit, bukan "tersembunyi".
2. **Engine dipisah ketat dari UI dan dari OpenGL/Android** (`model/*`, `app.*` murni C++ tanpa
   dependency platform) — ini yang membuat seluruh logika bisa di-fuzz-test di komputer biasa
   tanpa emulator/device.
3. **Semua-atau-tidak-sama-sekali.** Perubahan proyek divalidasi penuh pada salinan sebelum
   diterapkan; undo/redo pakai snapshot utuh, bukan operasi kebalikan — menghindari kelas bug
   "undo yang meleset" yang umum di editor custom.
4. **Performance-first.** Satu ABI (`arm64-v8a`), `-O2 -fno-exceptions -fno-rtti` di build native,
   konteks GL dianggap bisa hilang kapan saja (`init()` dipanggil ulang tiap konteks baru).

## Arsitektur

```
app/src/main/java/.../editor/   Kotlin — UI (Activity, Compose/View custom), tidak menyimpan state proyek
app/src/main/cpp/
  model/                        Model proyek murni C++ (Document, Layer, Transform, Scene resolve, Anim)
  app.cpp / app.h               Logika aplikasi: proyek aktif, playhead, seleksi, gesture, autosave
  engine.cpp / engine.h         Rendering GPU: compositing, shader efek, thumbnail, texture video (LRU)
  effects.cpp / effects_more.cpp  81 efek sebagai shader GLSL + katalog metadata
  export.cpp / export.h         Render frame -> NV12 untuk dimux jadi video
  jni_bridge.cpp                83 fungsi JNI, satu mutex global (UI thread & GL thread aman bersamaan)
tests/                          Test host (tanpa Android) — lihat tests/run_tests.sh
tests/gl/gl_harness.cpp         Test rendering GPU headless (EGL surfaceless + Mesa software)
```

### Fitur yang sudah ada

- Timeline multi-track, trim/split, grouping & parenting layer, multi-select dengan aksi massal.
- Keyframe dengan **graph/curve editor** (`GraphView.kt` + kanal animasi di engine) — bukan cuma
  easing preset.
- 81 efek visual: blur, distorsi, transisi (wipe/iris/blinds), stylize, dan **color correction
  dasar** (brightness/contrast, exposure, levels, hue/saturation, temperature, tint, duotone,
  split-tone, gradient map, vibrance).
- 25 blend mode (lengkap termasuk HSL group: Hue/Saturation/Color/Luminosity).
- Matte/masking, preset animasi (masuk/keluar/loop) dengan catalog & apply.
- Audio: mixing multi-track, proxy, export ke video (mux).
- Proxy video (generate frame yang belum siap di latar, retry otomatis).
- Autosave + validasi "semua-atau-tidak" di setiap perubahan proyek.

### Fitur yang BELUM ada (prioritas lanjutan, sesuai arahan awal)

Urutan disarankan berdasarkan dependency teknis:

1. **Curve editor sebagai graph Bezier penuh** — `GraphView.kt` sudah ada sebagai fondasi;
   perlu dipastikan tangent in/out bisa diedit bebas (bukan cuma preset easing), karena ini
   dependency untuk item #2 dan #3 di bawah.
2. **Color grading penuh** — color wheels (lift/gamma/gain), curves per-channel RGB, import LUT
   (.cube). Saat ini baru efek warna individual, belum ada compositing grading bertingkat.
3. **Time remap / frame interpolation (Twixtor-like)** — belum ada sama sekali. Mode speed-ramp
   non-linear bisa pakai mesin kurva yang sama dengan keyframe transform; frame interpolation
   sungguhan (bukan cuma time-stretch) butuh model optical-flow terpisah — lihat catatan khusus
   animanga di bawah.
4. **Sistem kamera/objek 3D** — baru ada efek `rotate3d` sederhana (bukan sistem kamera
   sungguhan). `Transform` di model sudah punya `posZ`/`anchorZ`, jadi fondasi data sudah ada;
   yang belum ada adalah kamera virtual dengan path 3D dan compositing layer di ruang 3D (2.5D).
5. **Auto-parting animanga** — belum ada komponen ML sama sekali di codebase ini. Ini genuinely
   item riset terpisah (lihat dokumen rencana awal, Bagian 4.5): rencanakan sebagai *assisted
   segmentation* dulu (model + koreksi manual), bukan full-otomatis.

## Build & Jalankan

1. Android Studio terbaru, pasang **NDK** dan **CMake** lewat SDK Manager.
2. **File > Open**, pilih folder project ini. Tunggu Gradle sync (akan generate
   `gradlew`/`gradlew.bat` otomatis — belum ada di repo ini, butuh koneksi internet saat sync
   pertama).
3. JDK 17 untuk Gradle.
4. Jalankan ke device fisik (S24 atau Honor Pad 10) via USB debugging. **Belum disarankan
   emulator** — banyak bagian butuh test performa GPU/encoder hardware yang tidak terwakili
   akurat di emulator.
5. **Laporkan SELURUH pesan error build/sync apa adanya** jika gagal — ini build pertama kalinya
   di lingkungan dengan Android SDK sungguhan, jadi error di tahap ini sangat mungkin dan jadi
   prioritas utama untuk diperbaiki sebelum lanjut ke fitur baru.

### Menjalankan test (tanpa Android, langsung di komputer)

```sh
sh tests/run_tests.sh            # model, scene, app, anim, effects, fx, export — C++ murni
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 \
  g++ -std=c++20 -I app/src/main/cpp -I tests/gl/stub tests/gl/gl_harness.cpp \
      app/src/main/cpp/*.cpp app/src/main/cpp/model/*.cpp \
      -lEGL -lGLESv2 -o /tmp/gl_harness && /tmp/gl_harness /tmp/gl_out
```

## Target perangkat uji

- Samsung Galaxy S24
- Honor Pad 10

## Rebranding (`com.mystudio.editor` → nama final)

Namespace Kotlin/JNI (`com.mystudio.editor`) **belum diganti** ke identitas `Vlab` di revisi
ini. Alasannya: mengganti package name berarti mengganti awalan semua 83 nama fungsi JNI
(`Java_com_mystudio_editor_NativeEngine_...`) di `jni_bridge.cpp` sekaligus — perubahan besar
dan berisiko yang sebaiknya dilakukan sebagai langkah terpisah dan sengaja, dengan test suite
dijalankan ulang penuh sesudahnya, bukan ditempel bersamaan dengan perubahan dokumentasi ini.

## Catatan Lisensi/IP

UI *terinspirasi* pola interaksi Alight Motion (lihat Prinsip Desain #1) — tidak ada aset,
kode, atau hasil decompile dari Alight Motion maupun APK modifikasinya yang dipakai di project
ini. Seluruh kode di repo ini (Kotlin & C++) adalah tulisan sendiri.
