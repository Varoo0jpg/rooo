package com.mystudio.editor

import android.app.Activity
import android.content.Intent
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.Build
import android.os.Bundle
import android.text.format.DateUtils
import android.view.Gravity
import android.view.View
import android.view.WindowInsets
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.HorizontalScrollView
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

/** Layar awal: buat proyek baru (pilih rasio/durasi/fps) atau lanjutkan proyek yang ada. */
class HomeActivity : Activity() {
    private lateinit var content: LinearLayout

    /** Kotak kecil yang menggambar bentuk rasio kanvas (9:16, 16:9, …). */
    private class RatioTile(act: Activity, private val rw: Int, private val rh: Int, private val title: String,
                            private val sub: String) : View(act) {
        var chosen = false
            set(v) { field = v; invalidate() }
        private val d = act.resources.displayMetrics.density
        private val p = Paint(Paint.ANTI_ALIAS_FLAG)
        private val r = RectF()
        override fun onDraw(c: Canvas) {
            val w = width.toFloat()
            val h = height.toFloat()
            val bw: Float
            val bh: Float
            val maxW = w - 36f * d
            val maxH = h - 52f * d
            if (rw.toFloat() / rh > maxW / maxH) { bw = maxW; bh = maxW * rh / rw } else { bh = maxH; bw = maxH * rw / rh }
            r.set((w - bw) / 2, 12f * d, (w + bw) / 2, 12f * d + bh)
            p.style = Paint.Style.FILL
            p.color = if (chosen) Ui.alpha(Ui.ACCENT, 90) else Ui.SURFACE3
            c.drawRoundRect(r, 6f * d, 6f * d, p)
            p.style = Paint.Style.STROKE
            p.strokeWidth = (if (chosen) 2f else 1f) * d
            p.color = if (chosen) Ui.ACCENT else Ui.HAIR
            c.drawRoundRect(r, 6f * d, 6f * d, p)
            p.style = Paint.Style.FILL
            p.textAlign = Paint.Align.CENTER
            p.typeface = Ui.MEDIUM
            p.textSize = 12f * d
            p.color = if (chosen) Color.WHITE else Ui.TEXT
            c.drawText(title, w / 2, h - 22f * d, p)
            p.textSize = 9.5f * d
            p.typeface = Ui.REGULAR
            p.color = Ui.DIM
            c.drawText(sub, w / 2, h - 8f * d, p)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        ProjectStore.migrateLegacy(this)
        val root = LinearLayout(this)
        root.orientation = LinearLayout.VERTICAL
        root.setBackgroundColor(Ui.BG)
        root.setOnApplyWindowInsetsListener { v, ins ->
            if (Build.VERSION.SDK_INT >= 30) {
                val i = ins.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout())
                v.setPadding(i.left, i.top, i.right, i.bottom)
            } else {
                @Suppress("DEPRECATION")
                v.setPadding(ins.systemWindowInsetLeft, ins.systemWindowInsetTop, ins.systemWindowInsetRight, ins.systemWindowInsetBottom)
            }
            ins
        }
        val scroll = ScrollView(this)
        scroll.isVerticalScrollBarEnabled = false
        content = LinearLayout(this)
        content.orientation = LinearLayout.VERTICAL
        content.setPadding(dp(18), dp(20), dp(18), dp(28))
        scroll.addView(content, FrameLayout.LayoutParams(-1, -2))
        root.addView(scroll, lp(-1, 0, 1f))
        setContentView(root)
    }

    override fun onResume() {
        super.onResume()
        render()
    }

