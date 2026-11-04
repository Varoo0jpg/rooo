package com.mystudio.editor

import android.app.Activity
import android.app.Dialog
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Typeface
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.GradientDrawable
import android.text.InputType
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import kotlin.math.cos
import kotlin.math.sin

/** Tema gelap: hitam kebiruan, aksen violet-biru, keyframe berwarna emas. */
object Ui {
    val BG = Color.rgb(9, 9, 13)
    val SURFACE = Color.rgb(21, 22, 29)
    val SURFACE2 = Color.rgb(31, 33, 43)
    val SURFACE3 = Color.rgb(44, 47, 61)
    val HAIR = Color.argb(30, 255, 255, 255)
    val ACCENT = Color.rgb(124, 108, 255)
    val ACCENT2 = Color.rgb(77, 163, 255)
    val ACCENT_SOFT = Color.rgb(176, 166, 255)
    val GOLD = Color.rgb(255, 192, 67)
    val TEXT = Color.rgb(242, 243, 247)
    val DIM = Color.rgb(142, 148, 168)
    val DANGER = Color.rgb(255, 93, 108)

    val MEDIUM: Typeface = Typeface.create("sans-serif-medium", Typeface.NORMAL)
    val REGULAR: Typeface = Typeface.create("sans-serif", Typeface.NORMAL)

    /** Format waktu m:ss.ff dari nomor frame. */
    fun time(frame: Int, fps: Int): String {
        val f = if (fps > 0) fps else 30
        val sec = frame / f
        return String.format("%d:%02d.%02d", sec / 60, sec % 60, frame % f)
    }

    fun alpha(c: Int, a: Int): Int = Color.argb(a, Color.red(c), Color.green(c), Color.blue(c))
}

fun Context.dp(v: Int): Int = (v * resources.displayMetrics.density + 0.5f).toInt()
fun Context.dpf(v: Float): Float = v * resources.displayMetrics.density

fun Context.rounded(color: Int, radiusDp: Float, strokeColor: Int = 0, strokeDp: Int = 0): GradientDrawable {
    val g = GradientDrawable()
    g.setColor(color)
    g.cornerRadius = dpf(radiusDp)
    if (strokeDp > 0) g.setStroke(dp(strokeDp), strokeColor)
    return g
}

fun Context.roundedTop(color: Int, radiusDp: Float): GradientDrawable {
    val g = GradientDrawable()
    g.setColor(color)
    val r = dpf(radiusDp)
    g.cornerRadii = floatArrayOf(r, r, r, r, 0f, 0f, 0f, 0f)
    return g
}

fun Context.accentFill(radiusDp: Float): GradientDrawable {
    val g = GradientDrawable()
    g.setColor(Ui.ACCENT)
    g.cornerRadius = dpf(radiusDp)
    return g
}

/** Kartu berlapis: gradasi vertikal halus + garis tepi tipis → terasa punya kedalaman, tidak datar. */
fun Context.card(radiusDp: Float, top: Int = Ui.SURFACE2, bottom: Int = Ui.SURFACE): GradientDrawable {
    val g = GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM, intArrayOf(top, bottom))
    g.cornerRadius = dpf(radiusDp)
    g.setStroke(1, Ui.HAIR)
    return g
}

/** Panel bawah (sheet/timeline): sudut atas membulat, gradasi halus, garis tepi atas. */
fun Context.panelTop(radiusDp: Float): GradientDrawable {
    val g = GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM, intArrayOf(Color.rgb(28, 30, 40), Ui.SURFACE))
    val r = dpf(radiusDp)
    g.cornerRadii = floatArrayOf(r, r, r, r, 0f, 0f, 0f, 0f)
    g.setStroke(1, Ui.HAIR)
    return g
}

fun Context.label(t: String, sp: Float = 12f, color: Int = Ui.TEXT, bold: Boolean = false): TextView {
    val v = TextView(this)
    v.text = t
    v.textSize = sp
    v.setTextColor(color)
    v.typeface = if (bold) Ui.MEDIUM else Ui.REGULAR
    return v
}

fun Context.sectionLabel(t: String): TextView {
    val v = label(t.uppercase(), 10f, Ui.DIM, true)
    v.letterSpacing = 0.1f
    v.setPadding(dp(2), dp(12), dp(2), dp(6))
    return v
}

fun styleChip(c: TextView, selected: Boolean) {
    val ctx = c.context
    if (selected) {
        c.background = ctx.rounded(Ui.alpha(Ui.ACCENT, 64), 17f, Ui.ACCENT, 1)
        c.setTextColor(Color.WHITE)
    } else {
        c.background = ctx.card(17f)
        c.setTextColor(Ui.TEXT)
    }
}

/** Tombol "pil" satu baris: untuk pilihan (bentuk, easing, kategori). */
fun Context.chip(text: String, selected: Boolean = false, onClick: () -> Unit): TextView {
    val c = TextView(this)
    c.text = text
    c.textSize = 12f
    c.typeface = Ui.MEDIUM
    c.gravity = Gravity.CENTER
    c.isAllCaps = false
    c.setSingleLine()
    c.minHeight = dp(34)
    c.setPadding(dp(13), dp(5), dp(13), dp(5))
    styleChip(c, selected)
    c.setOnClickListener { onClick() }
    return c
}

