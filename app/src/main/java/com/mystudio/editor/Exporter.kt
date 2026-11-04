package com.mystudio.editor

import android.content.ContentValues
import android.content.Context
import android.media.Image
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.media.MediaMuxer
import android.net.Uri
import android.os.Environment
import android.provider.MediaStore
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Ekspor MP4 (H.264): engine merender tiap frame di GPU → NV12 → MediaCodec → MediaMuxer.
 * encode() HARUS dipanggil dari thread GL (lewat GLSurfaceView.queueEvent) karena memakai konteks GL engine.
 */
object Exporter {
    @Volatile var cancel = false

    class Result(val ok: Boolean, val message: String)

    private fun fillImage(img: Image, nv: ByteArray, w: Int, h: Int) {
        val planes = img.planes
        // Bidang Y
        val yp = planes[0]
        val yb = yp.buffer
        if (yp.pixelStride == 1 && yp.rowStride == w) {
            yb.position(0)
            yb.put(nv, 0, w * h)
        } else {
            for (row in 0 until h) {
                for (x in 0 until w) yb.put(row * yp.rowStride + x * yp.pixelStride, nv[row * w + x])
            }
        }
        // Bidang U dan V (NV12 sumber: U,V berselang-seling)
        val uvBase = w * h
        val up = planes[1]
        val vp = planes[2]
        val ub = up.buffer
        val vb = vp.buffer
        for (row in 0 until h / 2) {
            for (x in 0 until w / 2) {
                val src = uvBase + row * w + x * 2
                ub.put(row * up.rowStride + x * up.pixelStride, nv[src])
                vb.put(row * vp.rowStride + x * vp.pixelStride, nv[src + 1])
            }
        }
    }

    fun encode(w: Int, h: Int, fps: Int, frames: Int, bitrate: Int, outPath: String,
               prepare: (Int) -> Unit = {}, onProgress: (Int) -> Unit): Result {
        var codec: MediaCodec? = null
        var muxer: MediaMuxer? = null
        var muxStarted = false
        try {
            val fmt = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, w, h)
            fmt.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible)
            fmt.setInteger(MediaFormat.KEY_BIT_RATE, bitrate)
            fmt.setInteger(MediaFormat.KEY_FRAME_RATE, fps)
            fmt.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1)
            codec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)
            codec.configure(fmt, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
            codec.start()
            File(outPath).delete()
            muxer = MediaMuxer(outPath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4)

            val info = MediaCodec.BufferInfo()
            var track = -1
            var submitted = 0
            var inputDone = false
            var outputDone = false
            while (!outputDone) {
                if (cancel) return Result(false, "Dibatalkan")
                if (!inputDone) {
                    val idx = codec.dequeueInputBuffer(4000)
                    if (idx >= 0) {
                        val pts = submitted.toLong() * 1_000_000L / fps
                        if (submitted >= frames) {
                            codec.queueInputBuffer(idx, 0, 0, pts, MediaCodec.BUFFER_FLAG_END_OF_STREAM)
                            inputDone = true
                        } else {
                            prepare(submitted)
                            val nv = NativeEngine.nativeRenderFrame(submitted, w, h)
                                ?: return Result(false, "Gagal merender frame $submitted")
                            val img = codec.getInputImage(idx) ?: return Result(false, "Encoder tidak memberi buffer gambar")
                            fillImage(img, nv, w, h)
                            codec.queueInputBuffer(idx, 0, w * h * 3 / 2, pts, 0)
                            submitted++
                            onProgress(submitted)
                        }
                    }
                }
                while (true) {
                    val o = codec.dequeueOutputBuffer(info, if (inputDone) 4000L else 0L)
                    if (o == MediaCodec.INFO_TRY_AGAIN_LATER) break
                    if (o == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                        track = muxer.addTrack(codec.outputFormat)
                        muxer.start()
                        muxStarted = true
                    } else if (o >= 0) {
                        val buf = codec.getOutputBuffer(o)
                        if ((info.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) info.size = 0
                        if (buf != null && info.size > 0 && muxStarted) {
                            buf.position(info.offset)
                            buf.limit(info.offset + info.size)
                            muxer.writeSampleData(track, buf, info)
                        }
                        codec.releaseOutputBuffer(o, false)
                        if ((info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                            outputDone = true
                            break
                        }
                    }
                }
            }
            return Result(true, "OK")
        } catch (e: Exception) {
            return Result(false, "Gagal ekspor: $e")
        } finally {
            try { codec?.stop() } catch (_: Exception) {}
            try { codec?.release() } catch (_: Exception) {}
            try { if (muxStarted) muxer?.stop() } catch (_: Exception) {}
            try { muxer?.release() } catch (_: Exception) {}
        }
    }

    /** Salin hasil ke galeri (Movies/Studio). Mengembalikan Uri atau null. */
    fun saveToGallery(ctx: Context, file: File): Uri? {
        val name = "Studio_" + SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(Date()) + ".mp4"
        val v = ContentValues()
        v.put(MediaStore.Video.Media.DISPLAY_NAME, name)
        v.put(MediaStore.Video.Media.MIME_TYPE, "video/mp4")
        v.put(MediaStore.Video.Media.RELATIVE_PATH, Environment.DIRECTORY_MOVIES + "/Studio")
        v.put(MediaStore.Video.Media.IS_PENDING, 1)
        val uri = ctx.contentResolver.insert(MediaStore.Video.Media.EXTERNAL_CONTENT_URI, v) ?: return null
        try {
            ctx.contentResolver.openOutputStream(uri)?.use { out -> file.inputStream().use { it.copyTo(out) } }
            val done = ContentValues()
            done.put(MediaStore.Video.Media.IS_PENDING, 0)
            ctx.contentResolver.update(uri, done, null, null)
            return uri
        } catch (e: Exception) {
            ctx.contentResolver.delete(uri, null, null)
            return null
        }
    }
}
