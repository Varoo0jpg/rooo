package com.mystudio.editor

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Typeface
import android.graphics.RectF
import android.view.MotionEvent
import android.view.VelocityTracker
import android.view.View
import android.view.ViewConfiguration
import android.widget.OverScroller

/**
 * Timeline satu-view dengan scroll sendiri:
 *  - Penggaris waktu MENEMPEL di atas (tidak ikut tergulir ke bawah).
 *  - Geser kiri/kanan/atas/bawah dengan satu jari di area kosong (ada fling); cubit dua jari = zoom waktu.
 *  - Garis playhead (merah) bisa diraih di sepanjang garisnya, bukan hanya segitiga di atas.
 *  - Berlian emas di bar = keyframe; layer terpilih punya lane keyframe kanal yang sedang diedit (bisa digeser).
 *  - Ketuk bar = pilih layer; seret badan = geser waktu; seret ujung = potong durasi (boleh melewati ujung: durasi
 *    proyek ikut memanjang).
 */
class TimelineView(ctx: Context) : View(ctx) {
    private class Row(val id: Int, val name: String, val inF: Int, val outF: Int, val type: Int, val sel: Boolean,
                      val color: Int, val keys: IntArray, val chKeys: IntArray, val msel: Boolean = false, val hidden: Boolean = false)

    private var rows = ArrayList<Row>()
    private var tops = IntArray(0)          // posisi atas tiap baris di koordinat konten (di bawah penggaris)
    private var contentH = 0
    private var playhead = 0
    private var duration = 30
    private var fps = 30

    var laneLabel: String = ""
        set(v) { if (v != field) { field = v; invalidate() } }

    private val d = ctx.resources.displayMetrics.density
    private val rulerH = (28 * d).toInt()
    private val rowH = (34 * d).toInt()
    private val laneH = (28 * d).toInt()
    private val pillL = 5 * d              // kolom pil layer di kiri (gaya Alight Motion): kotak warna + mata / centang
    private val pillW = 54 * d
    private val padL = (70 * d).toInt()
    private val padR = (12 * d).toInt()
    private val slop = ViewConfiguration.get(ctx).scaledTouchSlop
    private val minFling = ViewConfiguration.get(ctx).scaledMinimumFlingVelocity
    private val maxFling = ViewConfiguration.get(ctx).scaledMaximumFlingVelocity

    // Zoom & scroll
    private var ppf = 4f * d                // piksel per frame
    private var offX = 0f
    private var offY = 0f
    private var autoFit = true              // selama pengguna belum zoom/scroll manual, muat semua durasi di layar
    private val scroller = OverScroller(ctx)
    private var vt: VelocityTracker? = null