/** kind: 0 biasa, 1 utama (gradien aksen), 2 bahaya. */
fun Context.button(text: String, kind: Int = 0, onClick: () -> Unit): TextView {
    val c = TextView(this)
    c.text = text
    c.textSize = 12.5f
    c.typeface = Ui.MEDIUM
    c.gravity = Gravity.CENTER
    c.isAllCaps = false
    c.setSingleLine()
    c.minHeight = dp(40)
    c.setPadding(dp(12), dp(6), dp(12), dp(6))
    when (kind) {
        1 -> { c.background = accentFill(12f); c.setTextColor(Color.WHITE) }
        2 -> { c.background = card(12f); c.setTextColor(Ui.DANGER) }
        else -> { c.background = card(12f); c.setTextColor(Ui.TEXT) }
    }
    c.setOnClickListener { onClick() }
    return c
}

fun lp(w: Int, h: Int, weight: Float = 0f): LinearLayout.LayoutParams = LinearLayout.LayoutParams(w, h, weight)

/** Ikon digambar dengan kode (tidak bergantung font, jadi tidak ada karakter yang hilang). */
class IconView(ctx: Context, private val kind: Int, private val filled: Boolean = false,
               private val plain: Boolean = false) : View(ctx) {
    companion object {
        const val PLAY = 0
        const val PAUSE = 1
        const val PLUS = 2
        const val UNDO = 3
        const val REDO = 4
        const val MORE = 5
        const val UP = 6
        const val DOWN = 7
        const val CLOSE = 8
        const val CHEV_DOWN = 9
        const val CHEV_RIGHT = 10
        const val CHEV_LEFT = 11
        const val MOVE = 12
        const val STYLE = 13
        const val SPARK = 14
        const val CURVE = 15
        const val SCISSORS = 16
        const val COPY = 17
        const val TRASH = 18
        const val SPLIT = 19
        const val TRIM_FRONT = 20
        const val TRIM_BACK = 21
        const val FULL = 22
        const val CHECK = 23
        const val SKIP_START = 24
        const val SKIP_END = 25
        const val FIT = 26
        const val GRID = 27
        const val MINUS = 28
        const val CLIPBOARD = 29
        const val LAYERS = 30
        const val CAMERA = 31
        const val ROTATE = 32
        const val SCALE = 33
        const val ANCHOR = 34
        const val MORE_H = 35
        const val SPEED = 36
        const val TEXT = 37
        const val MASK = 38
        const val LINK = 39
        const val GROUP = 40
        const val SELECT = 41
        const val STAR = 42
    }

    var tint: Int = Ui.TEXT
        set(v) { field = v; invalidate() }
    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val path = Path()
    private val rect = RectF()

    init {
        isClickable = true
        if (!plain) background = ctx.card(40f)
        if (filled) {
            // Tombol utama: lingkaran solid. Play = putih dengan ikon gelap; lainnya = aksen dengan ikon putih.
            if (kind == PLAY || kind == PAUSE) {
                background = ctx.rounded(Color.WHITE, 40f)
                tint = Color.rgb(16, 16, 22)
            } else {
                background = ctx.accentFill(40f)
                tint = Color.WHITE
            }
        }
    }

    override fun setEnabled(enabled: Boolean) {
        super.setEnabled(enabled)
        alpha = if (enabled) 1f else 0.35f
    }

    private fun arrow(c: Canvas, tx: Float, ty: Float, dx: Float, dy: Float, s: Float) {
        // kepala panah di (tx,ty) menunjuk ke arah (dx,dy)
        val nx = -dy
        val ny = dx
        path.reset()
        path.moveTo(tx - dx * s + nx * s * 0.8f, ty - dy * s + ny * s * 0.8f)
        path.lineTo(tx, ty)
        path.lineTo(tx - dx * s - nx * s * 0.8f, ty - dy * s - ny * s * 0.8f)
        c.drawPath(path, p)
    }

    override fun onDraw(c: Canvas) {
        val w = width.toFloat()
        val h = height.toFloat()
        val cx = w / 2
        val cy = h / 2
        val r = minOf(w, h) * (if (plain) 0.25f else 0.24f)
        p.color = tint
        p.style = Paint.Style.STROKE
        p.strokeWidth = context.dpf(1.6f)
        p.strokeCap = Paint.Cap.ROUND
        p.strokeJoin = Paint.Join.ROUND
        when (kind) {
            PLAY -> {
                p.style = Paint.Style.FILL
                path.reset()
                path.moveTo(cx - r * 0.65f, cy - r * 1.05f)
                path.lineTo(cx + r * 1.1f, cy)
                path.lineTo(cx - r * 0.65f, cy + r * 1.05f)
                path.close()
                c.drawPath(path, p)
            }
            PAUSE -> {
                p.style = Paint.Style.FILL
                rect.set(cx - r * 0.85f, cy - r, cx - r * 0.2f, cy + r)
                c.drawRoundRect(rect, 3f, 3f, p)
                rect.set(cx + r * 0.2f, cy - r, cx + r * 0.85f, cy + r)
                c.drawRoundRect(rect, 3f, 3f, p)
            }
            PLUS -> {
                c.drawLine(cx - r, cy, cx + r, cy, p)
                c.drawLine(cx, cy - r, cx, cy + r, p)
            }
            CLOSE -> {
                val q = r * 0.8f
                c.drawLine(cx - q, cy - q, cx + q, cy + q, p)
                c.drawLine(cx - q, cy + q, cx + q, cy - q, p)
            }
            UP -> {
                c.drawLine(cx - r, cy + r * 0.4f, cx, cy - r * 0.5f, p)
                c.drawLine(cx, cy - r * 0.5f, cx + r, cy + r * 0.4f, p)
            }
            DOWN, CHEV_DOWN -> {
                c.drawLine(cx - r, cy - r * 0.4f, cx, cy + r * 0.5f, p)
                c.drawLine(cx, cy + r * 0.5f, cx + r, cy - r * 0.4f, p)
            }
            CHEV_RIGHT -> {
                c.drawLine(cx - r * 0.4f, cy - r, cx + r * 0.5f, cy, p)
                c.drawLine(cx + r * 0.5f, cy, cx - r * 0.4f, cy + r, p)
            }
            CHEV_LEFT -> {
                c.drawLine(cx + r * 0.4f, cy - r, cx - r * 0.5f, cy, p)
                c.drawLine(cx - r * 0.5f, cy, cx + r * 0.4f, cy + r, p)
            }
            MORE -> {
                p.style = Paint.Style.FILL
                val d = context.dpf(2.2f)
                c.drawCircle(cx, cy - r * 0.9f, d, p)
                c.drawCircle(cx, cy, d, p)
                c.drawCircle(cx, cy + r * 0.9f, d, p)
            }
            CHECK -> {
                c.drawLine(cx - r, cy, cx - r * 0.3f, cy + r * 0.7f, p)
                c.drawLine(cx - r * 0.3f, cy + r * 0.7f, cx + r, cy - r * 0.7f, p)
            }
            MOVE -> {
                val s = r * 0.5f
                c.drawLine(cx - r * 1.2f, cy, cx + r * 1.2f, cy, p)
                c.drawLine(cx, cy - r * 1.2f, cx, cy + r * 1.2f, p)
                arrow(c, cx + r * 1.2f, cy, 1f, 0f, s)
                arrow(c, cx - r * 1.2f, cy, -1f, 0f, s)
                arrow(c, cx, cy - r * 1.2f, 0f, -1f, s)
                arrow(c, cx, cy + r * 1.2f, 0f, 1f, s)
            }
            STYLE -> {
                c.drawCircle(cx, cy, r * 1.15f, p)
                p.style = Paint.Style.FILL
                val d = r * 0.24f
                c.drawCircle(cx - r * 0.45f, cy - r * 0.3f, d, p)
                c.drawCircle(cx + r * 0.1f, cy - r * 0.62f, d, p)
                c.drawCircle(cx + r * 0.55f, cy - r * 0.1f, d, p)
                c.drawCircle(cx - r * 0.1f, cy + r * 0.5f, d, p)
            }
            SPARK -> {
                path.reset()
                path.moveTo(cx, cy - r * 1.25f)
                path.quadTo(cx, cy, cx + r * 1.25f, cy)
                path.quadTo(cx, cy, cx, cy + r * 1.25f)
                path.quadTo(cx, cy, cx - r * 1.25f, cy)
                path.quadTo(cx, cy, cx, cy - r * 1.25f)
                c.drawPath(path, p)
                p.style = Paint.Style.FILL
                c.drawCircle(cx + r * 0.95f, cy - r * 0.95f, r * 0.17f, p)
            }
            TEXT -> {
                c.drawLine(cx - r * 1.1f, cy - r * 1.0f, cx + r * 1.1f, cy - r * 1.0f, p)
                c.drawLine(cx, cy - r * 1.0f, cx, cy + r * 1.1f, p)
                c.drawLine(cx - r * 0.45f, cy + r * 1.1f, cx + r * 0.45f, cy + r * 1.1f, p)
            }
            MASK -> {
                // kotak dengan jendela lingkaran di dalamnya
                rect.set(cx - r * 1.2f, cy - r * 1.2f, cx + r * 1.2f, cy + r * 1.2f)
                c.drawRoundRect(rect, r * 0.35f, r * 0.35f, p)
                c.drawCircle(cx, cy, r * 0.62f, p)
                p.style = Paint.Style.FILL
                c.drawCircle(cx, cy, r * 0.3f, p)
            }
            LINK -> {
                // dua mata rantai
                rect.set(cx - r * 1.35f, cy - r * 0.55f, cx + r * 0.35f, cy + r * 0.55f)
                c.drawRoundRect(rect, r * 0.55f, r * 0.55f, p)
                rect.set(cx - r * 0.35f, cy - r * 0.55f, cx + r * 1.35f, cy + r * 0.55f)
                c.drawRoundRect(rect, r * 0.55f, r * 0.55f, p)
            }
            GROUP -> {
                // bingkai putus-putus berisi dua kotak kecil
                rect.set(cx - r * 1.3f, cy - r * 1.3f, cx + r * 1.3f, cy + r * 1.3f)
                p.pathEffect = android.graphics.DashPathEffect(floatArrayOf(r * 0.45f, r * 0.3f), 0f)
                c.drawRoundRect(rect, r * 0.3f, r * 0.3f, p)
                p.pathEffect = null
                rect.set(cx - r * 0.85f, cy - r * 0.85f, cx - r * 0.05f, cy - r * 0.05f)
                c.drawRect(rect, p)
                rect.set(cx + r * 0.05f, cy + r * 0.05f, cx + r * 0.85f, cy + r * 0.85f)
                c.drawRect(rect, p)
            }
            SELECT -> {
                // daftar dengan tanda centang
                path.reset()
                path.moveTo(cx - r * 1.2f, cy - r * 0.7f)
                path.lineTo(cx - r * 0.85f, cy - r * 0.35f)
                path.lineTo(cx - r * 0.25f, cy - r * 1.05f)
                c.drawPath(path, p)
                c.drawLine(cx + r * 0.1f, cy - r * 0.7f, cx + r * 1.25f, cy - r * 0.7f, p)
                path.reset()
                path.moveTo(cx - r * 1.2f, cy + r * 0.85f)
                path.lineTo(cx - r * 0.85f, cy + r * 1.2f)
                path.lineTo(cx - r * 0.25f, cy + r * 0.5f)
                c.drawPath(path, p)
                c.drawLine(cx + r * 0.1f, cy + r * 0.85f, cx + r * 1.25f, cy + r * 0.85f, p)
            }
            STAR -> {
                path.reset()
                for (k in 0 until 10) {
                    val ang = Math.toRadians((-90.0 + k * 36.0))
                    val rr = if (k % 2 == 0) r * 1.3f else r * 0.55f
                    val px = cx + (Math.cos(ang) * rr).toFloat()
                    val py = cy + (Math.sin(ang) * rr).toFloat() + r * 0.1f
                    if (k == 0) path.moveTo(px, py) else path.lineTo(px, py)
                }
                path.close()
                c.drawPath(path, p)
            }
            SPEED -> {
                // speedometer: busur + jarum + titik skala
                rect.set(cx - r * 1.35f, cy - r * 1.35f, cx + r * 1.35f, cy + r * 1.35f)
                c.drawArc(rect, 140f, 260f, false, p)
                c.drawLine(cx, cy + r * 0.15f, cx + r * 0.65f, cy - r * 0.75f, p)
                p.style = Paint.Style.FILL
                c.drawCircle(cx, cy + r * 0.15f, r * 0.22f, p)
                c.drawCircle(cx - r * 0.8f, cy - r * 0.2f, r * 0.1f, p)
                c.drawCircle(cx - r * 0.2f, cy - r * 0.82f, r * 0.1f, p)
                c.drawCircle(cx + r * 0.62f, cy - r * 0.2f, r * 0.1f, p)
            }
            CURVE -> {
                path.reset()
                path.moveTo(cx - r * 1.15f, cy + r * 0.85f)
                path.cubicTo(cx + r * 0.1f, cy + r * 0.85f, cx - r * 0.1f, cy - r * 0.85f, cx + r * 1.15f, cy - r * 0.85f)
                c.drawPath(path, p)
                p.style = Paint.Style.FILL
                c.drawCircle(cx - r * 1.15f, cy + r * 0.85f, r * 0.2f, p)
                c.drawCircle(cx + r * 1.15f, cy - r * 0.85f, r * 0.2f, p)
            }
            SCISSORS -> {
                c.drawLine(cx - r * 0.95f, cy - r * 1.1f, cx + r * 0.55f, cy + r * 0.45f, p)
                c.drawLine(cx + r * 0.95f, cy - r * 1.1f, cx - r * 0.55f, cy + r * 0.45f, p)
                c.drawCircle(cx - r * 0.6f, cy + r * 0.85f, r * 0.38f, p)
                c.drawCircle(cx + r * 0.6f, cy + r * 0.85f, r * 0.38f, p)
            }
            COPY -> {
                rect.set(cx - r * 1.1f, cy - r * 0.55f, cx + r * 0.45f, cy + r * 1.1f)
                c.drawRoundRect(rect, 6f, 6f, p)
                rect.set(cx - r * 0.45f, cy - r * 1.1f, cx + r * 1.1f, cy + r * 0.55f)
                c.drawRoundRect(rect, 6f, 6f, p)
            }
            TRASH -> {
                c.drawLine(cx - r * 1.1f, cy - r * 0.55f, cx + r * 1.1f, cy - r * 0.55f, p)
                c.drawLine(cx - r * 0.4f, cy - r * 0.95f, cx + r * 0.4f, cy - r * 0.95f, p)
                path.reset()
                path.moveTo(cx - r * 0.75f, cy - r * 0.55f)
                path.lineTo(cx - r * 0.6f, cy + r * 1.1f)
                path.lineTo(cx + r * 0.6f, cy + r * 1.1f)
                path.lineTo(cx + r * 0.75f, cy - r * 0.55f)
                c.drawPath(path, p)
            }
            SPLIT -> {
                c.drawLine(cx, cy - r * 1.3f, cx, cy + r * 1.3f, p)
                rect.set(cx - r * 1.3f, cy - r * 0.6f, cx - r * 0.3f, cy + r * 0.6f)
                c.drawRoundRect(rect, 4f, 4f, p)
                rect.set(cx + r * 0.3f, cy - r * 0.6f, cx + r * 1.3f, cy + r * 0.6f)
                c.drawRoundRect(rect, 4f, 4f, p)
            }
            TRIM_FRONT, TRIM_BACK -> {
                c.save()
                if (kind == TRIM_BACK) c.scale(-1f, 1f, cx, cy)
                rect.set(cx - r * 1.3f, cy - r * 0.6f, cx + r * 1.3f, cy + r * 0.6f)
                c.drawRoundRect(rect, 5f, 5f, p)
                p.style = Paint.Style.FILL
                p.alpha = 90
                rect.set(cx - r * 1.3f, cy - r * 0.6f, cx - r * 0.25f, cy + r * 0.6f)
                c.drawRoundRect(rect, 5f, 5f, p)
                p.alpha = 255
                p.style = Paint.Style.STROKE
                c.drawLine(cx - r * 0.25f, cy - r * 1.2f, cx - r * 0.25f, cy + r * 1.2f, p)
                c.restore()
            }
            FULL -> {
                rect.set(cx - r * 1.3f, cy - r * 0.6f, cx + r * 1.3f, cy + r * 0.6f)
                c.drawRoundRect(rect, 5f, 5f, p)
                arrow(c, cx + r * 0.9f, cy, 1f, 0f, r * 0.45f)
                arrow(c, cx - r * 0.9f, cy, -1f, 0f, r * 0.45f)
                c.drawLine(cx - r * 0.9f, cy, cx + r * 0.9f, cy, p)
            }
            SKIP_START, SKIP_END -> {
                c.save()
                if (kind == SKIP_END) c.scale(-1f, 1f, cx, cy)
                c.drawLine(cx - r * 0.95f, cy - r * 0.95f, cx - r * 0.95f, cy + r * 0.95f, p)
                p.style = Paint.Style.FILL
                path.reset()
                path.moveTo(cx + r * 0.95f, cy - r * 1.0f)
                path.lineTo(cx - r * 0.5f, cy)
                path.lineTo(cx + r * 0.95f, cy + r * 1.0f)
                path.close()
                c.drawPath(path, p)
                c.restore()
            }
            FIT -> {
                // empat sudut menjauh dari pusat
                val q = r * 1.15f
                val l = r * 0.55f
                for (sx in intArrayOf(-1, 1)) for (sy in intArrayOf(-1, 1)) {
                    path.reset()
                    path.moveTo(cx + sx * q, cy + sy * (q - l))
                    path.lineTo(cx + sx * q, cy + sy * q)
                    path.lineTo(cx + sx * (q - l), cy + sy * q)
                    c.drawPath(path, p)
                }
            }
            GRID -> {
                val q = r * 1.1f
                rect.set(cx - q, cy - q, cx + q, cy + q)
                c.drawRoundRect(rect, 4f, 4f, p)
                c.drawLine(cx - q / 3, cy - q, cx - q / 3, cy + q, p)
                c.drawLine(cx + q / 3, cy - q, cx + q / 3, cy + q, p)
                c.drawLine(cx - q, cy - q / 3, cx + q, cy - q / 3, p)
                c.drawLine(cx - q, cy + q / 3, cx + q, cy + q / 3, p)
            }
            MINUS -> c.drawLine(cx - r, cy, cx + r, cy, p)
            CLIPBOARD -> {
                rect.set(cx - r * 0.95f, cy - r * 0.9f, cx + r * 0.95f, cy + r * 1.2f)
                c.drawRoundRect(rect, 6f, 6f, p)
                rect.set(cx - r * 0.45f, cy - r * 1.25f, cx + r * 0.45f, cy - r * 0.55f)
                c.drawRoundRect(rect, 4f, 4f, p)
                c.drawLine(cx - r * 0.5f, cy + r * 0.2f, cx + r * 0.5f, cy + r * 0.2f, p)
                c.drawLine(cx - r * 0.5f, cy + r * 0.65f, cx + r * 0.2f, cy + r * 0.65f, p)
            }
            LAYERS -> {
                for (k in -1..1) {
                    path.reset()
                    val y = cy + k * r * 0.7f
                    path.moveTo(cx - r * 1.2f, y)
                    path.lineTo(cx, y - r * 0.55f)
                    path.lineTo(cx + r * 1.2f, y)
                    path.lineTo(cx, y + r * 0.55f)
                    path.close()
                    c.drawPath(path, p)
                }
            }
            CAMERA -> {
                rect.set(cx - r * 1.2f, cy - r * 0.7f, cx + r * 0.6f, cy + r * 0.8f)
                c.drawRoundRect(rect, 5f, 5f, p)
                path.reset()
                path.moveTo(cx + r * 0.6f, cy - r * 0.2f)
                path.lineTo(cx + r * 1.3f, cy - r * 0.65f)
                path.lineTo(cx + r * 1.3f, cy + r * 0.75f)
                path.lineTo(cx + r * 0.6f, cy + r * 0.3f)
                c.drawPath(path, p)
            }
            ROTATE -> {
                rect.set(cx - r * 1.1f, cy - r * 1.1f, cx + r * 1.1f, cy + r * 1.1f)
                c.drawArc(rect, 200f, 250f, false, p)
                val th = Math.toRadians(200.0)
                arrow(c, cx + r * 1.1f * cos(th).toFloat() + sin(th).toFloat() * r * 0.2f,
                    cy + r * 1.1f * sin(th).toFloat() - cos(th).toFloat() * r * 0.2f, sin(th).toFloat(), -cos(th).toFloat(), r * 0.7f)
                rect.set(cx - r * 0.45f, cy - r * 0.45f, cx + r * 0.45f, cy + r * 0.45f)
                c.drawRoundRect(rect, 3f, 3f, p)
            }
            SCALE -> {
                rect.set(cx - r * 1.1f, cy - r * 0.1f, cx + r * 0.1f, cy + r * 1.1f)
                c.drawRoundRect(rect, 3f, 3f, p)
                c.drawLine(cx - r * 0.1f, cy + r * 0.1f, cx + r * 1.1f, cy - r * 1.1f, p)
                c.drawLine(cx + r * 1.1f, cy - r * 1.1f, cx + r * 0.4f, cy - r * 1.1f, p)
                c.drawLine(cx + r * 1.1f, cy - r * 1.1f, cx + r * 1.1f, cy - r * 0.4f, p)
            }
            ANCHOR -> {
                c.drawCircle(cx, cy, r * 0.7f, p)
                p.style = Paint.Style.FILL
                c.drawCircle(cx, cy, r * 0.22f, p)
                p.style = Paint.Style.STROKE
                c.drawLine(cx - r * 1.2f, cy, cx - r * 0.7f, cy, p)
                c.drawLine(cx + r * 0.7f, cy, cx + r * 1.2f, cy, p)
                c.drawLine(cx, cy - r * 1.2f, cx, cy - r * 0.7f, p)
                c.drawLine(cx, cy + r * 0.7f, cx, cy + r * 1.2f, p)
            }
            MORE_H -> {
                p.style = Paint.Style.FILL
                val d = context.dpf(2.2f)
                c.drawCircle(cx - r * 0.9f, cy, d, p)
                c.drawCircle(cx, cy, d, p)
                c.drawCircle(cx + r * 0.9f, cy, d, p)
            }
            UNDO, REDO -> {
                c.save()
                if (kind == REDO) c.scale(-1f, 1f, cx, cy)
                val rr = r * 1.1f
                rect.set(cx - rr, cy - rr * 0.8f, cx + rr, cy + rr * 1.2f)
                c.drawArc(rect, 200f, 240f, false, p)
                // kepala panah di ujung awal busur (menunjuk keluar dari busur)
                val th = Math.toRadians(200.0)
                val ccy = cy + rr * 0.2f
                val px = cx + rr * cos(th).toFloat()
                val py = ccy + rr * sin(th).toFloat()
                val ox = sin(th).toFloat()
                val oy = -cos(th).toFloat()
                arrow(c, px + ox * r * 0.45f, py + oy * r * 0.45f, ox, oy, r * 0.9f)
                c.restore()
            }
        }
    }
}

