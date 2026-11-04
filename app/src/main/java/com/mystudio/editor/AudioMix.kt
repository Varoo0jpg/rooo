package com.mystudio.editor

import android.content.Context
import java.io.File
import java.io.RandomAccessFile
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Pencampur suara perangkat lunak (dipakai pemutar dan ekspor): menjumlahkan semua layer suara pada rentang waktu
 * tertentu menjadi PCM 16-bit stereo 44,1 kHz. Tiap klip disimpan sebagai media/<klip>/audio.pcm (16-bit little-endian
 * berselang-seling) + audio.txt (laju sampel, jumlah kanal, jumlah sampel).
 */
class AudioMix(ctx: Context) {
    companion object {
        const val HZ = 44100
    }

    class Src(val id: Int, val clip: String, val inF: Int, val outF: Int, val srcStart: Long, val rate: Double, var vol: Float)

    private class Meta(val hz: Int, val ch: Int, val samples: Long)

    private val dir = File(ctx.filesDir, "media")
    private val metas = HashMap<String, Meta?>()
    private val files = HashMap<String, RandomAccessFile>()

    /** Parse keluaran NativeEngine.nativeAudioData(). */
    fun parse(s: String): List<Src> {
        val out = ArrayList<Src>()
        for (line in s.split("\n")) {
            val p = line.split("|")
            if (p.size < 7) continue
            val id = p[0].toIntOrNull() ?: continue
            out.add(Src(id, p[1], p[2].toIntOrNull() ?: continue, p[3].toIntOrNull() ?: continue,
                p[4].toLongOrNull() ?: 0L, p[5].toDoubleOrNull() ?: 1.0, p[6].toFloatOrNull() ?: 1f))
        }
        return out
    }

    private fun meta(clip: String): Meta? {
        if (metas.containsKey(clip)) return metas[clip]
        var m: Meta? = null
        try {
            val l = File(File(dir, clip), "audio.txt").readLines()
            if (l.size >= 3) m = Meta(l[0].trim().toInt(), l[1].trim().toInt(), l[2].trim().toLong())
            if (m != null && (m.hz < 4000 || m.hz > 192000 || m.ch < 1 || m.samples < 1)) m = null
        } catch (_: Exception) {
        }
        metas[clip] = m
        return m
    }

    private fun file(clip: String): RandomAccessFile? {
        files[clip]?.let { return it }
        return try {
            val f = RandomAccessFile(File(File(dir, clip), "audio.pcm"), "r")
            files[clip] = f
            f
        } catch (_: Exception) {
            null
        }
    }

    /** Baca `count` sampel (per kanal) mulai dari `first`; bagian di luar klip diisi nol. Hasil berselang-seling. */
    private fun read(clip: String, m: Meta, first: Long, count: Int): ShortArray {
        val out = ShortArray(count * m.ch)
        val f = file(clip) ?: return out
        val a = maxOf(first, 0L)
        val b = minOf(first + count, m.samples)
        if (b <= a) return out
        val n = (b - a).toInt()
        val bytes = ByteArray(n * m.ch * 2)
        try {
            f.seek(a * m.ch * 2L)
            f.readFully(bytes)
        } catch (_: Exception) {
            return out
        }
        val sb = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).asShortBuffer()
        sb.get(out, (a - first).toInt() * m.ch, n * m.ch)
        return out
    }

    /**
     * Campur `n` sampel stereo mulai dari waktu proyek t0 (detik) ke `out` (panjang n*2). `fps` = frame per detik proyek.
     * Volume tiap layer diambil dari Src.vol (perbarui sebelum memanggil bila dikeyframe).
     */
    fun mix(srcs: List<Src>, t0: Double, n: Int, fps: Int, out: ShortArray) {
        val acc = FloatArray(n * 2)
        var any = false
        val tEnd = t0 + n.toDouble() / HZ
        for (s in srcs) {
            if (s.vol <= 0.0005f) continue
            if (tEnd <= s.inF.toDouble() / fps || t0 >= s.outF.toDouble() / fps) continue
            val m = meta(s.clip) ?: continue
            val tsStart = ((t0 * fps - s.inF) * s.rate + s.srcStart) / fps    // detik di dalam klip
            val step = s.rate * m.hz / HZ                                      // sampel sumber per sampel keluaran
            val pos0 = tsStart * m.hz
            val first = Math.floor(pos0).toLong()
            val last = Math.floor(pos0 + (n - 1) * step).toLong() + 1
            val cnt = (last - first + 1).toInt()
            if (cnt <= 0 || cnt > 400000) continue
            val buf = read(s.clip, m, first, cnt)
            any = true
            val ch = m.ch
            for (i in 0 until n) {
                val fr = (t0 + i.toDouble() / HZ) * fps
                if (fr < s.inF || fr >= s.outF) continue
                val p = pos0 + i * step
                val idx = Math.floor(p).toLong()
                val fracF = (p - idx).toFloat()
                val a = (idx - first).toInt()
                if (a < 0 || a >= cnt) continue
                val a1 = minOf(a + 1, cnt - 1)
                val l0 = buf[a * ch].toFloat()
                val l1 = buf[a1 * ch].toFloat()
                val l = l0 + (l1 - l0) * fracF
                var r = l
                if (ch > 1) {
                    val r0 = buf[a * ch + 1].toFloat()
                    val r1 = buf[a1 * ch + 1].toFloat()
                    r = r0 + (r1 - r0) * fracF
                }
                acc[i * 2] += l * s.vol
                acc[i * 2 + 1] += r * s.vol
            }
        }
        for (i in 0 until n * 2) {
            val v = if (any) acc[i] else 0f
            out[i] = (if (v > 32767f) 32767f else if (v < -32768f) -32768f else v).toInt().toShort()
        }
    }

    fun close() {
        for (f in files.values) try { f.close() } catch (_: Exception) {}
        files.clear()
        metas.clear()
    }
}
