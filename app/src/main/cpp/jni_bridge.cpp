#include <jni.h>

#include <mutex>
#include <string>
#include <vector>

#include "app.h"
#include "effects.h"
#include "engine.h"
#include "model/storage.h"

// Satu kunci global untuk SEMUA pemanggilan: thread UI dan thread GL tidak pernah
// menyentuh App/Engine bersamaan.
static std::mutex g_mutex;
static studio::App g_app;
static Engine g_engine;

#define LOCK std::lock_guard<std::mutex> lock(g_mutex)
#define FN(name) Java_com_mystudio_editor_NativeEngine_##name

extern "C" {

JNIEXPORT void JNICALL FN(nativeOpen)(JNIEnv* env, jobject, jstring path) {
    const char* c = env->GetStringUTFChars(path, nullptr);
    std::string p = c ? c : "";
    if (c) env->ReleaseStringUTFChars(path, c);
    LOCK;
    g_app.open(p);
}

JNIEXPORT void JNICALL FN(nativeInit)(JNIEnv*, jobject) {
    LOCK;
    g_engine.init();
}

JNIEXPORT void JNICALL FN(nativeResize)(JNIEnv*, jobject, jint w, jint h) {
    LOCK;
    g_engine.resize(w, h);
    g_app.setViewSize(w, h);
}

JNIEXPORT void JNICALL FN(nativeRender)(JNIEnv*, jobject, jlong timeMs) {
    LOCK;
    g_app.tick(static_cast<int64_t>(timeMs));
    g_engine.render(g_app, static_cast<int64_t>(timeMs));
}

JNIEXPORT void JNICALL FN(nativeSaveNow)(JNIEnv*, jobject) {
    LOCK;
    g_app.saveNow();
}

JNIEXPORT void JNICALL FN(nativeTouch)(JNIEnv*, jobject, jint action, jfloat x, jfloat y) {
    if (action < 0 || action > 3) return;
    LOCK;
    g_app.touch(static_cast<studio::TouchAction>(action), x, y);
}

JNIEXPORT void JNICALL FN(nativeTogglePlay)(JNIEnv*, jobject) {
    LOCK;
    g_app.togglePlay();
}

JNIEXPORT void JNICALL FN(nativeSeek)(JNIEnv*, jobject, jint frame) {
    LOCK;
    g_app.seek(frame);
}

JNIEXPORT void JNICALL FN(nativeCommand)(JNIEnv*, jobject, jint cmd) {
    if (cmd < 0 || cmd > 5) return;
    LOCK;
    g_app.command(static_cast<studio::Cmd>(cmd));
}

JNIEXPORT void JNICALL FN(nativeSelectProp)(JNIEnv*, jobject, jint prop) {
    LOCK;
    g_app.selectProp(prop);
}

JNIEXPORT void JNICALL FN(nativeKeyHere)(JNIEnv*, jobject, jboolean both) {
    LOCK;
    g_app.keyHere(both != JNI_FALSE);
}

JNIEXPORT void JNICALL FN(nativeKeyMask)(JNIEnv*, jobject, jint mask) {
    LOCK;
    g_app.keyMask(static_cast<uint32_t>(mask));
}

JNIEXPORT void JNICALL FN(nativeKeyEdit)(JNIEnv*, jobject, jint op, jint frame, jint newFrame,
                                         jfloat a, jfloat b, jfloat c, jfloat d) {
    if (op < 0 || op > 6) return;
    const float v[4] = {a, b, c, d};
    LOCK;
    g_app.keyEdit(op, frame, newFrame, v);
}

JNIEXPORT jfloatArray JNICALL FN(nativeGraphData)(JNIEnv* env, jobject) {
    std::vector<float> data;
    {
        LOCK;
        g_app.graphData(&data);
    }
    jfloatArray arr = env->NewFloatArray(static_cast<jsize>(data.size()));
    if (arr) env->SetFloatArrayRegion(arr, 0, static_cast<jsize>(data.size()), data.data());
    return arr;
}

JNIEXPORT jstring JNICALL FN(nativeInfo)(JNIEnv* env, jobject) {
    std::string s;
    {
        LOCK;
        s = g_app.info();
    }
    return env->NewStringUTF(s.c_str());
}

JNIEXPORT jint JNICALL FN(nativeFrame)(JNIEnv*, jobject) {
    LOCK;
    return static_cast<jint>(g_app.frame());
}

JNIEXPORT jint JNICALL FN(nativeDuration)(JNIEnv*, jobject) {
    LOCK;
    return static_cast<jint>(g_app.project().durationFrames);
}

JNIEXPORT jboolean JNICALL FN(nativePlaying)(JNIEnv*, jobject) {
    LOCK;
    return g_app.playing() ? JNI_TRUE : JNI_FALSE;
}

static std::string jstr(JNIEnv* env, jstring js) {
    if (!js) return "";
    const char* c = env->GetStringUTFChars(js, nullptr);
    std::string r = c ? c : "";
    if (c) env->ReleaseStringUTFChars(js, c);
    return r;
}

static jstring toJ(JNIEnv* env, const std::string& s) { return env->NewStringUTF(s.c_str()); }

JNIEXPORT void JNICALL FN(nativeSelectLayer)(JNIEnv*, jobject, jint id) {
    LOCK;
    g_app.selectLayer(id < 0 ? 0 : static_cast<studio::LayerId>(id));
}

JNIEXPORT void JNICALL FN(nativeSetParam)(JNIEnv*, jobject, jint ch, jfloat v) {
    if (ch < 0 || ch > 0xFFFF) return;
    LOCK;
    g_app.setParam(ch, v);
}

JNIEXPORT void JNICALL FN(nativeGesture)(JNIEnv*, jobject, jboolean begin) {
    LOCK;
    g_app.gesture(begin != JNI_FALSE);
}

JNIEXPORT jint JNICALL FN(nativeFps)(JNIEnv*, jobject) {
    LOCK;
    return static_cast<jint>(g_app.fps());
}

JNIEXPORT void JNICALL FN(nativeAddEffect)(JNIEnv* env, jobject, jstring type) {
    const std::string t = jstr(env, type);
    LOCK;
    g_app.addEffect(t);
}

JNIEXPORT void JNICALL FN(nativeRemoveEffect)(JNIEnv*, jobject, jint i) {
    LOCK;
    g_app.removeEffect(i);
}

JNIEXPORT void JNICALL FN(nativeMoveEffect)(JNIEnv*, jobject, jint i, jint to) {
    LOCK;
    g_app.moveEffect(i, to);
}

JNIEXPORT void JNICALL FN(nativeSetEffectEnabled)(JNIEnv*, jobject, jint i, jboolean on) {
    LOCK;
    g_app.setEffectEnabled(i, on != JNI_FALSE);
}

JNIEXPORT void JNICALL FN(nativeSetShape)(JNIEnv*, jobject, jint kind, jfloat corner, jint sides, jfloat inner) {
    LOCK;
    g_app.setShape(kind, corner, sides, inner);
}

JNIEXPORT void JNICALL FN(nativeSetBlend)(JNIEnv*, jobject, jint mode) {
    LOCK;
    g_app.setBlend(mode);
}

JNIEXPORT void JNICALL FN(nativeSetColor)(JNIEnv*, jobject, jint argb) {
    // Kotlin memberi ARGB; model memakai RGBA (0xRRGGBBAA).
    const uint32_t u = static_cast<uint32_t>(argb);
    const uint32_t rgba = ((u & 0x00FFFFFFu) << 8) | ((u >> 24) & 0xFFu);
    LOCK;
    g_app.setColor(rgba);
}

JNIEXPORT void JNICALL FN(nativeAddImage)(JNIEnv* env, jobject, jstring name, jint w, jint h) {
    const std::string n = jstr(env, name);
    LOCK;
    g_app.addImage(n, w, h);
}

JNIEXPORT void JNICALL FN(nativeAddVideo)(JNIEnv* env, jobject, jstring clip, jint w, jint h, jint frames) {
    const std::string n = jstr(env, clip);
    LOCK;
    g_app.addVideo(n, w, h, frames);
}

JNIEXPORT void JNICALL FN(nativeAddGroup)(JNIEnv*, jobject) {
    LOCK;
    g_app.addGroup();
}

JNIEXPORT void JNICALL FN(nativeGroupSelected)(JNIEnv*, jobject) {
    LOCK;
    g_app.groupSelected();
}

JNIEXPORT void JNICALL FN(nativeSetParent)(JNIEnv*, jobject, jint id) {
    LOCK;
    g_app.setParentOfSelected(static_cast<studio::LayerId>(id < 0 ? 0 : id));
}

JNIEXPORT jstring JNICALL FN(nativeParentData)(JNIEnv* env, jobject) {
    std::string s;
    {
        LOCK;
        s = g_app.parentData();
    }
    return toJ(env, s);
}

JNIEXPORT void JNICALL FN(nativeApplyPreset)(JNIEnv*, jobject, jint preset, jdouble seconds) {
    LOCK;
    g_app.applyPreset(preset, seconds);
}

JNIEXPORT void JNICALL FN(nativeMultiMode)(JNIEnv*, jobject, jboolean on) {
    LOCK;
    g_app.setMultiMode(on == JNI_TRUE);
}

JNIEXPORT void JNICALL FN(nativeToggleMulti)(JNIEnv*, jobject, jint id) {
    LOCK;
    g_app.toggleMulti(static_cast<studio::LayerId>(id < 0 ? 0 : id));
}

JNIEXPORT jint JNICALL FN(nativeMultiCount)(JNIEnv*, jobject) {
    LOCK;
    return g_app.multiMode() ? g_app.multiCount() : -1;
}

// op: 0 kelompokkan, 1 hapus, 2 gandakan
JNIEXPORT void JNICALL FN(nativeMultiAction)(JNIEnv*, jobject, jint op) {
    LOCK;
    if (op == 0) g_app.groupMulti();
    else if (op == 1) g_app.deleteMulti();
    else if (op == 2) g_app.duplicateMulti();
}

JNIEXPORT void JNICALL FN(nativeUngroup)(JNIEnv*, jobject) {
    LOCK;
    g_app.ungroupSelected();
}

JNIEXPORT void JNICALL FN(nativeRename)(JNIEnv* env, jobject, jstring name) {
    const std::string n = jstr(env, name);
    LOCK;
    g_app.renameSelected(n);
}

JNIEXPORT void JNICALL FN(nativeSetHidden)(JNIEnv*, jobject, jint id, jboolean hidden) {
    LOCK;
    g_app.setHidden(static_cast<studio::LayerId>(id < 0 ? 0 : id), hidden == JNI_TRUE);
}

JNIEXPORT void JNICALL FN(nativeResetChannel)(JNIEnv*, jobject, jint ch, jfloat v) {
    LOCK;
    g_app.resetChannel(ch, v);
}

JNIEXPORT jstring JNICALL FN(nativePresetExport)(JNIEnv* env, jobject) {
    std::string s;
    {
        LOCK;
        s = g_app.presetExport();
    }
    return toJ(env, s);
}

JNIEXPORT void JNICALL FN(nativePresetApply)(JNIEnv* env, jobject, jstring text, jint mask) {
    const std::string t = jstr(env, text);
    LOCK;
    g_app.presetApply(t, static_cast<uint32_t>(mask));
}

JNIEXPORT void JNICALL FN(nativeSetMatte)(JNIEnv*, jobject, jint mode) {
    LOCK;
    g_app.setMatte(mode);
}

JNIEXPORT jstring JNICALL FN(nativeMatteInfo)(JNIEnv* env, jobject) {
    std::string s;
    {
        LOCK;
        s = g_app.matteInfo();
    }
    return toJ(env, s);
}

JNIEXPORT jint JNICALL FN(nativeMatteMode)(JNIEnv*, jobject) {
    LOCK;
    return g_app.matteMode();
}

JNIEXPORT jstring JNICALL FN(nativePresetCatalog)(JNIEnv* env, jobject) {
    return toJ(env, studio::App::presetCatalog());
}

JNIEXPORT void JNICALL FN(nativeAddText)(JNIEnv* env, jobject, jstring image, jint w, jint h, jstring text, jfloat size, jint style) {
    const std::string im = jstr(env, image);
    const std::string tx = jstr(env, text);
    LOCK;
    g_app.addText(im, w, h, tx, size, style);
}

JNIEXPORT void JNICALL FN(nativeSetText)(JNIEnv* env, jobject, jstring image, jint w, jint h, jstring text, jfloat size, jint style) {
    const std::string im = jstr(env, image);
    const std::string tx = jstr(env, text);
    LOCK;
    g_app.setText(im, w, h, tx, size, style);
}

JNIEXPORT jstring JNICALL FN(nativeTextData)(JNIEnv* env, jobject) {
    std::string s;
    {
        LOCK;
        s = g_app.textData();
    }
    return toJ(env, s);
}

JNIEXPORT void JNICALL FN(nativeAddAudio)(JNIEnv* env, jobject, jstring clip, jint frames, jint inFrame, jstring name) {
    const std::string n = jstr(env, clip);
    const std::string nm = jstr(env, name);
    LOCK;
    g_app.addAudio(n, frames, inFrame, nm);
}

JNIEXPORT jstring JNICALL FN(nativeAudioData)(JNIEnv* env, jobject) {
    std::string s;
    {
        LOCK;
        s = g_app.audioData();
    }
    return toJ(env, s);
}

JNIEXPORT jfloat JNICALL FN(nativeAudioVolume)(JNIEnv*, jobject, jint id, jint frame) {
    LOCK;
    return g_app.audioVolume(static_cast<studio::LayerId>(id < 0 ? 0 : id), frame);
}

// Ekspor: nama frame proxy video yang masih perlu diunggah ke GPU sebelum merender `frame`. Thread GL.
JNIEXPORT jobjectArray JNICALL FN(nativeMissingVideoFrames)(JNIEnv* env, jobject, jint frame) {
    std::vector<std::string> names;
    {
        LOCK;
        names = g_engine.missingVideoFrames(g_app.project(), static_cast<double>(frame));
    }
    jclass sc = env->FindClass("java/lang/String");
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(names.size()), sc, nullptr);
    for (size_t i = 0; i < names.size(); ++i) env->SetObjectArrayElement(arr, static_cast<jsize>(i), env->NewStringUTF(names[i].c_str()));
    return arr;
}