/** Slider ramping (jalur 3dp, gagang 16dp) – menggantikan SeekBar bawaan yang tebal. Nilai 0..1000. */
class SlimSlider(ctx: Context) : View(ctx) {
    var progress = 0
        set(v) { field = v.coerceIn(0, 1000); invalidate() }
    var onChange: ((Int) -> Unit)? = null
    var onStart: (() -> Unit)? = null
    var onStop: (() -> Unit)? = null
    var dragging = false
        private set
    private val d = ctx.resources.displayMetrics.density
    private val pTrack = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Ui.SURFACE3; strokeCap = Paint.Cap.ROUND; strokeWidth = 3f * d }
    private val pFill = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Ui.ACCENT; strokeCap = Paint.Cap.ROUND; strokeWidth = 3f * d }
    private val pThumb = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.WHITE }
    private val pShadow = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.argb(70, 0, 0, 0) }
    private var downX = 0f
    private var downProg = 0

    private fun x0() = 10f * d
    private fun x1() = width - 10f * d

    override fun onDraw(c: Canvas) {
        val cy = height / 2f
        val x = x0() + (x1() - x0()) * progress / 1000f
        c.drawLine(x0(), cy, x1(), cy, pTrack)
        c.drawLine(x0(), cy, x, cy, pFill)
        val r = (if (dragging) 9f else 7.5f) * d
        c.drawCircle(x, cy + 1f * d, r, pShadow)
        c.drawCircle(x, cy, r, pThumb)
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                parent?.requestDisallowInterceptTouchEvent(true)
                dragging = true
                downX = e.x
                // Ketuk di jalur = lompat ke sana; ketuk di gagang = geser relatif (tidak melompat).
                val cur = x0() + (x1() - x0()) * progress / 1000f
                val jumped = Math.abs(e.x - cur) > 18f * d
                if (jumped) progress = ((e.x - x0()) / (x1() - x0()) * 1000f).toInt().coerceIn(0, 1000)
                downProg = progress
                onStart?.invoke()
                if (jumped) onChange?.invoke(progress)
                invalidate()
            }
            MotionEvent.ACTION_MOVE -> {
                val p = (downProg + (e.x - downX) / (x1() - x0()) * 1000f).toInt().coerceIn(0, 1000)
                if (p != progress) {
                    progress = p
                    onChange?.invoke(p)
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                dragging = false
                onStop?.invoke()
                invalidate()
            }
        }
        return true
    }
}