    private val pBar = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pText = Paint(Paint.ANTI_ALIAS_FLAG).apply { textSize = 11f * d; typeface = Ui.MEDIUM }
    private val pSmall = Paint(Paint.ANTI_ALIAS_FLAG).apply { textSize = 9.5f * d; typeface = Ui.MEDIUM; color = Ui.DIM }
    private val pTick = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Ui.SURFACE3; strokeWidth = 1f * d }
    private val pHead = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Ui.DANGER; strokeWidth = 2f * d }
    private val pTag = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Ui.DANGER }
    private val pTagText = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE; textSize = 10f * d; typeface = Ui.MEDIUM; textAlign = Paint.Align.CENTER
    }
    private val pOutline = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Ui.GOLD; style = Paint.Style.STROKE; strokeWidth = 2f * d }
    private val pHandle = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.WHITE }
    private val pLane = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.argb(46, 255, 192, 67) }
    private val pRulerBg = Paint().apply { color = Ui.SURFACE }
    private val pKeyFill = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Ui.GOLD; style = Paint.Style.FILL }
    private val pKeyEdge = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(200, 20, 20, 28); style = Paint.Style.STROKE; strokeWidth = 1.2f * d
    }
    private val rect = RectF()
    private val pPillBg = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pGlyph = Paint(Paint.ANTI_ALIAS_FLAG).apply { textSize = 10f * d; typeface = Typeface.DEFAULT_BOLD; textAlign = Paint.Align.CENTER }
    private val pEye = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeWidth = 1.6f * d; strokeCap = Paint.Cap.ROUND }
    var onLayerHold: ((Int) -> Unit)? = null     // tahan pil layer
    private var pillRow = -1
    private var pillEye = false
    private var pillHeld = false
    private val pillHold = Runnable {
        val r = rows.getOrNull(pillRow)
        if (r != null && dragMode == 0 && pendingMode != 0) {
            pillHeld = true
            pendingMode = 0
            performHapticFeedback(android.view.HapticFeedbackConstants.LONG_PRESS)
            onLayerHold?.invoke(r.id)
        }
    }
    private val dia = Path()

    // 0 tidak ada, 1 geser bar, 2 trim kiri, 3 trim kanan, 4 scrub (penggaris/garis playhead), 5 seret keyframe lane,
    // 6 scrub di lane, 7 geser tampilan, 8 cubit
    private var dragMode = 0
    private var pendingMode = 0        // jenis seret bar/lane yang menunggu kepastian arah (horizontal)
    private var dragRow = -1
    private var dragStartX = 0f
    private var dragDelta = 0
    private var laneKeyFrame = 0
    private var laneKeyNew = 0
    var multiMode = false           // Pilih banyak: ketukan menandai layer, tahan-angkat dimatikan
    private var tapRowId = -1          // layer yang diketuk (0 = area kosong, -1 = bukan ketukan baris)
    private var tapSeekFrame = -1       // ketuk di lane keyframe tanpa menyeret: pindahkan playhead ke sini
    private var downX = 0f
    private var downY = 0f
    private var lastX = 0f
    private var lastY = 0f
    private var pinchSpan = 0f
    private var pinchFocusX = 0f

    // Urutkan layer: tahan bar lalu seret naik/turun (seperti Alight Motion / CapCut)
    private var reorderY = 0f          // posisi jari di koordinat konten
    private val longPress = Runnable { startReorder() }
    private val autoScroll = object : Runnable {
        override fun run() {
            if (dragMode != 9) return
            val edge = 30 * d
            var dy = 0f
            if (lastY < rulerH + edge) dy = -9f * d * ((rulerH + edge - lastY) / edge).coerceIn(0.3f, 1.5f)
            else if (lastY > height - edge) dy = 9f * d * ((lastY - (height - edge)) / edge).coerceIn(0.3f, 1.5f)
            if (dy != 0f) {
                offY = (offY + dy).coerceIn(0f, maxScrollY())
                reorderY = contentY(lastY)
                invalidate()
            }
            postOnAnimation(this)
        }
    }

    init { setBackgroundColor(Ui.SURFACE) }

    // ---------- Koordinat ----------
    private fun tailFrames() = maxOf(fps, duration / 6)
    private fun contentFrames() = duration + tailFrames()
    private fun viewW() = width - padL - padR
    private fun fitPpf() = viewW().toFloat() / maxOf(contentFrames(), 1)
    // boleh digulir sampai ujung proyek berada di 25% lebar layar, supaya selalu ada ruang bebas untuk menggeser
    private fun maxScrollX() = maxOf(0f, duration * ppf - viewW() * 0.25f)
    private fun maxScrollY() = maxOf(0f, contentH - (height - rulerH).toFloat())
    private fun xOf(f: Float) = padL + f * ppf - offX
    private fun frameAtX(x: Float): Int = Math.round((x - padL + offX) / ppf).coerceIn(0, duration - 1)
    private fun frameAtXFree(x: Float): Int = Math.round((x - padL + offX) / ppf)
    private fun contentY(y: Float) = y - rulerH + offY

    private fun clampScroll() {
        offX = offX.coerceIn(0f, maxScrollX())
        offY = offY.coerceIn(0f, maxScrollY())
    }

    /** Muat seluruh durasi di lebar layar dan kembali ke atas. */
    fun fit() {
        autoFit = true
        applyFit()
        offY = 0f
        invalidate()
    }

    private fun applyFit() {
        if (width <= 0) return
        ppf = fitPpf().coerceIn(0.2f * d, 60f * d)
        offX = 0f
    }

    override fun onSizeChanged(w: Int, h: Int, ow: Int, oh: Int) {
        if (autoFit) applyFit()
        clampScroll()
    }

    // ---------- Data ----------
    fun setPlayhead(ph: Int, follow: Boolean = false) {
        if (ph == playhead) return
        playhead = ph
        if (follow && dragMode == 0) {
            val hx = xOf(ph.toFloat())
            val left = padL + 10 * d
            val right = width - padR - 24 * d
            if (hx > right || hx < left) {
                offX = (padL + ph * ppf - viewW() * 0.3f - padL).coerceIn(0f, maxScrollX())
            }
        }
        invalidate()
    }

    /** Gulir vertikal agar layer terpilih terlihat. */
    fun scrollToSelected() {
        for (i in rows.indices) if (rows[i].sel) {
            val top = tops[i]
            val bot = top + heightOf(rows[i])
            val viewH = height - rulerH
            if (top < offY) offY = top.toFloat()
            else if (bot > offY + viewH) offY = (bot - viewH).toFloat()
            clampScroll()
            invalidate()
            return
        }
    }

    private fun heightOf(r: Row) = if (r.sel) rowH + laneH else rowH

    private fun relayout() {
        tops = IntArray(rows.size)
        var y = (4 * d).toInt()
        for (i in rows.indices) {
            tops[i] = y
            y += heightOf(rows[i])
        }
        contentH = y + (24 * d).toInt()
    }

    private fun ints(s: String?): IntArray {
        if (s.isNullOrEmpty()) return IntArray(0)
        val out = ArrayList<Int>()
        for (t in s.split(",")) t.toIntOrNull()?.let { out.add(it) }
        return out.toIntArray()
    }

    fun setData(s: String, ph: Int, dur: Int, f: Int) {
        val oldDur = duration
        playhead = ph
        duration = maxOf(dur, 1)
        fps = if (f > 0) f else 30
        if (dragMode in 1..3 || dragMode == 5 || dragMode == 9) {
            invalidate()
            return
        }
        val n = ArrayList<Row>()
        for (line in s.split("\n")) {
            val p = line.split("|")
            if (p.size < 7) continue
            val rgba = (p.getOrNull(7)?.toLongOrNull() ?: 0xFFFFFFFFL)
            val col = Color.rgb(((rgba shr 24) and 255).toInt(), ((rgba shr 16) and 255).toInt(), ((rgba shr 8) and 255).toInt())
            n.add(Row(p[0].toIntOrNull() ?: continue, p[1], p[2].toIntOrNull() ?: 0, p[3].toIntOrNull() ?: 1,
                p[4].toIntOrNull() ?: 0, p[6] == "1", col, ints(p.getOrNull(8)), ints(p.getOrNull(9)), p[6] == "2", p.getOrNull(10) == "1"))
        }
        rows = n
        relayout()
        if (autoFit && oldDur != duration) applyFit()
        clampScroll()
        invalidate()
    }

    // ---------- Gambar ----------
    private fun diamond(c: Canvas, cx: Float, cy: Float, r: Float, fill: Paint) {
        dia.reset()
        dia.moveTo(cx, cy - r); dia.lineTo(cx + r, cy); dia.lineTo(cx, cy + r); dia.lineTo(cx - r, cy); dia.close()
        c.drawPath(dia, fill)
        c.drawPath(dia, pKeyEdge)
    }

    private fun dim(c: Int): Int = Color.rgb((Color.red(c) * 0.55f).toInt(), (Color.green(c) * 0.55f).toInt(), (Color.blue(c) * 0.55f).toInt())

    private fun drawPill(c: Canvas, i: Int, r: Row) {
        val top = tops[i] + 3 * d
        val bottom = tops[i] + rowH - 3 * d
        rect.set(pillL, top, pillL + pillW, bottom)
        pPillBg.color = if (r.msel) Color.rgb(120, 255, 176) else Ui.SURFACE3
        c.drawRoundRect(rect, 9 * d, 9 * d, pPillBg)
        if (r.sel) {
            pEye.color = Ui.GOLD
            c.drawRoundRect(rect, 9 * d, 9 * d, pEye)
        }
        val cy = (top + bottom) / 2f
        // kotak warna layer (+ huruf jenis)
        val sx = pillL + 7 * d
        rect.set(sx, cy - 9 * d, sx + 18 * d, cy + 9 * d)
        pPillBg.color = if (r.hidden) dim(r.color) else r.color
        if (r.type == 1) {
            pEye.color = if (r.msel) Color.rgb(10, 40, 25) else Ui.TEXT
            c.drawRoundRect(rect, 4 * d, 4 * d, pEye)
        } else {
            c.drawRoundRect(rect, 4 * d, 4 * d, pPillBg)
        }
        val g = when (r.type) { 3 -> "G"; 4 -> "V"; 5 -> "S"; 6 -> "T"; 2 -> "N"; else -> "" }
        if (g.isNotEmpty()) {
            val lum = 0.299f * Color.red(r.color) + 0.587f * Color.green(r.color) + 0.114f * Color.blue(r.color)
            pGlyph.color = if (lum > 150f) Color.BLACK else Color.WHITE
            c.drawText(g, sx + 9 * d, cy + 3.5f * d, pGlyph)
        }
        // mata / centang
        val ex = pillL + pillW - 15 * d
        val ink = if (r.msel) Color.rgb(10, 40, 25) else if (r.hidden) Ui.DIM else Ui.TEXT
        if (multiMode) {
            pEye.color = ink
            if (r.msel) {
                c.drawLine(ex - 5 * d, cy, ex - 1.5f * d, cy + 4 * d, pEye)
                c.drawLine(ex - 1.5f * d, cy + 4 * d, ex + 6 * d, cy - 4 * d, pEye)
            } else {
                c.drawCircle(ex, cy, 6 * d, pEye)
            }
        } else {
            pEye.color = ink
            rect.set(ex - 8 * d, cy - 5 * d, ex + 8 * d, cy + 5 * d)
            c.drawOval(rect, pEye)
            if (!r.hidden) {
                pPillBg.color = ink
                c.drawCircle(ex, cy, 2.2f * d, pPillBg)
            } else {
                c.drawLine(ex - 7 * d, cy + 6 * d, ex + 7 * d, cy - 6 * d, pEye)
            }
        }
    }

    private fun drawRow(c: Canvas, i: Int, r: Row) {
            var a = r.inF
            var b = r.outF
            if (i == dragRow) {
                when (dragMode) {
                    1 -> { a += dragDelta; b += dragDelta }
                    2 -> a = minOf(a + dragDelta, b - 1)
                    3 -> b = maxOf(b + dragDelta, a + 1)
                }
            }
            val x0 = xOf(a.toFloat())
            val x1 = maxOf(xOf(b.toFloat()), x0 + 6 * d)
            if (x1 < -20 * d || x0 > width + 20 * d) { /* di luar layar */ }
            val top = tops[i] + 3 * d
            val bottom = tops[i] + rowH - 3 * d
            pBar.color = if (r.sel || r.msel) r.color else dim(r.color)
            rect.set(x0, top, x1, bottom)
            c.drawRoundRect(rect, 7 * d, 7 * d, pBar)
            if (r.sel || r.msel) c.drawRoundRect(rect, 7 * d, 7 * d, pOutline)
            c.save()
            c.clipRect(rect)
            val lum = 0.299f * Color.red(pBar.color) + 0.587f * Color.green(pBar.color) + 0.114f * Color.blue(pBar.color)
            pText.color = if (lum > 150f) Color.BLACK else Color.WHITE
            val hasKeys = r.keys.isNotEmpty()
            val textX = maxOf(x0, 0f) + 10 * d     // nama tetap terlihat walau awal bar tergulir keluar layar
            val textY = if (hasKeys) top + 14 * d else (top + bottom) / 2 + 4 * d
            c.drawText(if (r.msel) "✓ " + r.name else r.name, textX, textY, pText)
            if (hasKeys) {
                val shift = if (i == dragRow && dragMode == 1) dragDelta else 0
                for (kf in r.keys) {
                    val kx = xOf((kf + shift).toFloat())
                    if (kx < x0 + 4 * d || kx > x1 - 4 * d) continue
                    diamond(c, kx, bottom - 6 * d, 4.2f * d, pKeyFill)
                }
            }
            c.restore()
            if (r.sel && x1 - x0 > 40 * d) {
                val hh = (bottom - top) * 0.5f
                val cy = (top + bottom) / 2
                val w = 3 * d
                rect.set(x0 + 3 * d, cy - hh / 2, x0 + 3 * d + w, cy + hh / 2)
                c.drawRoundRect(rect, w, w, pHandle)
                rect.set(x1 - 3 * d - w, cy - hh / 2, x1 - 3 * d, cy + hh / 2)
                c.drawRoundRect(rect, w, w, pHandle)
            }
            if (r.sel) {
                val lt = tops[i] + rowH
                rect.set(padL.toFloat(), lt + 2 * d, (width - padR).toFloat(), lt + laneH - 2 * d)
                c.drawRoundRect(rect, 8 * d, 8 * d, pLane)
                val cy = lt + laneH / 2f
                if (r.chKeys.isEmpty()) {
                    pSmall.color = Ui.DIM
                    c.drawText("$laneLabel · belum ada keyframe (ketuk ◇ di panel)", padL + 10 * d, cy + 3.5f * d, pSmall)
                } else {
                    val xs = r.chKeys.map { kf -> xOf((if (dragMode == 5 && kf == laneKeyFrame) laneKeyNew else kf).toFloat()) }
                    pTick.color = Color.argb(120, 255, 192, 67)
                    c.drawLine(xs.minOrNull() ?: 0f, cy, xs.maxOrNull() ?: 0f, cy, pTick)
                    pTick.color = Ui.SURFACE3
                    pSmall.color = Color.argb(150, 255, 255, 255)
                    c.drawText(laneLabel, padL + 10 * d, lt + 10 * d, pSmall)
                    for (kf in r.chKeys) {
                        val moving = dragMode == 5 && kf == laneKeyFrame
                        val kx = xOf((if (moving) laneKeyNew else kf).toFloat())
                        pKeyFill.color = if (moving || kf == playhead) Color.WHITE else Ui.GOLD
                        diamond(c, kx, cy + 3 * d, 7 * d, pKeyFill)
                    }
                    pKeyFill.color = Ui.GOLD
                }
            }
    }

    /** Slot tujuan (indeks baris akhir) bila baris yang diseret dilepas di posisi jari sekarang. */
    private fun reorderTarget(): Int {
        var t = 0
        for (j in rows.indices) if (j != dragRow && reorderY > tops[j] + heightOf(rows[j]) / 2f) t++
        return t
    }

    private fun drawReorder(c: Canvas) {
        val r = rows.getOrNull(dragRow) ?: return
        // garis penyisip di slot tujuan
        val t = reorderTarget()
        var k = 0
        var y = (4 * d)
        for (j in rows.indices) {
            if (j == dragRow) continue
            if (k == t) { y = tops[j].toFloat(); break }
            y = (tops[j] + heightOf(rows[j])).toFloat()
            k++
        }
        pTag.color = Ui.ACCENT_SOFT
        rect.set(padL.toFloat(), y - 1.5f * d, (width - padR).toFloat(), y + 1.5f * d)
        c.drawRoundRect(rect, 2 * d, 2 * d, pTag)
        pTag.color = Ui.DANGER
        // baris terangkat mengikuti jari
        val shift = reorderY - (tops[dragRow] + rowH / 2f)
        c.save()
        c.translate(dragDelta * ppf, shift)
        c.saveLayerAlpha(0f, tops[dragRow].toFloat() - 4 * d, width.toFloat(), tops[dragRow] + heightOf(r) + 4 * d, 210)
        drawRow(c, dragRow, r)
        c.restore()
        c.restore()
    }

    private fun selectLocal(i: Int) {
        val r = rows[i]
        if (r.sel) return
        NativeEngine.nativeSelectLayer(r.id)
        for (k in rows.indices) {
            val o = rows[k]
            val want = k == i
            if (o.sel != want) rows[k] = Row(o.id, o.name, o.inF, o.outF, o.type, want, o.color, o.keys, o.chKeys, o.msel, o.hidden)
        }
        relayout()
    }

    private fun startReorder() {
        if (multiMode || dragMode != 0 || pendingMode == 0 || dragRow !in rows.indices) return
        selectLocal(dragRow)
        pendingMode = 0
        tapSeekFrame = -1
        dragMode = 9
        reorderY = contentY(lastY)
        autoFit = false
        parent?.requestDisallowInterceptTouchEvent(true)
        performHapticFeedback(android.view.HapticFeedbackConstants.LONG_PRESS)
        postOnAnimation(autoScroll)
        invalidate()
    }

    override fun onDraw(c: Canvas) {
        if (width <= 0) return
        // ---- baris (digulir) ----
        c.save()
        c.clipRect(0f, rulerH.toFloat(), width.toFloat(), height.toFloat())
        c.translate(0f, rulerH - offY)
        if (rows.isEmpty()) {
            pSmall.color = Ui.DIM
            pSmall.textAlign = Paint.Align.CENTER
            c.drawText("Proyek masih kosong · ketuk + untuk menambah bentuk atau gambar", width / 2f, rowH * 0.7f, pSmall)
            pSmall.textAlign = Paint.Align.LEFT
        }
        rows.forEachIndexed { i, r -> if (!(dragMode == 9 && i == dragRow)) drawRow(c, i, r) }
        if (dragMode == 9) drawReorder(c)
        c.restore()

        // ---- garis playhead (menembus baris & penggaris) ----
        val hx = xOf(playhead.toFloat())
        c.drawLine(hx, 0f, hx, height.toFloat(), pHead)

        // ---- kolom pil layer (menempel di kiri, di atas bar yang tergulir) ----
        c.save()
        c.clipRect(0f, rulerH.toFloat(), width.toFloat(), height.toFloat())
        c.translate(0f, rulerH - offY)
        pPillBg.color = Ui.SURFACE
        c.drawRect(0f, offY, pillL + pillW + 5 * d, offY + height, pPillBg)
        rows.forEachIndexed { i, r -> if (!(dragMode == 9 && i == dragRow)) drawPill(c, i, r) }
        c.restore()

        // ---- penggaris menempel di atas ----
        c.drawRect(0f, 0f, width.toFloat(), rulerH.toFloat(), pRulerBg)
        c.save()
        c.clipRect(0f, 0f, width.toFloat(), rulerH.toFloat())
        val minPx = 54 * d
        val cands = intArrayOf(1, 2, 5, 10, fps / 2, fps, fps * 2, fps * 5, fps * 10, fps * 15, fps * 30, fps * 60, fps * 120, fps * 300)
        var step = fps * 600
        for (cand in cands) if (cand >= 1 && cand * ppf >= minPx) { step = cand; break }
        pText.color = Ui.DIM
        pText.textSize = 10f * d
        var f0 = (Math.floor(((offX - padL) / ppf).toDouble()).toInt() / step) * step
        if (f0 < 0) f0 = 0
        var f = f0
        val maxF = ((width + offX - padL) / ppf).toInt() + step
        while (f <= maxF) {
            val x = xOf(f.toFloat())
            c.drawLine(x, rulerH - 8 * d, x, rulerH.toFloat(), pTick)
            val label = if (step >= fps) Ui.time(f, fps).substringBefore('.') else Ui.time(f, fps).substringAfter(':')
            c.drawText(label, x + 3 * d, rulerH - 11 * d, pText)
            // sub-tick
            if (step >= 5) {
                val sub = step / 5
                for (k in 1..4) {
                    val sx = xOf((f + sub * k).toFloat())
                    c.drawLine(sx, rulerH - 4 * d, sx, rulerH.toFloat(), pTick)
                }
            }
            f += step
        }
        pText.textSize = 11f * d
        // penanda ujung proyek
        val ex = xOf(duration.toFloat())
        pTick.color = Color.argb(160, 255, 255, 255)
        c.drawLine(ex, 0f, ex, rulerH.toFloat(), pTick)
        pTick.color = Ui.SURFACE3
        // label waktu pada playhead
        val tag = Ui.time(playhead, fps)
        val tw = pTagText.measureText(tag) + 14 * d
        rect.set(hx - tw / 2, 2 * d, hx + tw / 2, rulerH - 6 * d)
        c.drawRoundRect(rect, 8 * d, 8 * d, pTag)
        c.drawText(tag, hx, rect.centerY() + 3.5f * d, pTagText)
        c.restore()
    }

    // ---------- Sentuhan ----------
    private fun rowAt(cy: Float): Int {
        for (i in rows.indices) if (cy >= tops[i] && cy < tops[i] + heightOf(rows[i])) return i
        return -1
    }

    private fun startPan() {
        dragMode = 7
        autoFit = false
        parent?.requestDisallowInterceptTouchEvent(true)
    }

    override fun computeScroll() {
        if (scroller.computeScrollOffset()) {
            offX = scroller.currX.toFloat()
            offY = scroller.currY.toFloat()
            clampScroll()
            postInvalidateOnAnimation()
        }
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        if (vt == null) vt = VelocityTracker.obtain()
        vt?.addMovement(e)

        // ---- cubit dua jari: zoom waktu + geser ----
        if (e.pointerCount >= 2 && e.actionMasked != MotionEvent.ACTION_POINTER_UP) {
            val fx = (e.getX(0) + e.getX(1)) / 2f
            val span = Math.abs(e.getX(0) - e.getX(1)).coerceAtLeast(30f * d)
            removeCallbacks(longPress)
            if (dragMode != 8) {
                // batalkan seret satu jari yang sedang berjalan (tanpa menerapkannya)
                dragMode = 8
                dragRow = -1
                dragDelta = 0
                autoFit = false
                scroller.forceFinished(true)
                parent?.requestDisallowInterceptTouchEvent(true)
            } else {
                val fMin = 0.2f * d
                val frameAtFocus = (pinchFocusX - padL + offX) / ppf
                ppf = (ppf * span / pinchSpan).coerceIn(fMin, 60f * d)
                offX = padL + frameAtFocus * ppf - fx   // frame di bawah jari tetap di bawah jari
                offX = offX.coerceIn(0f, maxScrollX())
            }
            pinchSpan = span
            pinchFocusX = fx
            invalidate()
            return true
        }
        if (dragMode == 8) {
            if (e.actionMasked == MotionEvent.ACTION_UP || e.actionMasked == MotionEvent.ACTION_CANCEL) {
                dragMode = 0
                recycleVt()
            }
            return true
        }

        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                scroller.forceFinished(true)
                downX = e.x; downY = e.y; lastX = e.x; lastY = e.y
                pendingMode = 0
                tapSeekFrame = -1
                val hx = xOf(playhead.toFloat())
                // pil layer di kiri: ketuk = pilih / tandai, ketuk mata = sembunyikan, tahan = menu / mode pilih banyak
                if (e.y >= rulerH && e.x <= pillL + pillW + 4 * d) {
                    val cyy = contentY(e.y)
                    val pi = rowAt(cyy)
                    if (pi >= 0 && cyy < tops[pi] + rowH) {
                        pillRow = pi
                        pillEye = e.x > pillL + pillW - 28 * d
                        pillHeld = false
                        dragMode = 0
                        dragRow = -1
                        pendingMode = 7
                        tapRowId = -1
                        removeCallbacks(pillHold)
                        if (!pillEye) postDelayed(pillHold, ViewConfiguration.getLongPressTimeout().toLong())
                        return true
                    }
                }
                // garis playhead: bisa diraih di mana saja sepanjang garis (±20dp), di atas semua yang lain
                if (e.y < rulerH || Math.abs(e.x - hx) <= 20 * d) {
                    parent?.requestDisallowInterceptTouchEvent(true)
                    dragMode = 4
                    NativeEngine.nativeSeek(frameAtX(e.x))
                    invalidate()
                    return true
                }
                val cy = contentY(e.y)
                val i = rowAt(cy)
                tapRowId = -1
                if (i < 0) { dragMode = 0; pendingMode = 7; tapRowId = 0; return true }   // area kosong: geser tampilan; ketuk = batal pilih
                val r = rows[i]
                if (r.sel && cy >= tops[i] + rowH) {
                    var best = -1
                    var bd = 16 * d
                    for (kf in r.chKeys) {
                        val dd = Math.abs(xOf(kf.toFloat()) - e.x)
                        if (dd < bd) { bd = dd; best = kf }
                    }
                    dragRow = i
                    // Seperti layer: keyframe baru bisa digeser setelah dipilih (playhead di atasnya). Selain itu,
                    // seret di lane = menggulir timeline; ketuk = pindahkan playhead.
                    if (best >= 0 && best == playhead.toInt()) {
                        pendingMode = 5
                        laneKeyFrame = best
                        laneKeyNew = best
                    } else {
                        pendingMode = 7
                        tapSeekFrame = if (best >= 0) best else frameAtX(e.x)
                    }
                    invalidate()
                    return true
                }
                dragRow = i
                dragStartX = e.x
                dragDelta = 0
                val xa = xOf(r.inF.toFloat())
                val xb = maxOf(xOf(r.outF.toFloat()), xa + 6 * d)
                // Layer dipilih HANYA bila diketuk tepat di bar-nya (tanpa menggeser). Menyentuh ruang kosong di baris
                // yang sama hanya menggeser timeline; ketukan di sana membatalkan pilihan.
                val onBar = e.x >= xa - 6 * d && e.x <= xb + 6 * d
                tapRowId = if (onBar || multiMode) r.id else 0
                val edge = minOf(18 * d, (xb - xa) / 3f)
                // Seret di mana pun (termasuk di atas layer) = geser timeline bebas. Hanya pegangan di ujung layer
                // terpilih yang memotong durasi. Tahan lama = angkat layer: geser kiri/kanan = pindah waktu,
                // naik/turun = ubah urutan.
                pendingMode = when {
                    !r.sel -> 7
                    e.x < xa - 10 * d || e.x > xb + 10 * d -> 7
                    e.x <= xa + minOf(edge, 14 * d) -> 2
                    e.x >= xb - minOf(edge, 14 * d) -> 3
                    else -> 7
                }
                removeCallbacks(longPress)
                if (onBar) postDelayed(longPress, ViewConfiguration.getLongPressTimeout().toLong())
            }
            MotionEvent.ACTION_MOVE -> {
                val dx = e.x - downX
                val dy = e.y - downY
                if (pendingMode != 0 && dragMode == 0) {
                    // menentukan arah: horizontal dominan → seret objek; vertikal/area kosong → geser tampilan
                    if (Math.abs(dx) > slop || Math.abs(dy) > slop) {
                        removeCallbacks(longPress)
                        removeCallbacks(pillHold)
                        pillRow = -1
                        if (pendingMode == 7 || Math.abs(dy) > Math.abs(dx)) {
                            startPan()
                            dragRow = -1
                        } else {
                            dragMode = pendingMode
                            parent?.requestDisallowInterceptTouchEvent(true)
                        }
                        pendingMode = 0
                    }
                }
                when (dragMode) {
                    4, 6 -> NativeEngine.nativeSeek(frameAtX(e.x))
                    5 -> {
                        val r = rows.getOrNull(dragRow)
                        if (r != null) {
                            var lo = 0
                            var hi = duration - 1
                            for (kf in r.chKeys) {
                                if (kf < laneKeyFrame) lo = maxOf(lo, kf + 1)
                                if (kf > laneKeyFrame) hi = minOf(hi, kf - 1)
                            }
                            laneKeyNew = frameAtX(e.x).coerceIn(lo, maxOf(lo, hi))
                            NativeEngine.nativeSeek(laneKeyNew)
                        }
                    }
                    1, 2, 3 -> dragDelta = Math.round((e.x - dragStartX) / ppf)
                    9 -> {
                        reorderY = contentY(e.y)
                        val dxm = e.x - downX
                        dragDelta = if (Math.abs(dxm) > 2 * slop) Math.round(dxm / ppf) else 0
                    }
                    7 -> {
                        offX = (offX - (e.x - lastX)).coerceIn(0f, maxScrollX())
                        offY = (offY - (e.y - lastY)).coerceIn(0f, maxScrollY())
                    }
                }
                lastX = e.x; lastY = e.y
                invalidate()
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                val up = e.actionMasked == MotionEvent.ACTION_UP
                removeCallbacks(longPress)
                removeCallbacks(pillHold)
                val pr = rows.getOrNull(pillRow)
                if (up && pr != null && !pillHeld && dragMode == 0 && pendingMode != 0) {
                    if (pillEye && !multiMode) NativeEngine.nativeSetHidden(pr.id, !pr.hidden)
                    else if (multiMode) NativeEngine.nativeToggleMulti(pr.id)
                    else NativeEngine.nativeSelectLayer(pr.id)
                }
                pillRow = -1
                if (up && dragMode == 0 && pendingMode != 0 && tapRowId >= 0) {
                    if (multiMode) { if (tapRowId > 0) NativeEngine.nativeToggleMulti(tapRowId) }
                    else NativeEngine.nativeSelectLayer(tapRowId)
                }
                tapRowId = -1
                if (dragMode == 9 && up && dragRow in rows.indices) {
                    val t = reorderTarget()
                    if (dragDelta != 0) NativeEngine.nativeShift(dragDelta)
                    if (t != dragRow) NativeEngine.nativeMoveLayer(rows.size - 1 - t)
                }
                if (up && pendingMode != 0 && dragMode == 0 && tapSeekFrame >= 0) NativeEngine.nativeSeek(tapSeekFrame)
                tapSeekFrame = -1
                if (dragMode == 7 && up) {
                    vt?.computeCurrentVelocity(1000, maxFling.toFloat())
                    val vx = -(vt?.xVelocity ?: 0f)
                    val vy = -(vt?.yVelocity ?: 0f)
                    if (Math.abs(vx) > minFling || Math.abs(vy) > minFling) {
                        scroller.fling(offX.toInt(), offY.toInt(), vx.toInt(), vy.toInt(),
                            0, maxScrollX().toInt(), 0, maxScrollY().toInt())
                        postInvalidateOnAnimation()
                    }
                }
                if (dragMode in 1..3 && dragRow in rows.indices && up && dragDelta != 0) {
                    val r = rows[dragRow]
                    when (dragMode) {
                        1 -> NativeEngine.nativeShift(dragDelta)
                        2 -> NativeEngine.nativeTrim(r.inF + dragDelta, r.outF)
                        3 -> NativeEngine.nativeTrim(r.inF, r.outF + dragDelta)
                    }
                }
                if (dragMode == 5 && up && laneKeyNew != laneKeyFrame)
                    NativeEngine.nativeKeyEdit(6, laneKeyFrame, laneKeyNew, 0f, 0f, 0f, 0f)
                dragMode = 0
                pendingMode = 0
                dragRow = -1
                dragDelta = 0
                recycleVt()
                invalidate()
            }
        }
        return true
    }

    private fun recycleVt() {
        vt?.recycle()
        vt = null
    }
}
