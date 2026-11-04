package com.mystudio.editor

import android.app.Activity
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.Outline
import android.os.Handler
import android.os.Looper
import android.text.Editable
import android.text.TextWatcher
import android.view.Gravity
import android.view.View
import android.view.ViewOutlineProvider
import android.widget.EditText
import android.widget.HorizontalScrollView
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.FrameLayout
import android.widget.ScrollView
import android.widget.TextView

/** Gambar contoh efek (dirender GPU oleh engine) – dibuat sekali, disimpan sebagai Bitmap. */
object Thumbs {
    const val SIZE = 180
    private val cache = HashMap<String, Bitmap>()
    private var requested = false

    fun request() {
        if (!requested) {
            requested = true
            NativeEngine.nativeRequestThumbs()
        }
    }

    fun ready(): Boolean = NativeEngine.nativeThumbsReady()

    /** Bitmap contoh untuk efek `type` ("none" = asli); null jika belum siap. */
    fun get(type: String): Bitmap? {
        cache[type]?.let { return it }
        if (!ready()) return null
        val raw = NativeEngine.nativeThumb(type)
        if (raw.size != SIZE * SIZE * 4) return null
        val px = IntArray(SIZE * SIZE)
        for (i in px.indices) {
            val r = raw[i * 4].toInt() and 0xFF
            val g = raw[i * 4 + 1].toInt() and 0xFF
            val b = raw[i * 4 + 2].toInt() and 0xFF
            px[i] = Color.rgb(r, g, b)
        }
        val bmp = Bitmap.createBitmap(px, SIZE, SIZE, Bitmap.Config.ARGB_8888)
        cache[type] = bmp
        return bmp
    }
}

fun roundClip(v: View, radiusPx: Float) {
    v.outlineProvider = object : ViewOutlineProvider() {
        override fun getOutline(view: View, outline: Outline) {
            outline.setRoundRect(0, 0, view.width, view.height, radiusPx)
        }
    }
    v.clipToOutline = true
}

/** Gambar kecil contoh efek (bulat sudut). Mengisi bitmap otomatis begitu siap. */
fun thumbView(act: Activity, type: String, sizeDp: Int): ImageView {
    val iv = ImageView(act)
    iv.scaleType = ImageView.ScaleType.CENTER_CROP
    iv.setBackgroundColor(Ui.SURFACE3)
    roundClip(iv, act.dpf(12f))
    val bmp = Thumbs.get(type)
    if (bmp != null) iv.setImageBitmap(bmp)
    else {
        val h = Handler(Looper.getMainLooper())
        val r = object : Runnable {
            override fun run() {
                if (!iv.isAttachedToWindow && iv.parent == null) return
                val b = Thumbs.get(type)
                if (b != null) iv.setImageBitmap(b) else h.postDelayed(this, 300)
            }
        }
        h.postDelayed(r, 300)
    }
    iv.layoutParams = LinearLayout.LayoutParams(act.dp(sizeDp), act.dp(sizeDp))
    return iv
}

private val suggestedTypes = listOf("glow", "gaussian_blur", "rgb_split", "glitch", "wave", "vignette",
    "drop_shadow", "kaleidoscope", "hue_saturation", "pixelate")

/**
 * Katalog efek: layar awal = kotak kategori (latar contoh efek) + baris "Disarankan"; ketuk kategori untuk masuk
 * ke grid efeknya (tombol kembali di kiri atas). Mengetik di pencarian menampilkan hasil dari semua kategori.
 */