/** Tombol keyframe berbentuk berlian (digambar, bukan karakter font). */
class DiamondView(ctx: Context) : View(ctx) {
    var keyed = false
        set(v) { field = v; invalidate() }
    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val path = Path()
    init { isClickable = true }
    override fun onDraw(c: Canvas) {
        val cx = width / 2f
        val cy = height / 2f
        val r = minOf(width, height) * 0.2f
        path.reset()
        path.moveTo(cx, cy - r); path.lineTo(cx + r, cy); path.lineTo(cx, cy + r); path.lineTo(cx - r, cy); path.close()
        if (keyed) {
            p.style = Paint.Style.FILL; p.color = Ui.GOLD
        } else {
            p.style = Paint.Style.STROKE; p.strokeWidth = context.dpf(1.4f); p.color = Ui.DIM
        }
        c.drawPath(path, p)
    }
}

/**
 * Baris slider SATU baris: nama · slider ramping · angka · ◆. Padat seperti editor mobile populer.
 * Ketuk angka = ketik nilai; tahan nama = reset; satu gestur geser = satu langkah undo.
 */
class SliderRow(
    private val act: Activity, private val name: String, private val min: Float, private val max: Float,
    private val kind: Int, private val def: Float?, private val onValue: (Float) -> Unit,
    onKey: (() -> Unit)?
) {
    val view = LinearLayout(act)
    var flat = 0
    private val valueTv = TextView(act)
    private val slider = SlimSlider(act)
    private val keyV: DiamondView? = if (onKey != null) DiamondView(act) else null
    private var cur = 0f

    init {
        view.orientation = LinearLayout.HORIZONTAL
        view.gravity = Gravity.CENTER_VERTICAL
        val nameTv = act.label(name, 12f, Ui.DIM)
        nameTv.maxLines = 2
        nameTv.ellipsize = android.text.TextUtils.TruncateAt.END
        if (def != null) nameTv.setOnLongClickListener {
            NativeEngine.nativeGesture(true)
            onValue(def)
            NativeEngine.nativeGesture(false)
            set(def)
            true
        }
        view.addView(nameTv, lp(act.dp(84), -2))
        view.addView(slider, lp(0, act.dp(36), 1f))
        valueTv.textSize = 11.5f
        valueTv.typeface = Ui.MEDIUM
        valueTv.setTextColor(Ui.TEXT)
        valueTv.gravity = Gravity.CENTER
        valueTv.setSingleLine()
        valueTv.setPadding(act.dp(4), act.dp(4), act.dp(4), act.dp(4))
        valueTv.background = act.card(9f, Ui.SURFACE3, Ui.SURFACE2)
        valueTv.setOnClickListener { editValue() }
        view.addView(valueTv, lp(act.dp(56), act.dp(28)).apply { leftMargin = act.dp(4) })
        if (keyV != null) {
            keyV.setOnClickListener { onKey?.invoke() }
            view.addView(keyV, lp(act.dp(32), act.dp(36)))
        } else {
            view.addView(View(act), lp(act.dp(8), 1))
        }
        slider.onStart = { NativeEngine.nativeGesture(true) }
        slider.onStop = { NativeEngine.nativeGesture(false) }
        slider.onChange = { p ->
            val v = fromProg(p)
            cur = v
            valueTv.text = fmt(v)
            onValue(v)
        }
    }

    private fun fromProg(p: Int): Float {
        val v = min + (max - min) * p / 1000f
        return when (kind) {
            1 -> Math.round(v).toFloat()
            3 -> if (p > 500) 1f else 0f
            else -> v
        }
    }

    private fun toProg(v: Float): Int {
        val r = if (max > min) (v - min) / (max - min) else 0f
        return (r.coerceIn(0f, 1f) * 1000f).toInt()
    }

    private fun fmt(v: Float): String = when {
        kind == 1 || kind == 3 -> v.toInt().toString()
        Math.abs(v) >= 100f -> v.toInt().toString()
        else -> String.format("%.2f", v)
    }

    private fun editValue() {
        val et = EditText(act)
        et.inputType = InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_FLAG_DECIMAL or InputType.TYPE_NUMBER_FLAG_SIGNED
        et.setText(fmt(cur))
        et.selectAll()
        et.setTextColor(Ui.TEXT)
        et.textSize = 18f
        et.typeface = Ui.MEDIUM
        et.gravity = Gravity.CENTER
        et.background = act.card(12f)
        et.setPadding(act.dp(16), act.dp(12), act.dp(16), act.dp(12))
        showSheet(act, name, { box, close ->
            box.addView(et, lp(-1, -2))
            val row = LinearLayout(act)
            row.addView(act.button("Batal") { close() }, lp(0, -2, 1f).apply { rightMargin = act.dp(8) })
            row.addView(act.button("Terapkan", 1) {
                val v = et.text.toString().replace(',', '.').toFloatOrNull()
                if (v != null) {
                    NativeEngine.nativeGesture(true)
                    onValue(v)
                    NativeEngine.nativeGesture(false)
                    set(v)
                }
                close()
            }, lp(0, -2, 1f))
            box.addView(row, lp(-1, -2).apply { topMargin = act.dp(12) })
        }, keyboard = true)
    }

    fun set(v: Float) {
        if (slider.dragging) return
        cur = v
        slider.progress = toProg(v)
        valueTv.text = fmt(v)
    }

    fun setKeyed(k: Boolean) {
        keyV?.keyed = k
    }
}

