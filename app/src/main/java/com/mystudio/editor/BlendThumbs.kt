package com.mystudio.editor

import android.app.Activity
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Color

/**
 * Gambar contoh mode blend: foto latar + lingkaran gradien yang dicampur memakai rumus blend yang sama dengan engine.
 * Dihitung di CPU sekali (72×72 px) lalu disimpan.
 */
object BlendThumbs {
    const val S = 72
    private var base: IntArray? = null
    private val cache = HashMap<Int, Bitmap>()

    private fun loadBase(act: Activity): IntArray {
        base?.let { return it }
        val px = IntArray(S * S) { Color.rgb(90, 90, 100) }
        try {
            act.assets.open("thumb_src_1.jpg").use { ins ->
                val raw = BitmapFactory.decodeStream(ins)
                if (raw != null) {
                    val sc = Bitmap.createScaledBitmap(raw, S, S, true)
                    sc.getPixels(px, 0, S, 0, 0, S, S)
                }
            }
        } catch (_: Exception) {
        }
        base = px
        return px
    }

    fun get(act: Activity, mode: Int): Bitmap {
        cache[mode]?.let { return it }
        val bp = loadBase(act)
        val out = IntArray(S * S)
        val cb = FloatArray(3)
        val cs = FloatArray(3)
        val r = FloatArray(3)
        for (y in 0 until S) for (x in 0 until S) {
            val i = y * S + x
            val c0 = bp[i]
            cb[0] = Color.red(c0) / 255f; cb[1] = Color.green(c0) / 255f; cb[2] = Color.blue(c0) / 255f
            val dx = (x + 0.5f) / S - 0.5f
            val dy = (y + 0.5f) / S - 0.5f
            val dist = Math.sqrt((dx * dx + dy * dy).toDouble()).toFloat()
            if (dist > 0.43f) { out[i] = c0; continue }
            val t = ((dx + dy) + 0.86f) / 1.72f   // gradien diagonal pink → ungu → biru
            cs[0] = 1.0f - 0.7f * t; cs[1] = 0.35f + 0.05f * t; cs[2] = 0.55f + 0.45f * t
            blend(mode, cb, cs, r)
            // tepi lingkaran halus
            val a = ((0.43f - dist) / 0.02f).coerceIn(0f, 1f)
            val rr = cb[0] + (r[0] - cb[0]) * a
            val gg = cb[1] + (r[1] - cb[1]) * a
            val bb = cb[2] + (r[2] - cb[2]) * a
            out[i] = Color.rgb((rr.coerceIn(0f, 1f) * 255f).toInt(), (gg.coerceIn(0f, 1f) * 255f).toInt(),
                (bb.coerceIn(0f, 1f) * 255f).toInt())
        }
        val bmp = Bitmap.createBitmap(out, S, S, Bitmap.Config.ARGB_8888)
        cache[mode] = bmp
        return bmp
    }

    private fun lum(c: FloatArray) = c[0] * 0.3f + c[1] * 0.59f + c[2] * 0.11f
    private fun sat(c: FloatArray) = maxOf(c[0], c[1], c[2]) - minOf(c[0], c[1], c[2])

    private fun clip(c: FloatArray) {
        val l = lum(c)
        val n = minOf(c[0], c[1], c[2])
        val x = maxOf(c[0], c[1], c[2])
        if (n < 0f) for (k in 0..2) c[k] = l + (c[k] - l) * l / maxOf(l - n, 1e-5f)
        if (x > 1f) for (k in 0..2) c[k] = l + (c[k] - l) * (1f - l) / maxOf(x - l, 1e-5f)
    }

    private fun setLum(c: FloatArray, l: Float, out: FloatArray) {
        val d = l - lum(c)
        for (k in 0..2) out[k] = c[k] + d
        clip(out)
    }

    private fun setSat(c: FloatArray, s: Float, out: FloatArray) {
        val mn = minOf(c[0], c[1], c[2])
        val mx = maxOf(c[0], c[1], c[2])
        if (mx <= mn) { out[0] = 0f; out[1] = 0f; out[2] = 0f; return }
        for (k in 0..2) out[k] = (c[k] - mn) * s / (mx - mn)
    }

    private fun soft(b: Float, s: Float): Float {
        if (s <= 0.5f) return b - (1f - 2f * s) * b * (1f - b)
        val d = if (b <= 0.25f) ((16f * b - 12f) * b + 4f) * b else Math.sqrt(b.toDouble()).toFloat()
        return b + (2f * s - 1f) * (d - b)
    }

    private fun hard(b: Float, s: Float) = if (s < 0.5f) b * 2f * s else 1f - 2f * (1f - b) * (1f - s)

    private fun vivid(b: Float, s: Float): Float {
        if (s <= 0.5f) { val s2 = 2f * s; return if (s2 <= 0f) 0f else 1f - minOf((1f - b) / s2, 1f) }
        val s2 = 2f * (s - 0.5f)
        return if (s2 >= 1f) 1f else minOf(b / (1f - s2), 1f)
    }

    private fun pin(b: Float, s: Float) = if (s < 0.5f) minOf(b, 2f * s) else maxOf(b, 2f * (s - 0.5f))

    private fun blend(m: Int, b: FloatArray, s: FloatArray, o: FloatArray) {
        val tmp = FloatArray(3)
        when (m) {
            22 -> { setSat(s, sat(b), tmp); setLum(tmp, lum(b), o); return }       // Hue
            23 -> { setSat(b, sat(s), tmp); setLum(tmp, lum(b), o); return }       // Saturation
            24 -> { setLum(s, lum(b), o); return }                                  // Color
            25 -> { setLum(b, lum(s), o); return }                                  // Luminosity
            14 -> { val c = if (lum(s) < lum(b)) s else b; for (k in 0..2) o[k] = c[k]; return }
            15 -> { val c = if (lum(s) > lum(b)) s else b; for (k in 0..2) o[k] = c[k]; return }
        }
        for (k in 0..2) {
            val cb = b[k]
            val cs = s[k]
            o[k] = when (m) {
                0 -> cs
                1 -> minOf(cb + cs, 1f)
                2 -> cb * cs
                3 -> cb + cs - cb * cs
                4 -> hard(cs, cb)
                5 -> minOf(cb, cs)
                6 -> maxOf(cb, cs)
                7 -> if (cb <= 0f) 0f else minOf(cb / maxOf(1f - cs, 1e-4f), 1f)
                8 -> if (cb >= 1f) 1f else 1f - minOf((1f - cb) / maxOf(cs, 1e-4f), 1f)
                9 -> hard(cb, cs)
                10 -> soft(cb, cs)
                11 -> Math.abs(cb - cs)
                12 -> cb + cs - 2f * cb * cs
                13 -> maxOf(cb + cs - 1f, 0f)
                16 -> vivid(cb, cs)
                17 -> (cb + 2f * cs - 1f).coerceIn(0f, 1f)
                18 -> pin(cb, cs)
                19 -> if (cb + cs >= 1f) 1f else 0f
                20 -> maxOf(cb - cs, 0f)
                21 -> minOf(cb / maxOf(cs, 1e-4f), 1f)
                else -> cs
            }
        }
    }
}
