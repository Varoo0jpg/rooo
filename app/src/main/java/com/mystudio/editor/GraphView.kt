package com.mystudio.editor

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.view.MotionEvent
import android.view.View
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/**
 * Graph editor satu properti: nilai (sumbu Y) terhadap frame (sumbu X).
 * Kurva Bezier digambar dengan cubicTo memakai titik kontrol yang sama persis dengan engine:
 *   P0=(f0,v0)  P1=(f0+outX*dt, v0+outY)  P2=(f1-inX*dt, v1+inY)  P3=(f1,v1)
 * Menyeret titik/handle hanya mengubah salinan lokal; perubahan dikirim ke engine saat jari diangkat
 * (satu langkah undo), lalu tampilan disegarkan dari data engine.
 */
class GraphView(context: Context) : View(context) {

    private class Key(var frame: Float, var value: Float, var interp: Int,
                      var outX: Float, var outY: Float, var inX: Float, var inY: Float)

    private val keys = ArrayList<Key>()
    private var hasLayer = false
    private var playhead = 0f
    private var curValue = 0f
    private var duration = 300f
    private var propIndex = 0

    private var selFrame = Int.MIN_VALUE   // keyframe terpilih (frame aslinya di engine)

    // Rentang tampilan; dibekukan selama menyeret supaya grafik tidak melompat.
    private var yMin = 0f
    private var yMax = 1f
    private var frozen = false

    // Jendela tampilan X (frame). Pinch = zoom, dua jari = geser. manualView = tidak otomatis fit.
    private var vx0 = 0f
    private var vx1 = 300f
    private var manualView = false
    /** Tangen mulus: menyeret satu pegangan ikut memutar pegangan di sisi lain (seperti Bezier mulus di After Effects). */
    var smooth = true

    private enum class Mode { None, Scrub, Key, HandleOut, HandleIn, Pinch }
    private var mode = Mode.None
    private var dragKeyIndex = -1
    private var dragOrigFrame = 0

    private val d = resources.displayMetrics.density
    private val padL = 44f * d
    private val padR = 12f * d
    private val padT = 14f * d
    private val padB = 14f * d
    private val hitR = 26f * d