JNIEXPORT void JNICALL FN(nativeSplitAt)(JNIEnv*, jobject, jint frame) {
    LOCK;
    g_app.splitAt(frame);
}

JNIEXPORT void JNICALL FN(nativeSetThumbSource)(JNIEnv* env, jobject, jint idx, jbyteArray data, jint w, jint h) {
    if (!data || w <= 0 || h <= 0) return;
    const jsize n = env->GetArrayLength(data);
    if (static_cast<int64_t>(n) != static_cast<int64_t>(w) * h * 4) return;
    std::vector<uint8_t> px(static_cast<size_t>(n));
    env->GetByteArrayRegion(data, 0, n, reinterpret_cast<jbyte*>(px.data()));
    LOCK;
    g_engine.setThumbSource(idx, px.data(), w, h);
}

// Render satu frame untuk ekspor; kembalikan NV12 (null jika gagal). Harus dipanggil dari thread GL.
JNIEXPORT jbyteArray JNICALL FN(nativeRenderFrame)(JNIEnv* env, jobject, jint frame, jint w, jint h) {
    std::vector<uint8_t> nv;
    bool ok;
    {
        LOCK;
        ok = g_engine.renderFrameNV12(g_app.project(), static_cast<double>(frame), w, h, &nv);
    }
    if (!ok) return nullptr;
    jbyteArray arr = env->NewByteArray(static_cast<jsize>(nv.size()));
    if (!arr) return nullptr;
    env->SetByteArrayRegion(arr, 0, static_cast<jsize>(nv.size()), reinterpret_cast<const jbyte*>(nv.data()));
    return arr;
}

