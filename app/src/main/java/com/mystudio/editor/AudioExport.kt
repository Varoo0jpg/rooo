package com.mystudio.editor

import android.content.Context
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaExtractor
import android.media.MediaFormat
import android.media.MediaMuxer
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Tahap kedua ekspor: campur semua layer suara menjadi AAC lalu gabungkan dengan MP4 video yang sudah jadi
 * (video disalin apa adanya, tanpa dienkode ulang). Bila gagal, file video tanpa suara tetap dipakai.
 */
object AudioExport {
    class Result(val file: File, val message: String?)

    /** Apakah proyek punya layer suara. */
    fun hasAudio(): Boolean = NativeEngine.nativeAudioData().isNotBlank()

    /** frames/fps = panjang & laju video ekspor; projFps = laju proyek (satuan frame layer suara). */
    fun addAudio(ctx: Context, video: File, frames: Int, fps: Int, projFps: Int, onProgress: (Int) -> Unit): Result {
        val outFile = File(video.parentFile, "export_audio.mp4")
        outFile.delete()
        val mix = AudioMix(ctx)
        var enc: MediaCodec? = null
        var muxer: MediaMuxer? = null
        var exH: MediaExtractor? = null
        var muxStarted = false
        try {
            val srcs = mix.parse(NativeEngine.nativeAudioData())
            if (srcs.isEmpty()) return Result(video, null)

            // --- video sumber ---
            val ex = MediaExtractor()
            exH = ex
            ex.setDataSource(video.absolutePath)
            var vTrack = -1
            for (i in 0 until ex.trackCount) {
                val m = ex.getTrackFormat(i).getString(MediaFormat.KEY_MIME) ?: continue
                if (m.startsWith("video/")) { vTrack = i; break }
            }
            if (vTrack < 0) return Result(video, "Suara tidak ditambahkan: video ekspor tidak terbaca")
            val vFormat = ex.getTrackFormat(vTrack)

            // --- encoder AAC ---
            val aFormat = MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_AAC, AudioMix.HZ, 2)
            aFormat.setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC)
            aFormat.setInteger(MediaFormat.KEY_BIT_RATE, 160_000)
            aFormat.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 1 shl 16)
            val e = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_AUDIO_AAC)
            enc = e
            e.configure(aFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
            e.start()

            val mx = MediaMuxer(outFile.absolutePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4)
            muxer = mx
            val vOut = mx.addTrack(vFormat)
            var aOut = -1

            val totalSamples = Math.max(1L, frames.toLong() * AudioMix.HZ / fps)
            val chunk = 1024
            val pcm = ShortArray(chunk * 2)
            val bb = ByteBuffer.allocate(chunk * 4).order(ByteOrder.LITTLE_ENDIAN)
            var fed = 0L
            var inDone = false
            var outDone = false
            val info = MediaCodec.BufferInfo()
            var videoWritten = false
            var lastProgress = -1

            fun writeVideo() {
                val ve = ex
                ve.selectTrack(vTrack)
                val maxSize = if (vFormat.containsKey(MediaFormat.KEY_MAX_INPUT_SIZE)) vFormat.getInteger(MediaFormat.KEY_MAX_INPUT_SIZE) else 0
                val buf = ByteBuffer.allocate(maxOf(maxSize, 4 shl 20))
                val vi = MediaCodec.BufferInfo()
                while (true) {
                    buf.clear()
                    val size = ve.readSampleData(buf, 0)
                    if (size < 0) break
                    vi.set(0, size, ve.sampleTime,
                        if ((ve.sampleFlags and MediaExtractor.SAMPLE_FLAG_SYNC) != 0) MediaCodec.BUFFER_FLAG_KEY_FRAME else 0)
                    mx.writeSampleData(vOut, buf, vi)
                    ve.advance()
                }
            }

            while (!outDone) {
                if (Exporter.cancel) return Result(video, "Penggabungan suara dibatalkan")
                if (!inDone) {
                    val ii = e.dequeueInputBuffer(10_000)
                    if (ii >= 0) {
                        val ib = e.getInputBuffer(ii)!!
                        ib.clear()
                        val n = minOf(chunk.toLong(), totalSamples - fed).toInt()
                        if (n <= 0) {
                            e.queueInputBuffer(ii, 0, 0, fed * 1_000_000L / AudioMix.HZ, MediaCodec.BUFFER_FLAG_END_OF_STREAM)
                            inDone = true
                        } else {
                            val t0 = fed.toDouble() / AudioMix.HZ
                            val f0 = (t0 * projFps).toInt()
                            for (s in srcs) s.vol = NativeEngine.nativeAudioVolume(s.id, f0)
                            mix.mix(srcs, t0, n, projFps, pcm)
                            bb.clear()
                            for (k in 0 until n * 2) bb.putShort(pcm[k])
                            ib.put(bb.array(), 0, n * 4)
                            e.queueInputBuffer(ii, 0, n * 4, fed * 1_000_000L / AudioMix.HZ, 0)
                            fed += n
                            val pct = (fed * 100 / totalSamples).toInt()
                            if (pct != lastProgress) { lastProgress = pct; onProgress(pct) }
                        }
                    }
                }
                val oi = e.dequeueOutputBuffer(info, 10_000)
                if (oi >= 0) {
                    val ob = e.getOutputBuffer(oi)!!
                    if ((info.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) info.size = 0
                    if (info.size > 0 && muxStarted) {
                        ob.position(info.offset)
                        ob.limit(info.offset + info.size)
                        mx.writeSampleData(aOut, ob, info)
                    }
                    e.releaseOutputBuffer(oi, false)
                    if ((info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) outDone = true
                } else if (oi == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                    aOut = mx.addTrack(e.outputFormat)
                    mx.start()
                    muxStarted = true
                    if (!videoWritten) { writeVideo(); videoWritten = true }
                }
            }
            if (!muxStarted) return Result(video, "Suara tidak ditambahkan: encoder AAC tidak menghasilkan data")
            mx.stop()
            muxStarted = false
            video.delete()
            return Result(outFile, null)
        } catch (e: Exception) {
            outFile.delete()
            return Result(video, "Suara tidak ditambahkan ($e). Video diekspor tanpa suara.")
        } finally {
            try { enc?.stop() } catch (_: Exception) {}
            try { enc?.release() } catch (_: Exception) {}
            try { exH?.release() } catch (_: Exception) {}
            try { if (muxStarted) muxer?.stop() } catch (_: Exception) {}
            try { muxer?.release() } catch (_: Exception) {}
            mix.close()
        }
    }
}