/** Kotak kecil berisi gambar bentuk kurva easing (seperti pilihan preset di CapCut) + nama di bawahnya. */
class CurveTile(ctx: Context, private val kindC: Int, private val x1: Float, private val y1: Float,
                private val x2: Float, private val y2: Float, private val title: String) : View(ctx) {
    private val d = ctx.resources.displayMetrics.density
    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val path = Path()
    init {
        isClickable = true
        background = ctx.card(12f)
    }
    override fun onDraw(c: Canvas) {
        val w = width.toFloat()
        val h = height.toFloat()
        val l = 14f * d
        val r = w - 14f * d
        val t = 10f * d
        val b = h - 24f * d
        fun X(v: Float) = l + (r - l) * v
        fun Y(v: Float) = b - (b - t) * v
        p.style = Paint.Style.STROKE
        p.strokeWidth = 1f
        p.color = Ui.HAIR
        c.drawLine(l, Y(0f), r, Y(0f), p)
        p.strokeWidth = 1.8f * d
        p.strokeCap = Paint.Cap.ROUND
        p.color = Ui.ACCENT_SOFT
        path.reset()
        when (kindC) {
            0 -> { path.moveTo(X(0f), Y(0f)); path.lineTo(X(1f), Y(0f)); path.lineTo(X(1f), Y(1f)) }   // tahan
            1 -> { path.moveTo(X(0f), Y(0f)); path.lineTo(X(1f), Y(1f)) }                            // lurus
            else -> { path.moveTo(X(0f), Y(0f)); path.cubicTo(X(x1), Y(y1), X(x2), Y(y2), X(1f), Y(1f)) }
        }
        c.drawPath(path, p)
        p.style = Paint.Style.FILL
        p.textSize = 10f * d
        p.color = Ui.TEXT
        p.typeface = Ui.MEDIUM
        p.textAlign = Paint.Align.CENTER
        c.drawText(title, w / 2f, h - 8f * d, p)
        p.textAlign = Paint.Align.LEFT
    }
}