// Gambar mini proyek (RGBA, baris pertama = atas) untuk layar awal. Thread GL.
JNIEXPORT jbyteArray JNICALL FN(nativeRenderRGBA)(JNIEnv* env, jobject, jint frame, jint w, jint h) {
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048) return nullptr;
    std::vector<uint8_t> px;
    bool ok;
    {
        LOCK;
        std::vector<studio::Resolved> sc;
        studio::resolveScene(g_app.project(), static_cast<double>(frame), 0, nullptr, &sc);
        ok = g_engine.renderToPixels(g_app.project(), sc, static_cast<double>(frame), w, h, &px);
    }
    if (!ok) return nullptr;
    jbyteArray arr = env->NewByteArray(static_cast<jsize>(px.size()));
    if (!arr) return nullptr;
    env->SetByteArrayRegion(arr, 0, static_cast<jsize>(px.size()), reinterpret_cast<const jbyte*>(px.data()));
    return arr;
}

// Buat file proyek kosong baru (tanpa layer). Mengembalikan true jika tersimpan.
JNIEXPORT jboolean JNICALL FN(nativeCreateProject)(JNIEnv* env, jobject, jstring jpath, jint w, jint h, jint fps,
                                                   jint seconds) {
    if (!jpath || w < 16 || h < 16 || w > 4096 || h > 4096 || fps < 1 || fps > 120 || seconds < 1 || seconds > 600)
        return JNI_FALSE;
    const char* c = env->GetStringUTFChars(jpath, nullptr);
    if (!c) return JNI_FALSE;
    const std::string path(c);
    env->ReleaseStringUTFChars(jpath, c);
    studio::Project p;
    p.width = w;
    p.height = h;
    p.fps = fps;
    p.durationFrames = static_cast<int64_t>(seconds) * fps;   // proyek baru 1 dtk; tumbuh otomatis mengikuti layer
    if (!studio::validate(p)) return JNI_FALSE;
    return studio::saveProject(path, p) ? JNI_TRUE : JNI_FALSE;
}

