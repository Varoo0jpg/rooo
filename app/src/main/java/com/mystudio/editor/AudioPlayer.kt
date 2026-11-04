package com.mystudio.editor

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack

/**
 * Memutar semua layer suara bersama pemutaran timeline. Satu AudioTrack stream; suara dicampur di perangkat lunak
 * (AudioMix) dari posisi playhead engine. Bila selisih dengan playhead lebih dari ~200 ms, suara disinkronkan ulang.
 */
class AudioPlayer(ctx: Context) {
    private val app = ctx.applicationContext
    @Volatile private var running = false
    private var thread: Thread? = null

    val isRunning: Boolean get() = running

    fun start() {
        if (running) return
        running = true
        val t = Thread { loop() }
        t.isDaemon = true
        t.priority = Thread.MAX_PRIORITY - 2
        thread = t
        t.start()
    }

    fun stop() {
        running = false
        val t = thread
        thread = null
        try { t?.join(400) } catch (_: InterruptedException) {}
    }

    private fun loop() {
        val mix = AudioMix(app)
        var track: AudioTrack? = null
        try {
            val minBuf = AudioTrack.getMinBufferSize(AudioMix.HZ, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT)
            val bufBytes = maxOf(minBuf, AudioMix.HZ * 4 / 5)   // ≈ 200 ms
            track = AudioTrack.Builder()
                .setAudioAttributes(AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
                .setAudioFormat(AudioFormat.Builder()
                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .setSampleRate(AudioMix.HZ)
                    .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO).build())
                .setBufferSizeInBytes(bufBytes)
                .setTransferMode(AudioTrack.MODE_STREAM)
                .build()
            val fps = maxOf(NativeEngine.nativeFps(), 1)
            val chunk = 1024
            val pcm = ShortArray(chunk * 2)
            var srcs = mix.parse(NativeEngine.nativeAudioData())
            if (srcs.isEmpty()) return
            var lastList = System.nanoTime()
            var startT = NativeEngine.nativeFrame().toDouble() / fps    // waktu proyek pada sampel ke-0
            var written = 0L                                            // sampel yang sudah ditulis sejak startT
            var base = 0L                                               // playbackHeadPosition saat (re)start
            var lastSync = System.nanoTime()
            track.play()
            while (running && NativeEngine.nativePlaying()) {
                val now = System.nanoTime()
                if (now - lastList > 300_000_000L) {      // perbarui daftar layer (volume, potongan, layer baru)
                    srcs = mix.parse(NativeEngine.nativeAudioData())
                    lastList = now
                }
                val t0 = startT + written.toDouble() / AudioMix.HZ
                val f0 = (t0 * fps).toInt()
                for (s in srcs) s.vol = NativeEngine.nativeAudioVolume(s.id, f0)
                mix.mix(srcs, t0, chunk, fps, pcm)
                track.write(pcm, 0, chunk * 2)
                written += chunk
                // sinkronisasi: yang terdengar sekarang vs posisi playhead engine
                if (now - lastSync > 700_000_000L) {
                    lastSync = now
                    val heard = startT + (track.playbackHeadPosition.toLong() - base).coerceAtLeast(0L).toDouble() / AudioMix.HZ
                    val shown = NativeEngine.nativeFrame().toDouble() / fps
                    if (Math.abs(heard - shown) > 0.2) {
                        track.pause()
                        track.flush()
                        startT = shown
                        written = 0
                        base = track.playbackHeadPosition.toLong()
                        track.play()
                    }
                }
            }
            try { track.pause(); track.flush() } catch (_: Exception) {}
        } catch (_: Exception) {
            // gagal membuka perangkat audio: pemutaran video tetap berjalan tanpa suara
        } finally {
            try { track?.stop() } catch (_: Exception) {}
            try { track?.release() } catch (_: Exception) {}
            mix.close()
            running = false
        }
    }
}
