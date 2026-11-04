package com.mystudio.editor

import android.app.Activity
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.text.InputType
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView

/** Dialog ketik angka (dipakai kotak angka & baris parameter). */
fun promptNumber(act: Activity, title: String, cur: Float, onOk: (Float) -> Unit) {
    val et = EditText(act)
    et.inputType = InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_FLAG_DECIMAL or InputType.TYPE_NUMBER_FLAG_SIGNED
    et.setText(if (Math.abs(cur) >= 100f) cur.toInt().toString() else String.format("%.2f", cur))
    et.selectAll()
    et.setTextColor(Ui.TEXT)
    et.textSize = 18f
    et.typeface = Ui.MEDIUM
    et.gravity = Gravity.CENTER
    et.background = act.card(12f)
    et.setPadding(act.dp(16), act.dp(12), act.dp(16), act.dp(12))
    showSheet(act, title, { box, close ->
        box.addView(et, lp(-1, -2))
        val row = LinearLayout(act)
        row.addView(act.button("Batal") { close() }, lp(0, -2, 1f).apply { rightMargin = act.dp(8) })
        row.addView(act.button("Terapkan", 1) {
            val v = et.text.toString().replace(',', '.').toFloatOrNull()
            if (v != null) onOk(v)
            close()
        }, lp(0, -2, 1f))
        box.addView(row, lp(-1, -2).apply { topMargin = act.dp(12) })
    }, keyboard = true)
}

/**
 * Penggaris geser ala Alight Motion: garis tengah hijau tetap, skala bergerak mengikuti jari.
 * Hanya bereaksi saat [active] (komponen sudah difokuskan); jika tidak, sentuhan diteruskan ke baris induk.
 */
class RulerView(ctx: Context) : View(ctx) {
    var active = false
        set(v) { field = v; invalidate() }
    var minV = 0f
    var maxV = 1f
    /** Perubahan nilai per dp geseran. */
    var unitPerDp = 0.01f
    var value = 0f
        private set
    var onStart: () -> Unit = {}
    var onStop: () -> Unit = {}
    var onChange: (Float) -> Unit = {}
    private var lastX = 0f
    private var dragging = false
    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val d = ctx.resources.displayMetrics.density

    fun setValueSilent(v: Float) {
        if (dragging) return
        value = v
        invalidate()
    }

    fun setRange(mn: Float, mx: Float, perDp: Float) {
        minV = mn; maxV = mx; unitPerDp = perDp
    }

    override fun onDraw(c: Canvas) {
        val w = width.toFloat()
        val h = height.toFloat()
        val spacing = 8f * d
        val off = ((value / unitPerDp) * d) % (spacing * 5f)
        p.strokeWidth = 1.2f * d
        val cx = w / 2f
        var k = -((cx / spacing).toInt() + 6)
        val maxK = (cx / spacing).toInt() + 6
        while (k <= maxK) {
            val x = cx + k * spacing + off
            if (x >= 0f && x <= w) {
                val major = ((k % 5) + 5) % 5 == 0
                val dist = Math.abs(x - cx) / (w / 2f)
                val a = ((1f - dist * 0.85f) * (if (active) 210f else 90f)).toInt().coerceIn(20, 255)
                p.color = Color.argb(a, 200, 205, 225)
                val len = h * (if (major) 0.62f else 0.38f)
                c.drawLine(x, (h - len) / 2f, x, (h + len) / 2f, p)
            }
            k++
        }
        p.strokeWidth = 2f * d
        p.color = if (active) Color.rgb(110, 235, 160) else Color.argb(110, 200, 205, 225)
        c.drawLine(cx, h * 0.08f, cx, h * 0.92f, p)
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        if (!active) return false
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                parent?.requestDisallowInterceptTouchEvent(true)
                dragging = true
                lastX = e.x
                onStart()
            }
            MotionEvent.ACTION_MOVE -> if (dragging) {
                val dx = (e.x - lastX) / d
                lastX = e.x
                value = (value + dx * unitPerDp).coerceIn(minV, maxV)
                onChange(value)
                invalidate()
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> if (dragging) {
                dragging = false
                onStop()
            }
        }
        return true
    }
}

/** Kotak angka kecil dengan nama di bawahnya. Ketuk = fokus; ketuk lagi saat fokus = ketik angka. */
class NumBox(private val act: Activity, private val name: String) : LinearLayout(act) {
    private val valueTv = TextView(act)
    private val nameTv = TextView(act)
    var cur = 0f
        private set

