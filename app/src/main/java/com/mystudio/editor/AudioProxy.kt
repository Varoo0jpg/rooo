package com.mystudio.editor

import android.content.Context
import android.media.MediaCodec
import android.media.MediaExtractor
import android.media.MediaFormat
import android.net.Uri
import java.io.BufferedOutputStream
import java.io.File
import java.io.FileOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Mengambil jalur suara dari berkas audio/video menjadi PCM 16-bit di media/<klip>/audio.pcm (+ audio.txt).
 * Dekode suara jauh lebih cepat dari waktu nyata. Panggil dari thread latar.
 */
object AudioProxy {
    class Info(val clip: String, val frames: Int)

    const val MAX_SECONDS = 600
    @Volatile var noAudio = false     // true bila sumber tidak punya jalur suara (bukan kesalahan)

    fun extract(ctx0: Context, uri: Uri, fps: Int, error: (String) -> Unit): Info? {
        val ctx = ctx0.applicationContext
        noAudio = false
        val ex = MediaExtractor()
        var codec: MediaCodec? = null
        val clip = "aud_" + java.lang.Long.toHexString(System.currentTimeMillis()) +
            java.lang.Integer.toHexString((Math.random() * 65535).toInt())
        val dir = File(File(ctx.filesDir, "media"), clip)
        var ok = false
        try {
            ex.setDataSource(ctx, uri, null)
            var track = -1
            for (i in 0 until ex.trackCount) {
                val mime = ex.getTrackFormat(i).getString(MediaFormat.KEY_MIME) ?: continue
                if (mime.startsWith("audio/")) { track = i; break }
            }
            if (track < 0) { noAudio = true; return null }
            val fmt0 = ex.getTrackFormat(track)
            val mime = fmt0.getString(MediaFormat.KEY_MIME) ?: return null
            if (fmt0.containsKey(MediaFormat.KEY_DURATION) && fmt0.getLong(MediaFormat.KEY_DURATION) > MAX_SECONDS * 1_000_000L) {
                error("Suara terlalu panjang (maks. 10 menit)")
                return null
            }
            ex.selectTrack(track)
            if (!dir.mkdirs()) { error("Gagal membuat folder media"); return null }
            val dec = MediaCodec.createDecoderByType(mime)
            codec = dec
            dec.configure(fmt0, null, null, 0)
            dec.start()

            var hz = if (fmt0.containsKey(MediaFormat.KEY_SAMPLE_RATE)) fmt0.getInteger(MediaFormat.KEY_SAMPLE_RATE) else 44100
            var ch = if (fmt0.containsKey(MediaFormat.KEY_CHANNEL_COUNT)) fmt0.getInteger(MediaFormat.KEY_CHANNEL_COUNT) else 2
            var float = false
            var totalBytes = 0L
            val maxBytes = 1_200_000_000L
            val info = MediaCodec.BufferInfo()
            var inDone = false
            var outDone = false
            BufferedOutputStream(FileOutputStream(File(dir, "audio.pcm")), 1 shl 16).use { os ->
                var chunk = ByteArray(1 shl 16)
                while (!outDone) {
                    if (!inDone) {
                        val ii = dec.dequeueInputBuffer(10_000)
                        if (ii >= 0) {
                            val ib = dec.getInputBuffer(ii)!!
                            val size = ex.readSampleData(ib, 0)
                            if (size < 0) {
                                dec.queueInputBuffer(ii, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM)
                                inDone = true
                            } else {
                                dec.queueInputBuffer(ii, 0, size, ex.sampleTime, 0)
                                ex.advance()
                            }
                        }
                    }
                    val oi = dec.dequeueOutputBuffer(info, 10_000)
                    if (oi >= 0) {
                        if (info.size > 0) {
                            val ob = dec.getOutputBuffer(oi)!!
                            ob.position(info.offset)
                            ob.limit(info.offset + info.size)
                            if (float) {
                                val fb = ob.order(ByteOrder.nativeOrder()).asFloatBuffer()
                                val n = fb.remaining()
                                val outBytes = ByteBuffer.allocate(n * 2).order(ByteOrder.LITTLE_ENDIAN)
                                for (k in 0 until n) {
                                    val v = (fb.get() * 32767f).coerceIn(-32768f, 32767f)
                                    outBytes.putShort(v.toInt().toShort())
                                }
                                os.write(outBytes.array(), 0, n * 2)
                                totalBytes += n * 2
                            } else {
                                if (chunk.size < info.size) chunk = ByteArray(info.size)
                                ob.get(chunk, 0, info.size)
                                os.write(chunk, 0, info.size)
                                totalBytes += info.size
                            }
                        }
                        dec.releaseOutputBuffer(oi, false)
                        if ((info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) outDone = true
                        if (totalBytes > maxBytes) throw IllegalStateException("suara terlalu panjang")
                    } else if (oi == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                        val nf = dec.outputFormat
                        if (nf.containsKey(MediaFormat.KEY_SAMPLE_RATE)) hz = nf.getInteger(MediaFormat.KEY_SAMPLE_RATE)
                        if (nf.containsKey(MediaFormat.KEY_CHANNEL_COUNT)) ch = nf.getInteger(MediaFormat.KEY_CHANNEL_COUNT)
                        float = nf.containsKey(MediaFormat.KEY_PCM_ENCODING) && nf.getInteger(MediaFormat.KEY_PCM_ENCODING) == 4
                    }
                }
            }
            val samples = totalBytes / (2L * maxOf(ch, 1))
            if (samples < 1) { error("Suara kosong atau tidak bisa didekode"); return null }
            File(dir, "audio.txt").writeText("$hz\n$ch\n$samples\n")
            val frames = Math.max(1, Math.ceil(samples.toDouble() * fps / hz).toInt())
            ok = true
            return Info(clip, frames)
        } catch (e: Exception) {
            error("Suara tidak bisa diimpor: $e")
            return null
        } finally {
            try { codec?.stop() } catch (_: Exception) {}
            try { codec?.release() } catch (_: Exception) {}
            try { ex.release() } catch (_: Exception) {}
            if (!ok) dir.deleteRecursively()
        }
    }
}