fun showEffectPicker(act: Activity, defs: Collection<FxDef>, onPick: (String) -> Unit) {
    Thumbs.request()
    val handler = Handler(Looper.getMainLooper())
    var query = ""
    var category = ""
    var openCat = ""
    lateinit var body: LinearLayout
    lateinit var header: LinearLayout
    var fill: () -> Unit = {}

    val cats = defs.map { it.category }.distinct()
    val dm = act.resources.displayMetrics
    val gap = act.dp(10)
    val avail = dm.widthPixels - act.dp(32)

    showSheet(act, "Tambah efek", { box, close ->
        val search = EditText(act)
        search.hint = "Cari efek…"
        search.setSingleLine()
        search.setTextColor(Ui.TEXT)
        search.setHintTextColor(Ui.DIM)
        search.textSize = 13f
        search.background = act.card(14f)
        search.setPadding(act.dp(16), act.dp(10), act.dp(16), act.dp(10))
        box.addView(search, lp(-1, -2).apply { bottomMargin = act.dp(8) })

        header = LinearLayout(act)
        header.gravity = Gravity.CENTER_VERTICAL
        box.addView(header, lp(-1, -2))

        val scroll = ScrollView(act)
        body = LinearLayout(act)
        body.orientation = LinearLayout.VERTICAL
        scroll.addView(body)
        box.addView(scroll, lp(-1, 0, 1f))

        fun effectCell(d: FxDef, cardW: Int): View {
            val cell = LinearLayout(act)
            cell.orientation = LinearLayout.VERTICAL
            val iv = thumbView(act, d.type, 0)
            iv.layoutParams = LinearLayout.LayoutParams(cardW, cardW)
            cell.addView(iv)
            val t = act.label(d.label, 11.5f, Ui.TEXT, true)
            t.maxLines = 2
            t.ellipsize = android.text.TextUtils.TruncateAt.END
            t.gravity = Gravity.CENTER_HORIZONTAL
            t.setPadding(0, act.dp(5), 0, 0)
            cell.addView(t, lp(cardW, act.dp(36)))
            cell.setOnClickListener {
                close()
                onPick(d.type)
            }
            return cell
        }

        fun effectGrid(list: List<FxDef>) {
            val cols = 3
            val cardW = (avail - gap * (cols - 1)) / cols
            var i = 0
            while (i < list.size) {
                val row = LinearLayout(act)
                for (k in 0 until cols) {
                    val d = list.getOrNull(i + k)
                    val cell: View = if (d != null) effectCell(d, cardW) else View(act)
                    row.addView(cell, lp(cardW, -2).apply { if (k < cols - 1) rightMargin = gap })
                }
                body.addView(row, lp(-1, -2).apply { bottomMargin = act.dp(8) })
                i += cols
            }
        }

        fun categoryBox(cat: String, w: Int): View {
            val list = defs.filter { it.category == cat }
            val fl = FrameLayout(act)
            roundClip(fl, act.dpf(16f))
            val iv = thumbView(act, list.first().type, 0)
            fl.addView(iv, FrameLayout.LayoutParams(-1, -1))
            val shade = View(act)
            shade.background = android.graphics.drawable.GradientDrawable(
                android.graphics.drawable.GradientDrawable.Orientation.TOP_BOTTOM,
                intArrayOf(Color.argb(40, 0, 0, 0), Color.argb(215, 0, 0, 0)))
            fl.addView(shade, FrameLayout.LayoutParams(-1, -1))
            val tx = LinearLayout(act)
            tx.orientation = LinearLayout.VERTICAL
            tx.setPadding(act.dp(12), 0, act.dp(10), act.dp(9))
            tx.addView(act.label(cat, 14f, Color.WHITE, true))
            tx.addView(act.label("${list.size} efek", 10.5f, Color.argb(190, 255, 255, 255)))
            fl.addView(tx, FrameLayout.LayoutParams(-1, -2, Gravity.BOTTOM))
            fl.setOnClickListener { category = cat; fill() }
            return fl
        }

        fun home() {
            // Disarankan
            val sug = suggestedTypes.mapNotNull { t -> defs.firstOrNull { it.type == t } }
            if (sug.isNotEmpty()) {
                body.addView(act.sectionLabel("Disarankan"))
                val hs = HorizontalScrollView(act)
                hs.isHorizontalScrollBarEnabled = false
                val row = LinearLayout(act)
                val w = act.dp(92)
                for (d in sug) row.addView(effectCell(d, w), lp(w, -2).apply { rightMargin = gap })
                hs.addView(row)
                body.addView(hs, lp(-1, -2).apply { bottomMargin = act.dp(6) })
            }
            body.addView(act.sectionLabel("Kategori"))
            // Gaya Alight Motion: tiap kategori = baris yang bisa dibuka, isinya deretan gambar contoh efek
            for (c in cats) {
                val list = defs.filter { it.category == c }
                val open = openCat == c
                val card = LinearLayout(act)
                card.orientation = LinearLayout.VERTICAL
                card.background = act.card(14f)
                val head = LinearLayout(act)
                head.gravity = Gravity.CENTER_VERTICAL
                head.setPadding(act.dp(10), act.dp(10), act.dp(12), act.dp(10))
                val arrow = IconView(act, if (open) IconView.CHEV_DOWN else IconView.CHEV_RIGHT, false, true)
                arrow.isClickable = false
                head.addView(arrow, lp(act.dp(26), act.dp(26)))
                head.addView(act.label(c, 14f, Ui.TEXT, true), lp(0, -2, 1f).apply { leftMargin = act.dp(6) })
                head.addView(act.label("${list.size}", 11f, Ui.DIM))
                head.setOnClickListener { openCat = if (open) "" else c; fill() }
                card.addView(head, lp(-1, -2))
                if (open) {
                    val hs = HorizontalScrollView(act)
                    hs.isHorizontalScrollBarEnabled = false
                    val row = LinearLayout(act)
                    row.setPadding(act.dp(10), 0, act.dp(10), act.dp(8))
                    val w = act.dp(92)
                    for (d in list) row.addView(effectCell(d, w), lp(w, -2).apply { rightMargin = gap })
                    hs.addView(row)
                    card.addView(hs, lp(-1, -2))
                }
                body.addView(card, lp(-1, -2).apply { bottomMargin = act.dp(8) })
            }
        }

        fill = {
            body.removeAllViews()
            header.removeAllViews()
            val q = query.trim().lowercase()
            if (q.isNotEmpty()) {
                val hits = defs.filter { it.label.lowercase().contains(q) || it.category.lowercase().contains(q) || it.type.contains(q) }
                if (hits.isEmpty()) body.addView(act.label("Tidak ada efek yang cocok.", 13f, Ui.DIM)
                    .apply { setPadding(act.dp(4), act.dp(20), 0, 0) })
                else effectGrid(hits)
            } else if (category.isNotEmpty()) {
                val back = IconView(act, IconView.CHEV_LEFT, false, true)
                back.setOnClickListener { category = ""; fill() }
                header.addView(back, lp(act.dp(36), act.dp(36)).apply { leftMargin = act.dp(-8) })
                header.addView(act.label(category, 15f, Ui.TEXT, true))
                effectGrid(defs.filter { it.category == category })
            } else home()
        }

        search.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, st: Int, c: Int, a: Int) {}
            override fun onTextChanged(s: CharSequence?, st: Int, b: Int, c: Int) {}
            override fun afterTextChanged(s: Editable?) {
                query = s?.toString() ?: ""
                fill()
            }
        })
        fill()
        // Contoh efek dibuat engine di frame render berikutnya; segarkan begitu siap.
        if (!Thumbs.ready()) {
            val poll = object : Runnable {
                override fun run() {
                    if (Thumbs.ready()) fill() else handler.postDelayed(this, 250)
                }
            }
            handler.postDelayed(poll, 250)
        }
    }, fullHeight = true)
}