    init {
        orientation = VERTICAL
        gravity = Gravity.CENTER
        valueTv.textSize = 13f
        valueTv.typeface = Ui.MEDIUM
        valueTv.gravity = Gravity.CENTER
        valueTv.setSingleLine()
        valueTv.background = act.card(10f, Ui.SURFACE3, Ui.SURFACE2)
        valueTv.setPadding(act.dp(6), act.dp(8), act.dp(6), act.dp(8))
        nameTv.textSize = 9.5f
        nameTv.setTextColor(Ui.DIM)
        nameTv.gravity = Gravity.CENTER
        nameTv.text = name.uppercase()
        addView(valueTv, lp(-1, -2))
        addView(nameTv, lp(-1, -2).apply { topMargin = act.dp(2) })
        setFocused(false)
    }

    fun setValue(v: Float, suffix: String = "") {
        cur = v
        valueTv.text = (if (Math.abs(v) >= 100f) String.format("%.1f", v) else String.format("%.2f", v)) + suffix
    }

    fun setFocused(f: Boolean) {
        valueTv.setTextColor(if (f) Color.rgb(110, 235, 160) else Ui.TEXT)
    }
}

/** Bidang sentuh: seret di dalamnya melaporkan perpindahan jari (px). */
class PadView(ctx: Context) : View(ctx) {
    var onStart: () -> Unit = {}
    var onStop: () -> Unit = {}
    var onDrag: (Float, Float) -> Unit = { _, _ -> }
    private var lx = 0f
    private var ly = 0f
    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val d = ctx.resources.displayMetrics.density

    override fun onDraw(c: Canvas) {
        // sudut siku di empat pojok sebagai penanda area
        p.style = Paint.Style.STROKE
        p.strokeWidth = 1.6f * d
        p.color = Color.argb(150, 200, 205, 225)
        val m = 4f * d
        val l = 14f * d
        val w = width.toFloat()
        val h = height.toFloat()
        c.drawLine(m, m, m + l, m, p); c.drawLine(m, m, m, m + l, p)
        c.drawLine(w - m, m, w - m - l, m, p); c.drawLine(w - m, m, w - m, m + l, p)
        c.drawLine(m, h - m, m + l, h - m, p); c.drawLine(m, h - m, m, h - m - l, p)
        c.drawLine(w - m, h - m, w - m - l, h - m, p); c.drawLine(w - m, h - m, w - m, h - m - l, p)
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                parent?.requestDisallowInterceptTouchEvent(true)
                lx = e.x; ly = e.y
                onStart()
            }
            MotionEvent.ACTION_MOVE -> {
                onDrag(e.x - lx, e.y - ly)
                lx = e.x; ly = e.y
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> onStop()
        }
        return true
    }
}

/** Cincin putar: seret mengelilingi pusat; melaporkan selisih sudut (derajat, tidak dibatasi 360). */
class DialView(ctx: Context) : View(ctx) {
    var onStart: () -> Unit = {}
    var onStop: () -> Unit = {}
    var onDelta: (Float) -> Unit = {}
    var angle = 0f
        set(v) { field = v; invalidate() }
    private var last = 0.0
    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val d = ctx.resources.displayMetrics.density
    private val rc = RectF()

    private fun ang(e: MotionEvent): Double =
        Math.toDegrees(Math.atan2((e.y - height / 2f).toDouble(), (e.x - width / 2f).toDouble()))

    override fun onDraw(c: Canvas) {
        val cx = width / 2f
        val cy = height / 2f
        val r = minOf(width, height) / 2f - 14f * d
        p.style = Paint.Style.STROKE
        p.strokeWidth = 2f * d
        p.color = Color.argb(120, 200, 205, 225)
        c.drawCircle(cx, cy, r, p)
        val th = Math.toRadians(angle.toDouble())
        p.style = Paint.Style.FILL
        p.color = Color.WHITE
        c.drawCircle(cx + r * Math.cos(th).toFloat(), cy + r * Math.sin(th).toFloat(), 11f * d, p)
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                parent?.requestDisallowInterceptTouchEvent(true)
                last = ang(e)
                onStart()
            }
            MotionEvent.ACTION_MOVE -> {
                val a = ang(e)
                var dlt = a - last
                while (dlt > 180) dlt -= 360
                while (dlt < -180) dlt += 360
                last = a
                onDelta(dlt.toFloat())
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> onStop()
        }
        return true
    }
}

/**
 * Satu baris parameter animasi ala Alight Motion: [nama] [penggaris] [nilai].
 * Ketuk baris = fokus (keyframe & kurva di rail kiri berlaku untuknya); baru setelah fokus penggaris bisa digeser.
 */