// Salin/tempel. op: 0 salin, 1 tempel layer, 2 tempel gaya (mask bit: 1 warna&bentuk, 2 blend&opasitas, 4 transform, 8 efek)
JNIEXPORT void JNICALL FN(nativeClip)(JNIEnv*, jobject, jint op, jint mask) {
    LOCK;
    if (op == 0) g_app.copySelected();
    else if (op == 1) g_app.pasteLayer();
    else if (op == 2) g_app.pasteStyle(static_cast<uint32_t>(mask) & 15u);
}

JNIEXPORT jboolean JNICALL FN(nativeHasClip)(JNIEnv*, jobject) {
    LOCK;
    return g_app.hasClip() ? JNI_TRUE : JNI_FALSE;
}

// Tampilan kanvas. op: 0 zoom (a=faktor, b,c=fokus layar), 1 geser (a,b = dx,dy), 2 reset, 3 toggle garis bantu
JNIEXPORT void JNICALL FN(nativeView)(JNIEnv*, jobject, jint op, jfloat a, jfloat b, jfloat c) {
    LOCK;
    if (op == 0) g_app.zoomAt(a, b, c);
    else if (op == 1) g_app.panBy(a, b);
    else if (op == 2) g_app.resetView();
    else if (op == 3) g_app.toggleGrid();
}