    private fun render() {
        content.removeAllViews()
        // Judul
        content.addView(label(AppStrings.HOME_TITLE, 26f, Ui.TEXT, true))
        if (AppStrings.HOME_TAGLINE.isNotEmpty())
            content.addView(label(AppStrings.HOME_TAGLINE, 12f, Ui.DIM).apply { setPadding(0, dp(2), 0, dp(18)) })
        else content.addView(View(this), lp(-1, dp(16)))

        // Kartu "Proyek baru"
        val hero = LinearLayout(this)
        hero.gravity = Gravity.CENTER_VERTICAL
        hero.setPadding(dp(16), dp(16), dp(16), dp(16))
        hero.background = card(20f, Color.rgb(46, 42, 94), Color.rgb(30, 29, 60))
        val plus = IconView(this, IconView.PLUS, true)
        plus.isClickable = false
        hero.addView(plus, lp(dp(48), dp(48)))
        val tx = LinearLayout(this)
        tx.orientation = LinearLayout.VERTICAL
        tx.addView(label(AppStrings.HOME_NEW_PROJECT, 16f, Ui.TEXT, true))
        tx.addView(label(AppStrings.HOME_NEW_PROJECT_HINT, 11.5f, Ui.ACCENT_SOFT).apply { setPadding(0, dp(2), 0, 0) })
        hero.addView(tx, lp(0, -2, 1f).apply { leftMargin = dp(14) })
        hero.setOnClickListener { showNewProject() }
        content.addView(hero, lp(-1, -2))

        // Daftar proyek
        val projects = ProjectStore.list(this)
        content.addView(sectionLabel("Proyek saya").apply { setPadding(dp(2), dp(24), dp(2), dp(8)) })
        if (projects.isEmpty()) {
            val empty = LinearLayout(this)
            empty.orientation = LinearLayout.VERTICAL
            empty.gravity = Gravity.CENTER
            empty.setPadding(dp(20), dp(36), dp(20), dp(36))
            empty.background = card(18f)
            empty.addView(label(AppStrings.HOME_EMPTY_TITLE, 14f, Ui.TEXT, true))
            empty.addView(label(AppStrings.HOME_EMPTY_HINT, 12f, Ui.DIM).apply {
                setPadding(0, dp(4), 0, 0); gravity = Gravity.CENTER })
            content.addView(empty, lp(-1, -2))
            return
        }
        val cols = 2
        val gap = dp(12)
        val cardW = (resources.displayMetrics.widthPixels - dp(36) - gap * (cols - 1)) / cols
        var i = 0
        while (i < projects.size) {
            val row = LinearLayout(this)
            for (k in 0 until cols) {
                val p = projects.getOrNull(i + k)
                if (p != null) row.addView(projectCard(p, cardW), lp(cardW, -2).apply { if (k < cols - 1) rightMargin = gap })
                else row.addView(View(this), lp(cardW, 1))
            }
            content.addView(row, lp(-1, -2).apply { bottomMargin = gap })
            i += cols
        }
    }

    private fun ratioText(w: Int, h: Int): String {
        fun g(a: Int, b: Int): Int = if (b == 0) a else g(b, a % b)
        val k = g(w, h)
        return "${w / k}:${h / k}"
    }

    private fun projectCard(p: ProjectStore.Info, cardW: Int): View {
        val box = LinearLayout(this)
        box.orientation = LinearLayout.VERTICAL
        box.background = card(16f)
        box.setPadding(dp(8), dp(8), dp(8), dp(10))

        val thumbH = (cardW * 1.05f).toInt()
        val frame = FrameLayout(this)
        frame.background = rounded(Color.rgb(14, 15, 20), 11f)
        roundClip(frame, dpf(11f))
        val iv = ImageView(this)
        iv.scaleType = ImageView.ScaleType.FIT_CENTER
        iv.setPadding(dp(8), dp(8), dp(8), dp(8))
        if (p.thumb.exists()) {
            val bmp = BitmapFactory.decodeFile(p.thumb.absolutePath)
            if (bmp != null) iv.setImageBitmap(bmp)
        }
        frame.addView(iv, FrameLayout.LayoutParams(-1, -1))
        if (!p.thumb.exists()) {
            val hint = label(ratioText(p.w, p.h), 13f, Ui.DIM, true)
            hint.gravity = Gravity.CENTER
            frame.addView(hint, FrameLayout.LayoutParams(-1, -1))
        }
        val more = IconView(this, IconView.MORE, false, true)
        more.background = rounded(Color.argb(150, 10, 10, 14), 20f)
        more.setOnClickListener { showProjectMenu(p) }
        frame.addView(more, FrameLayout.LayoutParams(dp(32), dp(32), Gravity.TOP or Gravity.END).apply {
            topMargin = dp(4); rightMargin = dp(4) })
        box.addView(frame, lp(-1, thumbH - dp(16)))

        box.addView(label(p.name, 13f, Ui.TEXT, true).apply {
            setSingleLine(); ellipsize = android.text.TextUtils.TruncateAt.END; setPadding(dp(3), dp(8), dp(3), 0) })
        val whenText = DateUtils.getRelativeTimeSpanString(p.modified, System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS).toString()
        box.addView(label("${ratioText(p.w, p.h)} · $whenText", 10.5f, Ui.DIM).apply {
            setSingleLine(); ellipsize = android.text.TextUtils.TruncateAt.END; setPadding(dp(3), dp(2), dp(3), 0) })
        box.setOnClickListener { openProject(p) }
        return box
    }

    private fun openProject(p: ProjectStore.Info) {
        val i = Intent(this, MainActivity::class.java)
        i.putExtra("path", p.file.absolutePath)
        i.putExtra("id", p.id)
        i.putExtra("name", p.name)
        startActivity(i)
    }