class ParamRow(
    private val act: Activity, private var name: String, private val min: Float, private val max: Float,
    private val kind: Int, private val def: Float?, val channel: Int,
    private val onFocus: (Int) -> Unit, private val onValue: (Float) -> Unit
) {
    val view = LinearLayout(act)
    var flat = 0
    private val nameTv = TextView(act)
    private val valueTv = TextView(act)
    private val ruler = RulerView(act)
    private var cur = 0f
    private var focused = false
    private var keyed = false
    private var swChip: TextView? = null

    init {
        view.orientation = LinearLayout.HORIZONTAL
        view.gravity = Gravity.CENTER_VERTICAL
        nameTv.textSize = 12f
        nameTv.gravity = Gravity.CENTER
        nameTv.maxLines = 2
        nameTv.ellipsize = android.text.TextUtils.TruncateAt.END
        nameTv.setPadding(act.dp(6), act.dp(4), act.dp(6), act.dp(4))
        view.addView(nameTv, lp(act.dp(92), act.dp(52)))
        if (kind == 3) {
            ruler.visibility = View.GONE
            val sw = act.chip("", false) {}
            sw.setOnClickListener {
                if (!focused) onFocus(channel)
                val nv = if (cur > 0.5f) 0f else 1f
                NativeEngine.nativeGesture(true); onValue(nv); NativeEngine.nativeGesture(false)
                set(nv)
            }
            swChip = sw
            view.addView(sw, lp(0, act.dp(36), 1f).apply { leftMargin = act.dp(8); rightMargin = act.dp(8) })
        } else {
            ruler.setRange(min, max, ((max - min) / 260f).coerceAtLeast(1e-4f) * (if (kind == 1) 1f else 1f))
            if (kind == 1) ruler.unitPerDp = ((max - min) / 140f).coerceAtLeast(0.05f)
            ruler.onStart = { NativeEngine.nativeGesture(true) }
            ruler.onStop = { NativeEngine.nativeGesture(false) }
            ruler.onChange = { v ->
                val nv = if (kind == 1) Math.round(v).toFloat() else v
                cur = nv
                valueTv.text = fmt(nv)
                onValue(nv)
            }
            view.addView(ruler, lp(0, act.dp(52), 1f).apply { leftMargin = act.dp(4) })
        }
        valueTv.textSize = 11.5f
        valueTv.typeface = Ui.MEDIUM
        valueTv.setTextColor(Ui.TEXT)
        valueTv.gravity = Gravity.CENTER
        valueTv.setSingleLine()
        valueTv.setPadding(act.dp(4), act.dp(4), act.dp(4), act.dp(4))
        view.addView(valueTv, lp(act.dp(62), act.dp(52)).apply { leftMargin = act.dp(4) })
        view.setOnClickListener { onFocus(channel) }
        nameTv.setOnClickListener { onFocus(channel) }
        nameTv.setOnLongClickListener {
            if (def != null) {
                NativeEngine.nativeGesture(true); onValue(def); NativeEngine.nativeGesture(false); set(def)
            }
            true
        }
        valueTv.setOnClickListener {
            if (focused) promptNumber(act, name, cur) { v ->
                val nv = v.coerceIn(min - Math.abs(max - min) * 4f, max + Math.abs(max - min) * 4f)
                NativeEngine.nativeGesture(true); onValue(nv); NativeEngine.nativeGesture(false); set(nv)
            } else onFocus(channel)
        }
        applyLook()
    }

    private fun fmt(v: Float): String = when {
        kind == 1 || kind == 3 -> v.toInt().toString()
        Math.abs(v) >= 100f -> v.toInt().toString()
        else -> String.format("%.2f", v)
    }

    fun rename(n: String) {
        if (n != name) { name = n; applyLook() }
    }

    private fun applyLook() {
        view.background = if (focused) act.rounded(Ui.alpha(Color.WHITE, 14), 12f) else null
        nameTv.background = act.rounded(if (focused) Color.rgb(18, 18, 24) else Ui.alpha(Color.WHITE, 10), 12f)
        nameTv.setTextColor(if (focused) Color.rgb(110, 235, 160) else Ui.TEXT)
        nameTv.text = name + if (keyed) "  ◆" else ""
        ruler.active = focused
        valueTv.setTextColor(if (focused) Color.rgb(110, 235, 160) else Ui.TEXT)
        swChip?.let { it.text = if (cur > 0.5f) "Aktif" else "Mati"; styleChip(it, cur > 0.5f) }
    }

    fun setFocused(f: Boolean) {
        if (f == focused) return
        focused = f
        applyLook()
    }

    fun set(v: Float) {
        cur = v
        ruler.setValueSilent(v)
        valueTv.text = fmt(v)
        if (kind == 3) applyLook()
    }

    fun setKeyed(k: Boolean) {
        if (k == keyed) return
        keyed = k
        applyLook()
    }
}