// ---------- Bottom sheet & dialog bergaya ----------

/** Lembar dari bawah layar. build() mengisi `box`; close() menutupnya. fullHeight = 88% tinggi layar. */
fun showSheet(act: Activity, title: String, build: (LinearLayout, () -> Unit) -> Unit,
              fullHeight: Boolean = false, keyboard: Boolean = false): Dialog {
    val dlg = Dialog(act)
    dlg.requestWindowFeature(android.view.Window.FEATURE_NO_TITLE)
    val outer = LinearLayout(act)
    outer.orientation = LinearLayout.VERTICAL
    outer.background = act.panelTop(22f)
    outer.setPadding(act.dp(16), act.dp(10), act.dp(16), act.dp(18))

    val grab = View(act)
    grab.background = act.rounded(Ui.SURFACE3, 3f)
    outer.addView(grab, lp(act.dp(40), act.dp(5)).apply { gravity = Gravity.CENTER_HORIZONTAL; bottomMargin = act.dp(10) })

    val head = LinearLayout(act)
    head.gravity = Gravity.CENTER_VERTICAL
    head.addView(act.label(title, 16f, Ui.TEXT, true), lp(0, -2, 1f))
    val x = IconView(act, IconView.CLOSE)
    x.setOnClickListener { dlg.dismiss() }
    head.addView(x, lp(act.dp(36), act.dp(36)))
    outer.addView(head, lp(-1, -2).apply { bottomMargin = act.dp(10) })

    val content = LinearLayout(act)
    content.orientation = LinearLayout.VERTICAL
    build(content) { dlg.dismiss() }
    if (fullHeight) {
        outer.addView(content, lp(-1, 0, 1f))
    } else {
        val sc = ScrollView(act)
        sc.addView(content)
        outer.addView(sc, lp(-1, -2))
    }
    dlg.setContentView(outer)
    val w = dlg.window
    if (w != null) {
        w.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
        w.setGravity(Gravity.BOTTOM)
        val dm = act.resources.displayMetrics
        w.setLayout(ViewGroup.LayoutParams.MATCH_PARENT,
            if (fullHeight) (dm.heightPixels * 0.88f).toInt() else ViewGroup.LayoutParams.WRAP_CONTENT)
        w.setDimAmount(0.55f)
        if (keyboard) w.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE or
            WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE)
    }
    dlg.show()
    return dlg
}

