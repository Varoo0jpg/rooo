package com.mystudio.editor

/**
 * Jembatan Kotlin -> engine C++.
 * Semua pemanggilan aman dari thread mana pun: sisi C++ memakai satu kunci global.
 */
object NativeEngine {
    init {
        System.loadLibrary("studio_engine")
    }

    external fun nativeOpen(path: String)
    external fun nativeInit()
    external fun nativeResize(width: Int, height: Int)
    external fun nativeRender(timeMs: Long)
    external fun nativeSaveNow()
    external fun nativeTouch(action: Int, x: Float, y: Float)
    external fun nativeTogglePlay()
    external fun nativeSeek(frame: Int)
    external fun nativeCommand(cmd: Int)
    external fun nativeSelectProp(prop: Int)
    external fun nativeKeyHere(both: Boolean)
    /** op: 0 pindah/ubah nilai, 1 handle segmen, 2 hapus, 3 jenis interp (newFrame), 4 preset easing. */
    external fun nativeKeyEdit(op: Int, frame: Int, newFrame: Int, a: Float, b: Float, c: Float, d: Float)
    /** [nKeys(-1=tak ada layer), playhead, nilai, durasi, lalu 7 angka per keyframe]. */
    external fun nativeGraphData(): FloatArray
    external fun nativeInfo(): String
    external fun nativeFrame(): Int
    external fun nativeDuration(): Int
    external fun nativePlaying(): Boolean

    // --- Paket A ---
    external fun nativeSelectLayer(id: Int)
    /** Kanal: 0..9 properti transform; >=256 parameter efek (256 + efek*32 + param). Auto-key. */
    external fun nativeSetParam(channel: Int, value: Float)
    /** Awal/akhir gestur seret: semua perubahan di antaranya = satu langkah undo. */
    external fun nativeGesture(begin: Boolean)
    external fun nativeFps(): Int
    external fun nativeAddEffect(type: String)
    external fun nativeRemoveEffect(index: Int)
    external fun nativeMoveEffect(index: Int, to: Int)
    external fun nativeSetEffectEnabled(index: Int, enabled: Boolean)
    external fun nativeSetShape(kind: Int, corner: Float, sides: Int, inner: Float)
    external fun nativeSetBlend(mode: Int)
    external fun nativeSetColor(argb: Int)
    external fun nativeAddImage(name: String, w: Int, h: Int)
    external fun nativeSplitAt(frame: Int)
    /** Foto sumber contoh efek: RGBA w*h*4 (baris pertama = atas). */
    external fun nativeSetThumbSource(idx: Int, rgba: ByteArray, w: Int, h: Int)
    /** NV12 satu frame untuk ekspor (thread GL). null jika gagal. */
    external fun nativeRenderFrame(frame: Int, w: Int, h: Int): ByteArray?
    external fun nativeProjectSize(): IntArray
    /** op: 0 salin, 1 tempel layer, 2 tempel gaya (mask: 1 warna&bentuk, 2 blend&opasitas, 4 transform, 8 efek). */
    external fun nativeClip(op: Int, mask: Int)
    external fun nativeHasClip(): Boolean
    /** op: 0 zoom(a=faktor,b,c=fokus), 1 geser(a,b), 2 reset, 3 garis bantu. */
    external fun nativeView(op: Int, a: Float, b: Float, c: Float)
    /** [zoom, panX, panY, grid] */
    external fun nativeViewState(): FloatArray
    /** RGBA mini proyek (thread GL). */
    external fun nativeRenderRGBA(frame: Int, w: Int, h: Int): ByteArray?
    /** Buat file proyek kosong. true jika tersimpan. */
    external fun nativeCreateProject(path: String, w: Int, h: Int, fps: Int, seconds: Int): Boolean
    external fun nativeRequestThumbs()
    external fun nativeThumbsReady(): Boolean
    /** RGBA 180x180 (baris pertama = atas); kosong jika tidak ada. type "none" = gambar asli. */
    external fun nativeThumb(type: String): ByteArray
    external fun nativeTrim(inFrame: Int, outFrame: Int)
    external fun nativeShift(delta: Int)
    external fun nativeMoveLayer(index: Int)
    external fun nativeScaleLayer(factor: Double)
    external fun nativeSpeedCurve()
    external fun nativeAddVideo(clip: String, w: Int, h: Int, frames: Int)
    external fun nativeAddGroup()
    external fun nativeGroupSelected()
    external fun nativeSetParent(id: Int)
    external fun nativeParentData(): String
    external fun nativeApplyPreset(preset: Int, seconds: Double)
    external fun nativePresetCatalog(): String
    external fun nativeSetMatte(mode: Int)
    external fun nativeRename(name: String)
    external fun nativeSetHidden(id: Int, hidden: Boolean)
    external fun nativeResetChannel(ch: Int, value: Float)
    external fun nativePresetExport(): String
    external fun nativePresetApply(text: String, mask: Int)
    external fun nativeMultiMode(on: Boolean)
    external fun nativeToggleMulti(id: Int)
    external fun nativeMultiCount(): Int
    external fun nativeMultiAction(op: Int)
    external fun nativeUngroup()
    external fun nativeMatteMode(): Int
    external fun nativeMatteInfo(): String
    external fun nativeAddText(image: String, w: Int, h: Int, text: String, size: Float, style: Int)
    external fun nativeSetText(image: String, w: Int, h: Int, text: String, size: Float, style: Int)
    external fun nativeTextData(): String
    external fun nativeAddAudio(clip: String, frames: Int, inFrame: Int, name: String)
    external fun nativeAudioData(): String
    external fun nativeAudioVolume(id: Int, frame: Int): Float
    external fun nativeMissingVideoFrames(frame: Int): Array<String>
    external fun nativeKeyMask(mask: Int)
    external fun nativeLayerOrder(): IntArray
    external fun nativeLayerState(): String
    external fun nativeTimeline(): String
    external fun nativeEffectCatalog(): String
    external fun nativeDiagnostics(): String
    external fun nativeRequestSelfTest()
    /** Dipanggil dari thread GL. Mengembalikan nama gambar yang perlu diunggah, atau "". */
    external fun nativeNextImageRequest(): String
    external fun nativeSetImageTexture(name: String, tex: Int, w: Int, h: Int)
}