    private val pGrid = Paint().apply { color = Color.argb(60, 255, 255, 255); strokeWidth = 1f }
    private val pCurve = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Ui.ACCENT; style = Paint.Style.STROKE; strokeWidth = 2.5f * d
    }
    private val pKey = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.WHITE; style = Paint.Style.FILL }
    private val pKeySel = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.rgb(255, 210, 80); style = Paint.Style.FILL }
    private val pHandleLine = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(255, 210, 80); strokeWidth = 1.5f * d
    }
    private val pHandle = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.rgb(255, 210, 80); style = Paint.Style.FILL }
    private val pHead = Paint().apply { color = Color.rgb(255, 90, 90); strokeWidth = 2f * d }
    private val pText = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.LTGRAY; textSize = 10f * d }
    private val pDot = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.rgb(255, 90, 90); style = Paint.Style.FILL }

    init { setBackgroundColor(Ui.SURFACE) }

    val selectedKeyFrame: Int get() = selFrame

    fun setProp(i: Int) { propIndex = i; selFrame = Int.MIN_VALUE; invalidate() }

    /** Dipanggil berkala dari data engine. Diabaikan saat jari sedang menyeret. */
    fun setData(a: FloatArray) {
        if (mode == Mode.Key || mode == Mode.HandleIn || mode == Mode.HandleOut || mode == Mode.Pinch) return
        if (a.size < 4) return
        val n = a[0].toInt()
        hasLayer = n >= 0
        playhead = a[1]; curValue = a[2]
        val nd = max(a[3], 1f)
        if (nd != duration || !manualView) { duration = nd; if (!manualView) { vx0 = 0f; vx1 = duration } }
        duration = nd
        keys.clear()
        if (hasLayer) for (i in 0 until n) {
            val o = 4 + i * 7
            if (o + 6 >= a.size) break
            keys.add(Key(a[o], a[o + 1], a[o + 2].toInt(), a[o + 3], a[o + 4], a[o + 5], a[o + 6]))
        }
        if (selFrame != Int.MIN_VALUE && keys.none { it.frame.roundToInt() == selFrame }) selFrame = Int.MIN_VALUE
        if (mode != Mode.Scrub && !manualView) recomputeRange()
        invalidate()
    }

    private fun recomputeRange() {
        var lo = curValue; var hi = curValue
        for (i in keys.indices) {
            val k = keys[i]
            lo = min(lo, k.value); hi = max(hi, k.value)
            if (i + 1 < keys.size) {   // titik kontrol segmen Bezier ikut menentukan rentang
                val k1 = keys[i + 1]
                if (k.interp == 2) {
                    lo = min(lo, min(k.value + k.outY, k1.value + k1.inY))
                    hi = max(hi, max(k.value + k.outY, k1.value + k1.inY))
                }
            }
        }
        if (propIndex == 6) { lo = min(lo, 0f); hi = max(hi, 1f) }  // opasitas: selalu tampilkan 0..1
        if (hi - lo < 1e-3f) { lo -= 1f; hi += 1f }
        val m = (hi - lo) * 0.15f
        yMin = lo - m; yMax = hi + m
    }

    fun fit() { manualView = false; vx0 = 0f; vx1 = duration; recomputeRange(); invalidate() }

    /** Lompat playhead ke keyframe sebelum (-1) / sesudah (+1). */
    fun jumpKey(dir: Int) {
        val cur = playhead.roundToInt()
        val t = if (dir > 0) keys.firstOrNull { it.frame.roundToInt() > cur } else keys.lastOrNull { it.frame.roundToInt() < cur }
        if (t != null) { selFrame = t.frame.roundToInt(); NativeEngine.nativeSeek(selFrame); invalidate() }
    }

    private fun xOf(f: Float) = padL + ((f - vx0) / (vx1 - vx0)) * (width - padL - padR)
    private fun yOf(v: Float) = padT + (1f - (v - yMin) / (yMax - yMin)) * (height - padT - padB)
    private fun fOf(x: Float) = vx0 + (x - padL) / (width - padL - padR) * (vx1 - vx0)
    private fun vOf(y: Float) = yMin + (1f - (y - padT) / (height - padT - padB)) * (yMax - yMin)

    // Posisi handle di layar untuk keyframe i (null jika tidak berlaku)
    // Segmen Lurus punya pegangan "virtual" (setara garis lurus) yang bisa langsung diseret → otomatis jadi Bezier.
    private fun effOutX(i: Int) = if (keys[i].interp == 2) keys[i].outX else 1f / 3f
    private fun effOutY(i: Int) = if (keys[i].interp == 2) keys[i].outY else (keys[i + 1].value - keys[i].value) / 3f
    private fun effInX(i: Int) = if (keys[i - 1].interp == 2) keys[i].inX else 1f / 3f
    private fun effInY(i: Int) = if (keys[i - 1].interp == 2) keys[i].inY else -(keys[i].value - keys[i - 1].value) / 3f

    private fun outHandlePos(i: Int): FloatArray? {
        if (i + 1 >= keys.size || keys[i].interp == 0) return null
        val k = keys[i]; val dt = keys[i + 1].frame - k.frame
        return floatArrayOf(xOf(k.frame + effOutX(i) * dt), yOf(k.value + effOutY(i)))
    }
    private fun inHandlePos(i: Int): FloatArray? {
        if (i == 0 || keys[i - 1].interp == 0) return null
        val k = keys[i]; val dt = k.frame - keys[i - 1].frame
        return floatArrayOf(xOf(k.frame - effInX(i) * dt), yOf(k.value + effInY(i)))
    }

    override fun onDraw(c: Canvas) {
        if (!hasLayer) {
            c.drawText("Ketuk sebuah layer untuk melihat grafiknya", padL, height / 2f, pText)
            return
        }
        // Grid: 5 garis horizontal dengan label nilai
        for (g in 0..4) {
            val v = yMin + (yMax - yMin) * g / 4f
            val y = yOf(v)
            c.drawLine(padL, y, width - padR, y, pGrid)
            c.drawText(fmt(v), 2f * d, y + 4f * d, pText)
        }
        c.save()
        c.clipRect(padL, 0f, width - padR, height.toFloat())
        // Kurva
        val path = Path()
        if (keys.isEmpty()) {
            path.moveTo(padL, yOf(curValue)); path.lineTo(width - padR, yOf(curValue))
        } else {
            path.moveTo(xOf(vx0 - 1000f), yOf(keys[0].value))
            path.lineTo(xOf(keys[0].frame), yOf(keys[0].value))
            for (i in 0 until keys.size - 1) {
                val a = keys[i]; val b = keys[i + 1]
                when (a.interp) {
                    0 -> { path.lineTo(xOf(b.frame), yOf(a.value)); path.lineTo(xOf(b.frame), yOf(b.value)) }
                    1 -> path.lineTo(xOf(b.frame), yOf(b.value))
                    else -> {
                        val dt = b.frame - a.frame
                        path.cubicTo(
                            xOf(a.frame + a.outX * dt), yOf(a.value + a.outY),
                            xOf(b.frame - b.inX * dt), yOf(b.value + b.inY),
                            xOf(b.frame), yOf(b.value))
                    }
                }
            }
            val last = keys.last()
            path.lineTo(xOf(vx1 + 1000f), yOf(last.value))
        }
        c.drawPath(path, pCurve)

        // Playhead + nilai sekarang
        c.drawLine(xOf(playhead), padT, xOf(playhead), height - padB, pHead)
        c.drawCircle(xOf(playhead), yOf(curValue), 4f * d, pDot)
        c.drawText("f${playhead.roundToInt()}  ${fmt(curValue)}", padL + 4f * d, padT + 10f * d, pText)

        // Handle keyframe terpilih
        val si = keys.indexOfFirst { it.frame.roundToInt() == selFrame }
        if (si >= 0) {
            val k = keys[si]
            outHandlePos(si)?.let {
                c.drawLine(xOf(k.frame), yOf(k.value), it[0], it[1], pHandleLine)
                c.drawCircle(it[0], it[1], 7f * d, pHandle)
            }
            inHandlePos(si)?.let {
                c.drawLine(xOf(k.frame), yOf(k.value), it[0], it[1], pHandleLine)
                c.drawCircle(it[0], it[1], 7f * d, pHandle)
            }
        }
        // Keyframe (berlian)
        for ((i, k) in keys.withIndex()) {
            val x = xOf(k.frame); val y = yOf(k.value); val r = 8f * d
            val p = Path().apply { moveTo(x, y - r); lineTo(x + r, y); lineTo(x, y + r); lineTo(x - r, y); close() }
            c.drawPath(p, if (i == si) pKeySel else pKey)
        }
        c.restore()
    }

    private fun fmt(v: Float) = if (abs(v) >= 100f) v.roundToInt().toString() else String.format("%.2f", v)

    private var liveFrame = 0
    private var gestureOpen = false
    private var lastLive = 0L

    /** Kirim keadaan seretan ke engine SEKARANG (kanvas ikut bergerak mengikuti jari). */
    private fun pushLive() {
        val now = android.os.SystemClock.uptimeMillis()
        if (now - lastLive < 12) return   // ±80 Hz cukup; hindari membanjiri engine
        lastLive = now
        when (mode) {
            Mode.Key -> {
                val k = keys[dragKeyIndex]
                val nf = k.frame.roundToInt()
                NativeEngine.nativeKeyEdit(0, liveFrame, nf, k.value, 0f, 0f, 0f)
                liveFrame = nf
            }
            Mode.HandleOut -> pushHandles(dragKeyIndex, true)
            Mode.HandleIn -> pushHandles(dragKeyIndex, false)
            else -> {}
        }
    }

    private fun pushHandles(i: Int, outSide: Boolean) {
        val k = keys[i]
        if (i + 1 < keys.size && (outSide || (smooth && k.interp == 2))) {
            val n = keys[i + 1]
            NativeEngine.nativeKeyEdit(1, k.frame.roundToInt(), 0, k.outX, k.outY, n.inX, n.inY)
        }
        if (i > 0 && (!outSide || (smooth && keys[i - 1].interp == 2))) {
            val p = keys[i - 1]
            NativeEngine.nativeKeyEdit(1, p.frame.roundToInt(), 0, p.outX, p.outY, k.inX, k.inY)
        }
    }

    private var lastFx = 0f
    private var lastFy = 0f
    private var lastSpan = 0f
    private var lastSpanY = 0f

    private fun frameClamp(f: Float) = f.roundToInt().coerceIn(0, duration.toInt() - 1)

    // Pegangan keluar milik key i diseret ke (x,y) layar; sisi masuk ikut berputar jika `smooth`.
    private fun dragOut(i: Int, x: Float, y: Float) {
        val k = keys[i]; val dt = keys[i + 1].frame - k.frame
        val ox = ((fOf(x) - k.frame) / dt).coerceIn(0.02f, 1f)
        val oy = vOf(y) - k.value
        k.interp = 2; k.outX = ox; k.outY = oy
        if (smooth && i > 0 && keys[i - 1].interp != 0) {
            val slope = oy / (ox * dt)
            val dtIn = k.frame - keys[i - 1].frame
            val ix = effInX(i)
            val pv = keys[i - 1]
            if (pv.interp != 2) { pv.outX = 1f / 3f; pv.outY = (k.value - pv.value) / 3f; pv.interp = 2 }
            k.inX = ix; k.inY = -slope * ix * dtIn
        }
    }

    private fun dragIn(i: Int, x: Float, y: Float) {
        val k = keys[i]; val dt = k.frame - keys[i - 1].frame
        val ix = ((k.frame - fOf(x)) / dt).coerceIn(0.02f, 1f)
        val iy = vOf(y) - k.value
        val p = keys[i - 1]
        // segmen masuk harus Bezier: bawa pegangan keluar p ke nilai efektifnya sebelum diubah
        if (p.interp != 2) { p.outX = 1f / 3f; p.outY = (k.value - p.value) / 3f; p.interp = 2 }
        k.inX = ix; k.inY = iy
        if (smooth && i + 1 < keys.size && k.interp != 0) {
            val slope = -iy / (ix * dt)
            val dtOut = keys[i + 1].frame - k.frame
            val ox = effOutX(i)
            if (k.interp != 2) { k.outX = ox; k.outY = effOutY(i); k.interp = 2; keys[i + 1].inX = 1f / 3f; keys[i + 1].inY = -(keys[i + 1].value - k.value) / 3f }
            k.outX = ox; k.outY = slope * ox * dtOut
        }
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        if (!hasLayer) return true
        // Dua jari: zoom (jarak) + geser (titik tengah), seperti graph editor desktop.
        if (e.pointerCount >= 2) {
            val fx = (e.getX(0) + e.getX(1)) / 2f
            val fy = (e.getY(0) + e.getY(1)) / 2f
            val sx = abs(e.getX(0) - e.getX(1)).coerceAtLeast(40f * d)
            val sy = abs(e.getY(0) - e.getY(1)).coerceAtLeast(40f * d)
            if (mode != Mode.Pinch) {
                if (gestureOpen) { NativeEngine.nativeGesture(false); gestureOpen = false }
                mode = Mode.Pinch; manualView = true
                parent?.requestDisallowInterceptTouchEvent(true)
            } else {
                val pw = width - padL - padR; val ph = height - padT - padB
                // zoom X
                val zx = lastSpan / sx
                val anchorF = vx0 + (fx - padL) / pw * (vx1 - vx0)
                var w = ((vx1 - vx0) * zx).coerceIn(5f, duration * 1.5f)
                var x0 = anchorF - (fx - padL) / pw * w
                // geser X mengikuti jari
                x0 -= (fx - lastFx) / pw * w
                vx0 = x0; vx1 = x0 + w
                // zoom Y
                val zy = lastSpanY / sy
                val anchorV = vOf(fy)
                val h = ((yMax - yMin) * zy).coerceAtLeast(1e-3f)
                val ny0 = anchorV - (1f - (fy - padT) / ph) * h
                val shift = (fy - lastFy) / ph * h
                yMin = ny0 + shift; yMax = yMin + h
            }
            lastFx = fx; lastFy = fy; lastSpan = sx; lastSpanY = sy
            invalidate()
            return true
        }
        if (mode == Mode.Pinch) {
            if (e.actionMasked == MotionEvent.ACTION_UP || e.actionMasked == MotionEvent.ACTION_CANCEL) mode = Mode.None
            else if (e.actionMasked == MotionEvent.ACTION_POINTER_UP) { /* tunggu jari terakhir diangkat */ }
            return true
        }
        val x = e.x; val y = e.y
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                parent?.requestDisallowInterceptTouchEvent(true)
                val si = keys.indexOfFirst { it.frame.roundToInt() == selFrame }
                mode = Mode.None
                if (si >= 0) {
                    outHandlePos(si)?.let { if (dist(x, y, it[0], it[1]) <= hitR) { mode = Mode.HandleOut; dragKeyIndex = si } }
                    if (mode == Mode.None)
                        inHandlePos(si)?.let { if (dist(x, y, it[0], it[1]) <= hitR) { mode = Mode.HandleIn; dragKeyIndex = si } }
                }
                if (mode == Mode.None) {
                    var best = -1; var bd = hitR
                    for ((i, k) in keys.withIndex()) {
                        val dd = dist(x, y, xOf(k.frame), yOf(k.value))
                        if (dd < bd) { bd = dd; best = i }
                    }
                    if (best >= 0) {
                        mode = Mode.Key; dragKeyIndex = best
                        dragOrigFrame = keys[best].frame.roundToInt()
                        selFrame = dragOrigFrame
                    }
                }
                if (mode == Mode.None) {
                    mode = Mode.Scrub
                    NativeEngine.nativeSeek(frameClamp(fOf(x)))
                } else {
                    if (!manualView) recomputeRange()   // bekukan rentang selama seretan
                    liveFrame = dragOrigFrame
                    NativeEngine.nativeGesture(true)    // seluruh seretan = satu langkah undo
                    gestureOpen = true
                }
                invalidate()
            }
            MotionEvent.ACTION_MOVE -> {
                when (mode) {
                    Mode.Scrub -> NativeEngine.nativeSeek(frameClamp(fOf(x)))
                    Mode.Key -> {
                        val k = keys[dragKeyIndex]
                        val lo = if (dragKeyIndex > 0) keys[dragKeyIndex - 1].frame + 1f else 0f
                        val hi = if (dragKeyIndex + 1 < keys.size) keys[dragKeyIndex + 1].frame - 1f else duration - 1f
                        k.frame = frameClamp(fOf(x)).toFloat().coerceIn(lo, max(lo, hi))   // tidak boleh menimpa tetangga
                        var v = vOf(y)
                        if (propIndex == 6) v = v.coerceIn(0f, 1f)
                        k.value = v
                    }
                    Mode.HandleOut -> dragOut(dragKeyIndex, x, y)
                    Mode.HandleIn -> dragIn(dragKeyIndex, x, y)
                    else -> {}
                }
                pushLive()
                invalidate()
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                val up = e.actionMasked == MotionEvent.ACTION_UP
                lastLive = 0L
                if (up) pushLive()   // posisi akhir persis di titik jari diangkat
                if (mode == Mode.Key && up) selFrame = keys[dragKeyIndex].frame.roundToInt()
                if (gestureOpen) { NativeEngine.nativeGesture(false); gestureOpen = false }
                mode = Mode.None
                dragKeyIndex = -1
                setData(NativeEngine.nativeGraphData())
            }
        }
        return true
    }

    private fun dist(x1: Float, y1: Float, x2: Float, y2: Float): Float {
        val dx = x1 - x2; val dy = y1 - y2
        return Math.sqrt((dx * dx + dy * dy).toDouble()).toFloat()
    }
}