class SheetAction(val icon: Int, val title: String, val desc: String, val danger: Boolean = false,
                  val onClick: () -> Unit)

/** Daftar aksi dengan ikon + judul + penjelasan singkat, dalam lembar dari bawah. */
fun showActions(act: Activity, title: String, rows: List<SheetAction>, note: String = "") {
    showSheet(act, title, { box, close ->
        if (note.isNotEmpty()) {
            val n = act.label(note, 12f, Ui.DIM)
            n.setLineSpacing(0f, 1.15f)
            box.addView(n, lp(-1, -2).apply { bottomMargin = act.dp(10) })
        }
        for (a in rows) {
            val row = LinearLayout(act)
            row.gravity = Gravity.CENTER_VERTICAL
            row.background = act.card(14f)
            row.setPadding(act.dp(10), act.dp(8), act.dp(10), act.dp(8))
            val ic = IconView(act, a.icon, false, true)
            ic.tint = if (a.danger) Ui.DANGER else Ui.ACCENT_SOFT
            ic.isClickable = false
            ic.background = act.rounded(Ui.SURFACE3, 12f)
            row.addView(ic, lp(act.dp(38), act.dp(38)).apply { rightMargin = act.dp(12) })
            val tx = LinearLayout(act)
            tx.orientation = LinearLayout.VERTICAL
            tx.addView(act.label(a.title, 13.5f, if (a.danger) Ui.DANGER else Ui.TEXT, true))
            if (a.desc.isNotEmpty()) tx.addView(act.label(a.desc, 11.5f, Ui.DIM).apply { setPadding(0, act.dp(2), 0, 0) })
            row.addView(tx, lp(0, -2, 1f))
            row.setOnClickListener {
                close()
                a.onClick()
            }
            box.addView(row, lp(-1, -2).apply { bottomMargin = act.dp(8) })
        }
    })
}

/** Pilihan daftar sederhana (mis. mode blend, kanal grafik). */
fun showChoice(act: Activity, title: String, items: List<String>, selected: Int = -1, onPick: (Int) -> Unit) {
    showSheet(act, title, { box, close ->
        items.forEachIndexed { i, t ->
            val row = TextView(act)
            row.text = t
            row.textSize = 13.5f
            row.typeface = Ui.MEDIUM
            row.setTextColor(if (i == selected) Color.WHITE else Ui.TEXT)
            row.gravity = Gravity.CENTER_VERTICAL
            row.minHeight = act.dp(44)
            row.setSingleLine()
            row.ellipsize = android.text.TextUtils.TruncateAt.END
            row.setPadding(act.dp(16), 0, act.dp(16), 0)
            row.background = if (i == selected) act.rounded(Ui.alpha(Ui.ACCENT, 80), 14f, Ui.ACCENT, 1)
            else act.card(12f)
            row.setOnClickListener {
                close()
                onPick(i)
            }
            box.addView(row, lp(-1, -2).apply { bottomMargin = act.dp(6) })
        }
    })
}