    private fun showProjectMenu(p: ProjectStore.Info) {
        showActions(this, p.name, listOf(
            SheetAction(IconView.CHECK, "Buka", "Lanjutkan mengedit proyek ini") { openProject(p) },
            SheetAction(IconView.MOVE, "Ganti nama", "") { askName("Ganti nama", p.name) { n -> ProjectStore.rename(this, p, n); render() } },
            SheetAction(IconView.COPY, "Gandakan", "Buat salinan proyek ini") { ProjectStore.duplicate(this, p); render() },
            SheetAction(IconView.TRASH, "Hapus", "Hapus permanen proyek ini", true) { confirmDelete(p) }))
    }

    private fun confirmDelete(p: ProjectStore.Info) {
        showSheet(this, "Hapus proyek?", { box, close ->
            box.addView(label("“${p.name}” akan dihapus permanen dan tidak bisa dikembalikan.", 12.5f, Ui.DIM)
                .apply { setPadding(dp(2), 0, dp(2), dp(14)) })
            val row = LinearLayout(this)
            row.addView(button("Batal") { close() }, lp(0, -2, 1f).apply { rightMargin = dp(8) })
            row.addView(button("Hapus", 2) { close(); ProjectStore.delete(this, p); render() }, lp(0, -2, 1f))
            box.addView(row, lp(-1, -2))
        })
    }

    private fun askName(title: String, initial: String, onOk: (String) -> Unit) {
        val et = EditText(this)
        et.setText(initial)
        et.setSingleLine()
        et.selectAll()
        et.setTextColor(Ui.TEXT)
        et.textSize = 15f
        et.background = card(12f)
        et.setPadding(dp(14), dp(12), dp(14), dp(12))
        showSheet(this, title, { box, close ->
            box.addView(et, lp(-1, -2))
            val row = LinearLayout(this)
            row.addView(button("Batal") { close() }, lp(0, -2, 1f).apply { rightMargin = dp(8) })
            row.addView(button("Simpan", 1) {
                val n = et.text.toString().trim()
                if (n.isNotEmpty()) onOk(n)
                close()
            }, lp(0, -2, 1f))
            box.addView(row, lp(-1, -2).apply { topMargin = dp(12) })
        }, keyboard = true)
    }

    private fun showNewProject() {
        data class Ratio(val w: Int, val h: Int, val title: String, val sub: String)
        val ratios = listOf(
            Ratio(1080, 1920, "9:16", "TikTok · Reels"),
            Ratio(1920, 1080, "16:9", "YouTube"),
            Ratio(1080, 1080, "1:1", "Persegi"),
            Ratio(1080, 1350, "4:5", "Feed"))
        val fpsList = listOf(24, 30, 60)
        var rSel = 0
        var fSel = 1
        val nameEt = EditText(this)
        showSheet(this, "Proyek baru", { box, close ->
            nameEt.setText(ProjectStore.nextDefaultName(this))
            nameEt.setSingleLine()
            nameEt.setTextColor(Ui.TEXT)
            nameEt.textSize = 14f
            nameEt.background = card(12f)
            nameEt.setPadding(dp(14), dp(10), dp(14), dp(10))
            box.addView(nameEt, lp(-1, -2))

            box.addView(sectionLabel("Rasio layar"))
            val sc = HorizontalScrollView(this)
            sc.isHorizontalScrollBarEnabled = false
            val tiles = ArrayList<RatioTile>()
            val rr = LinearLayout(this)
            ratios.forEachIndexed { i, r ->
                val t = RatioTile(this, r.w, r.h, r.title, r.sub)
                t.chosen = i == rSel
                t.setOnClickListener { rSel = i; tiles.forEachIndexed { k, x -> x.chosen = k == rSel } }
                tiles.add(t)
                rr.addView(t, lp(dp(84), dp(112)).apply { rightMargin = dp(8) })
            }
            sc.addView(rr)
            box.addView(sc, lp(-1, -2))

            fun chipRow(title: String, items: List<String>, sel: Int, onSel: (Int) -> Unit) {
                box.addView(sectionLabel(title))
                val row = LinearLayout(this)
                val chips = ArrayList<TextView>()
                items.forEachIndexed { i, t ->
                    val c = chip(t, i == sel) { onSel(i); chips.forEachIndexed { k, x -> styleChip(x, k == i) } }
                    chips.add(c)
                    row.addView(c, lp(0, dp(36), 1f).apply { rightMargin = dp(8) })
                }
                box.addView(row, lp(-1, -2))
            }
            chipRow("Frame rate", fpsList.map { "$it fps" }, fSel) { fSel = it }

            box.addView(button("Buat & mulai", 1) {
                val r = ratios[rSel]
                val name = nameEt.text.toString().trim().ifEmpty { ProjectStore.nextDefaultName(this) }
                val info = ProjectStore.create(this, name, r.w, r.h, fpsList[fSel], 1)
                close()
                if (info != null) openProject(info)
            }, lp(-1, -2).apply { topMargin = dp(18) })
        }, keyboard = false)
    }
}