// [zoom, panX, panY, grid(0/1)]
JNIEXPORT jfloatArray JNICALL FN(nativeViewState)(JNIEnv* env, jobject) {
    jfloat v[4];
    {
        LOCK;
        v[0] = g_app.viewZoom();
        v[1] = g_app.viewPanX();
        v[2] = g_app.viewPanY();
        v[3] = g_app.showGrid() ? 1.f : 0.f;
    }
    jfloatArray arr = env->NewFloatArray(4);
    if (arr) env->SetFloatArrayRegion(arr, 0, 4, v);
    return arr;
}

// Ukuran kanvas proyek (lebar, tinggi) untuk menentukan resolusi ekspor.
JNIEXPORT jintArray JNICALL FN(nativeProjectSize)(JNIEnv* env, jobject) {
    jint v[2];
    {
        LOCK;
        v[0] = g_app.project().width;
        v[1] = g_app.project().height;
    }
    jintArray arr = env->NewIntArray(2);
    if (arr) env->SetIntArrayRegion(arr, 0, 2, v);
    return arr;
}

JNIEXPORT void JNICALL FN(nativeRequestThumbs)(JNIEnv*, jobject) {
    LOCK;
    g_engine.requestThumbs();
}

JNIEXPORT jboolean JNICALL FN(nativeThumbsReady)(JNIEnv*, jobject) {
    LOCK;
    return g_engine.thumbsReady() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jbyteArray JNICALL FN(nativeThumb)(JNIEnv* env, jobject, jstring type) {
    const std::string t = jstr(env, type);
    std::vector<uint8_t> px;
    {
        LOCK;
        if (!g_engine.thumb(t, &px)) px.clear();
    }
    jbyteArray arr = env->NewByteArray(static_cast<jsize>(px.size()));
    if (arr && !px.empty()) env->SetByteArrayRegion(arr, 0, static_cast<jsize>(px.size()), reinterpret_cast<const jbyte*>(px.data()));
    return arr;
}

JNIEXPORT void JNICALL FN(nativeTrim)(JNIEnv*, jobject, jint in, jint out) {
    LOCK;
    g_app.trimSelected(in, out);
}

JNIEXPORT void JNICALL FN(nativeShift)(JNIEnv*, jobject, jint d) {
    LOCK;
    g_app.shiftSelected(d);
}

JNIEXPORT void JNICALL FN(nativeMoveLayer)(JNIEnv*, jobject, jint idx) {
    LOCK;
    g_app.moveSelected(idx);
}

JNIEXPORT void JNICALL FN(nativeSpeedCurve)(JNIEnv*, jobject) {
    LOCK;
    g_app.speedCurve();
}

JNIEXPORT void JNICALL FN(nativeScaleLayer)(JNIEnv*, jobject, jdouble factor) {
    LOCK;
    g_app.scaleSelected(factor);
}

// [indeks layer terpilih (0 = paling bawah, -1 = tidak ada), jumlah layer]
JNIEXPORT jintArray JNICALL FN(nativeLayerOrder)(JNIEnv* env, jobject) {
    jint v[2] = {-1, 0};
    {
        LOCK;
        int i = -1, n = 0;
        g_app.layerOrder(&i, &n);
        v[0] = i;
        v[1] = n;
    }
    jintArray arr = env->NewIntArray(2);
    env->SetIntArrayRegion(arr, 0, 2, v);
    return arr;
}

JNIEXPORT jstring JNICALL FN(nativeLayerState)(JNIEnv* env, jobject) {
    std::string s;
    {
        LOCK;
        s = g_app.layerState();
    }
    return toJ(env, s);
}

JNIEXPORT jstring JNICALL FN(nativeTimeline)(JNIEnv* env, jobject) {
    std::string s;
    {
        LOCK;
        s = g_app.timelineData();
    }
    return toJ(env, s);
}

JNIEXPORT jstring JNICALL FN(nativeEffectCatalog)(JNIEnv* env, jobject) {
    return toJ(env, studio::describeCatalog());
}

JNIEXPORT jstring JNICALL FN(nativeDiagnostics)(JNIEnv* env, jobject) {
    std::string s;
    {
        LOCK;
        s = g_engine.diagnostics();
    }
    return toJ(env, s);
}

JNIEXPORT void JNICALL FN(nativeRequestSelfTest)(JNIEnv*, jobject) {
    LOCK;
    g_engine.requestSelfTest();
}

JNIEXPORT jstring JNICALL FN(nativeNextImageRequest)(JNIEnv* env, jobject) {
    std::string s;
    {
        LOCK;
        s = g_engine.nextImageRequest();
    }
    return toJ(env, s);
}

JNIEXPORT void JNICALL FN(nativeSetImageTexture)(JNIEnv* env, jobject, jstring name, jint tex, jint w, jint h) {
    const std::string n = jstr(env, name);
    LOCK;
    g_engine.setImageTexture(n, static_cast<GLuint>(tex), w, h);
}

}  // extern "C"
