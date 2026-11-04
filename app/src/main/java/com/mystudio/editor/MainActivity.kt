package com.mystudio.editor

import android.app.Activity
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Intent
import android.graphics.BitmapFactory
import android.graphics.Color
import android.graphics.Typeface
import android.net.Uri
import android.opengl.GLES30
import android.opengl.GLSurfaceView
import android.opengl.GLUtils
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.graphics.Bitmap
import android.graphics.Canvas
import android.text.InputType
import android.text.Layout
import android.text.SpannableString
import android.text.StaticLayout
import android.text.TextPaint
import android.widget.EditText
import android.text.Spanned
import android.text.style.ForegroundColorSpan
import android.text.style.RelativeSizeSpan
import android.view.Gravity
import android.view.ViewGroup
import android.view.MotionEvent
import android.view.View
import android.view.WindowInsets
import android.widget.FrameLayout
import android.view.Choreographer
import android.widget.HorizontalScrollView
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import java.io.File
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class MainActivity : Activity() {

    private var catalog = LinkedHashMap<String, FxDef>()

    // Kerangka
    private lateinit var glView: GLSurfaceView
    private lateinit var snack: TextView
    private lateinit var statusTv: TextView
    private lateinit var undoBtn: IconView
    private lateinit var redoBtn: IconView
    private lateinit var playBtn: IconView
    private lateinit var playHolder: FrameLayout
    private lateinit var timeTv: TextView
    private lateinit var timeline: TimelineView
    private lateinit var gridPanel: LinearLayout
    private lateinit var topBarView: View
    private var fullPreview = false
    private lateinit var zoomTv: TextView
    private lateinit var gridBtn: IconView
    private lateinit var sheet: LinearLayout
    private lateinit var sheetTitle: TextView
    private lateinit var sheetSub: TextView
    private lateinit var emptyMsg: TextView
    private val pages = ArrayList<View>()
    private var currentTab = -1
    private var playingShown = false
    private val handler = Handler(Looper.getMainLooper())
    private var lastNote = ""
    private var lastLayerId = -1
    private var lastState: LState? = null
    private var currentChannel = 0
    private var projectId: String? = null
    private var projectName = "Proyek"
    private var fps = 30
    private var sheetH = 0

    // Strip alat
    private lateinit var shapeBtn: View
    private lateinit var shapeLabel: TextView
    private lateinit var rowBlend: LinearLayout     // baris tombol Warna/Blend/Efek
    private lateinit var rowTransform: LinearLayout // baris Transform/Bentuk/Gandakan
    private lateinit var rowAudio: LinearLayout     // baris khusus layer suara
    private lateinit var layerBar: LinearLayout     // bilah atas saat ada layer terpilih (gaya Alight Motion)
    private lateinit var layerNameTv: TextView
    private lateinit var layerSubTv: TextView
    private lateinit var parentIcon: IconView
    private lateinit var multiTop: LinearLayout     // bilah hijau saat mode Pilih banyak
    private lateinit var multiCountTv: TextView
    private var multiShown = false
    private lateinit var styleLabel: TextView
    private lateinit var audioPlayer: AudioPlayer

    // Tab Transform
    private val transformIds = intArrayOf(0, 1, 2, 3, 4, 5, 7, 8)
    private val chNames = mapOf(0 to "Posisi X", 1 to "Posisi Y", 2 to "Posisi Z", 3 to "Skala X", 4 to "Skala Y",
        5 to "Rotasi", 6 to "Opasitas", 7 to "Pusat X", 8 to "Pusat Y")
    private var tSub = 0
    private val tSubViews = ArrayList<View>()
    private val tSubBtns = ArrayList<IconView>()
    private val moveBoxes = ArrayList<NumBox>()
    private val scaleBoxes = ArrayList<NumBox>()
    private val anchorBoxes = ArrayList<NumBox>()
    private lateinit var rotBox: NumBox
    private lateinit var dial: DialView
    private lateinit var scaleRuler: RulerView
    private lateinit var anchorRuler: RulerView
    private var scaleLock = true
    private lateinit var railMore: IconView
    private val TIME_CH = 10   // kanal Waktu (kurva kecepatan)

    // Rail samping panel
    private lateinit var railKey: DiamondView
    private lateinit var railCurve: IconView
    private lateinit var sheetScroll: ScrollView
    private var prevTab = -1
    private lateinit var opacityRow: ParamRow
    private lateinit var blendBox: LinearLayout
    private var blendOpen = "Normal"
    private var blendSig = -2

    // Tab Gaya
    private lateinit var colorSection: LinearLayout
    private lateinit var shapeSection: LinearLayout
    private lateinit var blendSection: LinearLayout
    private val shapeChips = ArrayList<TextView>()
    private lateinit var cornerRow: SliderRow
    private lateinit var sidesRow: SliderRow
    private lateinit var innerRow: SliderRow

    // Tab Efek
    private lateinit var fxList: LinearLayout
    private var fxSignature = ""
    private val fxRows = ArrayList<Pair<Int, ParamRow>>()
    private val collapsed = HashSet<String>()

    // Tab Grafik
    private lateinit var graph: GraphView
    private lateinit var channelBtn: TextView

    private val blendNames = listOf("Normal", "Add", "Multiply", "Screen", "Overlay", "Darken", "Lighten",
        "Color Dodge", "Color Burn", "Hard Light", "Soft Light", "Difference", "Exclusion",
        "Linear Burn", "Darker Color", "Lighter Color", "Vivid Light", "Linear Light", "Pin Light", "Hard Mix",
        "Subtract", "Divide", "Hue", "Saturation", "Color", "Luminosity")
    private val blendGroups = listOf(
        "Gelapkan" to listOf(2, 5, 14, 8, 13),
        "Cerahkan" to listOf(3, 6, 15, 7, 1),
        "Kontras" to listOf(4, 10, 9, 16, 17, 18, 19),
        "Perbandingan" to listOf(11, 12, 20, 21),
        "Warna" to listOf(22, 23, 24, 25))
    private val shapeNames = listOf("Kotak", "Kotak bulat", "Elips", "Poligon", "Bintang")
    private val palette = intArrayOf(
        Color.rgb(255, 90, 90), Color.rgb(255, 154, 60), Color.rgb(255, 211, 90), Color.rgb(90, 255, 138),
        Color.rgb(90, 255, 240), Color.rgb(90, 160, 255), Color.rgb(211, 90, 255), Color.WHITE,
        Color.rgb(140, 140, 150), Color.BLACK)

    private val poll = object : Runnable {
        override fun run() {
            try {
                tick()
            } catch (e: Exception) {
                statusTv.text = "Kesalahan UI: $e"
            }
            handler.postDelayed(this, 100)
        }
    }

    private val hideSnack = Runnable { snack.visibility = View.GONE }

    private fun toast(msg: String) {
        snack.text = msg
        snack.visibility = View.VISIBLE
        handler.removeCallbacks(hideSnack)
        handler.postDelayed(hideSnack, 2600)
    }

    // ---------- Pembaruan berkala ----------
    private fun tick() {
        val info = NativeEngine.nativeInfo().split("\n")
        val l1 = info.getOrElse(0) { "" }
        val m = Regex("undo (\\d+) redo (\\d+)").find(l1)
        undoBtn.isEnabled = (m?.groupValues?.get(1)?.toIntOrNull() ?: 0) > 0
        redoBtn.isEnabled = (m?.groupValues?.get(2)?.toIntOrNull() ?: 0) > 0
        val l3 = info.getOrElse(2) { "" }
        val cut = l3.lastIndexOf('|')
        val note = (if (cut >= 0) l3.substring(0, cut) else l3).trim()
        val status = (if (cut >= 0) l3.substring(cut + 1) else "").trim()
        statusTv.text = when {
            status.contains("GAGAL") -> "Gagal menyimpan!"
            status.contains("menyimpan") -> "Menyimpan…"
            else -> "Tersimpan"
        }
        statusTv.setTextColor(if (status.contains("GAGAL")) Ui.DANGER else Ui.DIM)
        val vp = VideoProxy.progress()
        if (vp >= 0 && !status.contains("GAGAL")) {
            statusTv.text = "Menyiapkan video $vp%"
            statusTv.setTextColor(Ui.ACCENT_SOFT)
        }
        if (note.isNotEmpty() && note != lastNote) toast(note)
        lastNote = note

        val dur = NativeEngine.nativeDuration()
        val fr = NativeEngine.nativeFrame()
        fps = NativeEngine.nativeFps()
        setTime(fr, dur)
        val nowPlaying = NativeEngine.nativePlaying()
        if (nowPlaying != playingShown) {
            playingShown = nowPlaying
            if (nowPlaying) audioPlayer.start()      // berhenti sendiri saat pemutaran berhenti
            playHolder.removeAllViews()
            playBtn = IconView(this, if (nowPlaying) IconView.PAUSE else IconView.PLAY, true)
            playBtn.setOnClickListener { NativeEngine.nativeTogglePlay() }
            playHolder.addView(playBtn, FrameLayout.LayoutParams(dp(40), dp(40)))
        }

        val st = Parsing.state(NativeEngine.nativeLayerState())
        lastState = st
        val id = st?.id ?: 0
        if (id != lastLayerId) {
            lastLayerId = id
            fxSignature = ""
            timeline.post { timeline.scrollToSelected() }
        }
        if (currentChannel >= 256 && st != null && (currentChannel - 256) / 32 >= st.fx.size) selectChannel(0)
        timeline.laneLabel = channelName(currentChannel)
        timeline.setPlayhead(fr, nowPlaying)
        timeline.setData(NativeEngine.nativeTimeline(), fr, dur, fps)
        val vs = NativeEngine.nativeViewState()
        if (vs.size >= 4) {
            zoomTv.text = Math.round(vs[0] * 100f).toString() + "%"
            gridBtn.tint = if (vs[3] > 0.5f) Ui.ACCENT_SOFT else Ui.TEXT
        }
        if (st == null && currentTab >= 0) openTab(-1)       // layer tidak terpilih → panel edit ditutup
        updateMultiBar()
        updatePanelVisibility()
        val isAudio = st != null && st.type == 5
        val wantA = if (isAudio) View.VISIBLE else View.GONE
        val wantV = if (isAudio) View.GONE else View.VISIBLE
        if (rowAudio.visibility != wantA) rowAudio.visibility = wantA
        if (rowBlend.visibility != wantV) rowBlend.visibility = wantV
        if (rowTransform.visibility != wantV) rowTransform.visibility = wantV
        shapeBtn.alpha = if (st != null && (st.type == 0 || st.type == 6)) 1f else 0.4f
        val shapeText = if (st != null && st.type == 6) "Edit Teks" else "Edit Bentuk"
        if (shapeLabel.text.toString() != shapeText) shapeLabel.text = shapeText

        if (currentTab >= 0) {
            sheetTitle.text = st?.name ?: "Tidak ada layer terpilih"
            sheetSub.text = if (st == null) "" else typeName(st.type) + "  ·  " + Ui.time(st.inF, fps) + " – " + Ui.time(st.outF, fps)
            emptyMsg.visibility = if (st == null) View.VISIBLE else View.GONE
            pages.forEachIndexed { k, v -> v.visibility = if (k == currentTab && st != null) View.VISIBLE else View.GONE }
            if (st != null) when (currentTab) {
                0 -> updateTransformTab(st)
                1 -> updateStyleTab(st)
                2 -> updateFxTab(st)
                3 -> {
                    railKey.keyed = if (currentChannel >= 256) st.fx.any { f -> f.keyed.any { 256 + f.idx * 32 + it == currentChannel } }
                                    else st.keyedT.contains(currentChannel)
                    updateGraphTab()
                    graph.setData(NativeEngine.nativeGraphData())
                }
            }
        }
    }

    private fun setTime(fr: Int, dur: Int) {
        val a = Ui.time(fr, fps)
        val b = "  /  " + Ui.time(dur, fps)
        val sp = SpannableString(a + b)
        sp.setSpan(ForegroundColorSpan(Ui.DIM), a.length, a.length + b.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        timeTv.text = sp
    }

    private fun typeName(t: Int) = when (t) { 0 -> "Bentuk"; 1 -> "Grup"; 2 -> "Null"; 4 -> "Video"; 5 -> "Suara"; 6 -> "Teks"; else -> "Gambar" }

    // ---------- Pembuatan UI ----------
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val path = intent.getStringExtra("path")
        if (path == null) {   // dibuka tanpa proyek → kembali ke layar awal
            startActivity(Intent(this, HomeActivity::class.java))
            finish()
            return
        }
        projectId = intent.getStringExtra("id")
        projectName = intent.getStringExtra("name") ?: "Proyek"
        NativeEngine.nativeOpen(path)
        VideoProxy.onError = { msg -> handler.post { toast(msg) } }
        audioPlayer = AudioPlayer(this)
        catalog = Parsing.catalog(NativeEngine.nativeEffectCatalog())
        sheetH = (resources.displayMetrics.heightPixels * 0.30f).toInt()

        val root = LinearLayout(this)
        root.orientation = LinearLayout.VERTICAL
        root.setBackgroundColor(Ui.BG)
        root.setOnApplyWindowInsetsListener { v, ins ->
            applyInsets(v, ins)
            ins
        }

        topBarView = buildTopBar()
        root.addView(topBarView, lp(-1, dp(46)))
        layerBar = buildLayerBar()
        root.addView(layerBar, lp(-1, dp(46)))
        multiTop = buildMultiTop()
        root.addView(multiTop, lp(-1, dp(46)))
        root.addView(buildPreview(), lp(-1, 0, 1f))
        root.addView(buildTransport(), lp(-1, dp(52)))

        timeline = TimelineView(this)
        timeline.background = panelTop(16f)
        timeline.elevation = dpf(6f)
        timeline.onLayerHold = { id ->
            if (NativeEngine.nativeMultiCount() < 0) {
                NativeEngine.nativeSelectLayer(id)
                NativeEngine.nativeMultiMode(true)
            }
        }
        root.addView(timeline, lp(-1, dp(190)))

        root.addView(buildSheet(), lp(-1, sheetH))
        gridPanel = buildToolGrid()
        root.addView(gridPanel, lp(-1, -2))

        showTab(-1)
        setContentView(root)
    }

    private fun buildTopBar(): View {
        val bar = LinearLayout(this)
        bar.gravity = Gravity.CENTER_VERTICAL
        bar.setPadding(dp(12), 0, dp(10), 0)
        val titles = LinearLayout(this)
        titles.orientation = LinearLayout.VERTICAL
        val back = IconView(this, IconView.CHEV_LEFT, false, true)
        back.setOnClickListener { finish() }
        bar.addView(back, lp(dp(36), dp(36)).apply { leftMargin = dp(-8); rightMargin = dp(4) })
        titles.addView(label(projectName, 15f, Ui.TEXT, true).apply {
            setSingleLine(); ellipsize = android.text.TextUtils.TruncateAt.END })
        val sub = LinearLayout(this)
        sub.gravity = Gravity.CENTER_VERTICAL
        timeTv = label("0:00.00", 10.5f, Ui.TEXT)
        statusTv = label("Tersimpan", 10f, Ui.DIM)
        sub.addView(timeTv)
        sub.addView(statusTv, lp(-2, -2).apply { leftMargin = dp(10) })
        titles.addView(sub)
        bar.addView(titles, lp(0, -2, 1f))

        val more = IconView(this, IconView.MORE, false, true)
        more.setOnClickListener {
            showActions(this, "Menu", listOf(
                SheetAction(IconView.SPARK, "Diagnostik GPU & efek", "Uji kompilasi semua efek, lihat info perangkat") { showDiag() }))
        }
        val exportBtn = TextView(this)
        exportBtn.text = "Ekspor"
        exportBtn.textSize = 12f
        exportBtn.typeface = Ui.MEDIUM
        exportBtn.setTextColor(Color.WHITE)
        exportBtn.gravity = Gravity.CENTER
        exportBtn.setPadding(dp(14), 0, dp(14), 0)
        exportBtn.background = accentFill(15f)
        exportBtn.setOnClickListener { showExport() }
        bar.addView(more, lp(dp(38), dp(38)))
        bar.addView(exportBtn, lp(-2, dp(30)).apply { leftMargin = dp(4) })
        return bar
    }

    private fun buildPreview(): View {
        val fl = FrameLayout(this)
        glView = GLSurfaceView(this)
        glView.setEGLContextClientVersion(3)
        glView.setEGLConfigChooser(8, 8, 8, 8, 0, 0)
        glView.setRenderer(EngineRenderer(this))
        glView.renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
        // Satu jari = pilih/geser objek (engine). Dua jari = cubit zoom + geser kanvas.
        var multi = false
        var lastSpan = 0f
        var lastFx = 0f
        var lastFy = 0f
        glView.setOnTouchListener { _, e ->
            if (e.pointerCount >= 2 || multi) {
                val act = e.actionMasked
                if (e.pointerCount >= 2 && act != MotionEvent.ACTION_POINTER_UP) {
                    val fx = (e.getX(0) + e.getX(1)) / 2f
                    val fy = (e.getY(0) + e.getY(1)) / 2f
                    val dx = e.getX(0) - e.getX(1)
                    val dy = e.getY(0) - e.getY(1)
                    val span = Math.sqrt((dx * dx + dy * dy).toDouble()).toFloat().coerceAtLeast(20f)
                    if (!multi) {
                        multi = true
                        NativeEngine.nativeTouch(3, e.x, e.y)     // batalkan seret satu jari yang sedang berjalan
                    } else if (lastSpan > 0f) {
                        NativeEngine.nativeView(1, fx - lastFx, fy - lastFy, 0f)
                        NativeEngine.nativeView(0, span / lastSpan, fx, fy)
                    }
                    lastSpan = span; lastFx = fx; lastFy = fy
                } else if (act == MotionEvent.ACTION_POINTER_UP) {
                    // jari tersisa diabaikan sampai semua jari diangkat
                    lastSpan = 0f
                }
                if (act == MotionEvent.ACTION_UP || act == MotionEvent.ACTION_CANCEL) multi = false
                true
            } else {
                val a = when (e.actionMasked) {
                    MotionEvent.ACTION_DOWN -> 0
                    MotionEvent.ACTION_MOVE -> 1
                    MotionEvent.ACTION_UP -> 2
                    MotionEvent.ACTION_CANCEL -> 3
                    else -> -1
                }
                if (a >= 0) NativeEngine.nativeTouch(a, e.x, e.y)
                true
            }
        }
        fl.addView(glView, FrameLayout.LayoutParams(-1, -1))

        snack = TextView(this)
        snack.textSize = 12f
        snack.typeface = Ui.MEDIUM
        snack.setTextColor(Ui.TEXT)
        snack.setPadding(dp(16), dp(9), dp(16), dp(9))
        snack.background = card(18f, Color.argb(240, 38, 40, 52), Color.argb(240, 26, 28, 37))
        snack.visibility = View.GONE
        fl.addView(snack, FrameLayout.LayoutParams(-2, -2, Gravity.TOP or Gravity.CENTER_HORIZONTAL).apply {
            topMargin = dp(10)
        })

        // Rel kanan: garis bantu, zoom +, persen (ketuk = reset), zoom −
        val rail = LinearLayout(this)
        rail.orientation = LinearLayout.VERTICAL
        rail.gravity = Gravity.CENTER_HORIZONTAL
        rail.setPadding(dp(3), dp(6), dp(3), dp(6))
        rail.background = card(20f, Color.argb(215, 31, 33, 43), Color.argb(215, 21, 22, 29))
        fun railBtn(kind: Int, onClick: () -> Unit): IconView {
            val b = IconView(this, kind, false, true)
            b.setOnClickListener { onClick() }
            rail.addView(b, lp(dp(38), dp(38)))
            return b
        }
        gridBtn = railBtn(IconView.GRID) { NativeEngine.nativeView(3, 0f, 0f, 0f) }
        railBtn(IconView.PLUS) { NativeEngine.nativeView(0, 1.25f, glView.width / 2f, glView.height / 2f) }
        zoomTv = label("100%", 10.5f, Ui.TEXT, true)
        zoomTv.gravity = Gravity.CENTER
        zoomTv.setOnClickListener { NativeEngine.nativeView(2, 0f, 0f, 0f) }
        rail.addView(zoomTv, lp(dp(38), dp(30)))
        railBtn(IconView.MINUS) { NativeEngine.nativeView(0, 0.8f, glView.width / 2f, glView.height / 2f) }
        fl.addView(rail, FrameLayout.LayoutParams(-2, -2, Gravity.END or Gravity.CENTER_VERTICAL).apply { rightMargin = dp(8) })
        return fl
    }

    private fun buildTransport(): View {
        val row = LinearLayout(this)
        row.gravity = Gravity.CENTER_VERTICAL
        row.setPadding(dp(8), 0, dp(8), 0)
        fun small(kind: Int, onClick: () -> Unit): IconView {
            val b = IconView(this, kind, false, true)
            b.setOnClickListener { onClick() }
            return b
        }
        fun spacer() = View(this)

        undoBtn = small(IconView.UNDO) { NativeEngine.nativeCommand(3) }
        redoBtn = small(IconView.REDO) { NativeEngine.nativeCommand(4) }
        row.addView(undoBtn, lp(dp(36), dp(36)))
        row.addView(redoBtn, lp(dp(36), dp(36)))
        row.addView(spacer(), lp(0, 1, 1f))

        row.addView(small(IconView.SKIP_START) { NativeEngine.nativeSeek(0) }, lp(dp(36), dp(36)))
        playHolder = FrameLayout(this)
        playBtn = IconView(this, IconView.PLAY, true)
        playBtn.setOnClickListener { NativeEngine.nativeTogglePlay() }
        playHolder.addView(playBtn, FrameLayout.LayoutParams(dp(40), dp(40)))
        row.addView(playHolder, lp(dp(40), dp(40)).apply { leftMargin = dp(4); rightMargin = dp(4) })
        row.addView(small(IconView.SKIP_END) { NativeEngine.nativeSeek(maxOf(NativeEngine.nativeDuration() - 1, 0)) },
            lp(dp(36), dp(36)))
        row.addView(spacer(), lp(0, 1, 1f))

        row.addView(small(IconView.SELECT) { toggleMultiMode() }, lp(dp(36), dp(36)))
        row.addView(small(IconView.CLIPBOARD) { showCopyMenu() }, lp(dp(36), dp(36)))
        row.addView(small(IconView.FIT) { toggleFullPreview() }, lp(dp(36), dp(36)))
        val add = IconView(this, IconView.PLUS, true)
        add.setOnClickListener { showAddMenu() }
        row.addView(add, lp(dp(34), dp(34)).apply { leftMargin = dp(4) })
        return row
    }

    /** Layar penuh pratinjau: sembunyikan bilah atas, timeline, dan panel; ketuk lagi untuk kembali. */
    private fun toggleFullPreview() {
        fullPreview = !fullPreview
        val vis = if (fullPreview) View.GONE else View.VISIBLE
        timeline.visibility = vis
        sheet.visibility = if (fullPreview || currentTab < 0) View.GONE else View.VISIBLE
        updateBars()
        updatePanelVisibility()
        toast(if (fullPreview) "Layar penuh pratinjau — ketuk ikon yang sama untuk kembali" else "Kembali ke editor")
    }

    // ---------- Menu salin / tempel ----------
    private fun showCopyMenu() {
        val has = lastState != null
        val clip = NativeEngine.nativeHasClip()
        val rows = ArrayList<SheetAction>()
        rows.add(SheetAction(IconView.COPY, "Gandakan", "Buat salinan layer terpilih tepat di atasnya") {
            needLayer { NativeEngine.nativeCommand(5) }
        })
        rows.add(SheetAction(IconView.CLIPBOARD, "Salin", if (has) "Simpan layer terpilih ke clipboard" else "Pilih layer dulu") {
            needLayer { NativeEngine.nativeClip(0, 0); toast("Layer disalin") }
        })
        rows.add(SheetAction(IconView.CLIPBOARD, "Tempel", if (clip) "Tempel layer dari clipboard di playhead" else "Clipboard kosong") {
            if (NativeEngine.nativeHasClip()) NativeEngine.nativeClip(1, 0) else toast("Salin layer dulu")
        })
        rows.add(SheetAction(IconView.STYLE, "Tempel gaya…", "Pilih bagian gaya yang ingin ditempel ke layer terpilih") {
            if (!NativeEngine.nativeHasClip()) toast("Salin layer dulu") else needLayer { showPasteStyle() }
        })
        showActions(this, "Salin & tempel", rows)
    }

    private fun showPasteStyle() {
        var mask = 0
        val names = listOf("Warna & Bentuk", "Blend & Opasitas", "Transform", "Efek")
        val icons = intArrayOf(IconView.STYLE, IconView.LAYERS, IconView.MOVE, IconView.SPARK)
        showSheet(this, "Tempel gaya", { box, close ->
            val cells = ArrayList<LinearLayout>()
            fun paint(i: Int) {
                val on = (mask shr i) and 1 == 1
                cells[i].background = if (on) rounded(Ui.alpha(Ui.ACCENT, 70), 16f, Ui.ACCENT, 1)
                else card(16f)
            }
            for (r in 0 until 2) {
                val row = LinearLayout(this)
                for (k in 0 until 2) {
                    val i = r * 2 + k
                    val cell = LinearLayout(this)
                    cell.orientation = LinearLayout.VERTICAL
                    cell.gravity = Gravity.CENTER
                    val ic = IconView(this, icons[i], false, true)
                    ic.isClickable = false
                    cell.addView(ic, lp(dp(30), dp(30)))
                    cell.addView(label(names[i], 12f, Ui.TEXT, true).apply { gravity = Gravity.CENTER; setPadding(0, dp(4), 0, 0) })
                    cell.setOnClickListener { mask = mask xor (1 shl i); paint(i) }
                    cells.add(cell)
                    row.addView(cell, lp(0, dp(84), 1f).apply { if (k == 0) rightMargin = dp(10) })
                }
                box.addView(row, lp(-1, -2).apply { bottomMargin = dp(10) })
            }
            for (i in 0 until 4) paint(i)
            box.addView(button("Tempel", 1) {
                if (mask == 0) toast("Pilih minimal satu bagian")
                else { NativeEngine.nativeClip(2, mask); close() }
            }, lp(-1, -2).apply { topMargin = dp(6) })
        })
    }

    // ---------- Ekspor video ----------
    private var exporting = false

    private fun showExport() {
        if (exporting) return
        val size = NativeEngine.nativeProjectSize()
        val pw = size[0].coerceAtLeast(2)
        val ph = size[1].coerceAtLeast(2)
        val dur = NativeEngine.nativeDuration()
        val f = NativeEngine.nativeFps().coerceAtLeast(1)
        // Pilihan resolusi = panjang sisi pendek; sisi lain mengikuti rasio kanvas (dibulatkan ke genap).
        val shortSide = minOf(pw, ph)
        val opts = listOf(480, 720, 1080).filter { it <= shortSide * 2 } .ifEmpty { listOf(shortSide) }
        var sel = opts.indexOf(720).let { if (it >= 0) it else 0 }
        fun dims(s: Int): IntArray {
            val k = s.toFloat() / shortSide
            val w = (Math.round(pw * k) / 2) * 2
            val h = (Math.round(ph * k) / 2) * 2
            return intArrayOf(w.coerceAtLeast(2), h.coerceAtLeast(2))
        }
        showSheet(this, "Ekspor video", { box, close ->
            val info = label("", 11.5f, Ui.DIM)
            fun refreshInfo() {
                val d = dims(opts[sel])
                val secs = dur.toFloat() / f
                info.text = "${d[0]} × ${d[1]} · ${f} fps · ${String.format("%.1f", secs)} dtk · MP4 (H.264)"
            }
            val row = LinearLayout(this)
            val chips = ArrayList<TextView>()
            opts.forEachIndexed { i, s ->
                val c = chip("${s}p", i == sel) {
                    sel = i
                    chips.forEachIndexed { k, cc -> styleChip(cc, k == sel) }
                    refreshInfo()
                }
                chips.add(c)
                row.addView(c, lp(0, dp(36), 1f).apply { rightMargin = dp(8) })
            }
            box.addView(sectionLabel("Kualitas"))
            box.addView(row, lp(-1, -2))
            info.setPadding(dp(2), dp(10), dp(2), dp(4))
            box.addView(info)
            refreshInfo()
            box.addView(label("Layar akan berhenti merespons sebentar saat proses ekspor. Jangan tutup aplikasi.",
                10.5f, Ui.DIM).apply { setPadding(dp(2), dp(4), dp(2), dp(10)) })
            box.addView(button("Mulai ekspor", 1) {
                close()
                val d = dims(opts[sel])
                runExport(d[0], d[1], f, dur)
            }, lp(-1, -2))
        })
    }

    private fun runExport(w: Int, h: Int, fpsV: Int, frames: Int) {
        if (NativeEngine.nativePlaying()) NativeEngine.nativeTogglePlay()
        exporting = true
        Exporter.cancel = false
        val bitrate = (w.toLong() * h * fpsV / 8).coerceIn(1_500_000L, 40_000_000L).toInt()  // ≈0,125 bit/piksel
        val out = File(cacheDir, "export.mp4")
        var progressTv: TextView? = null
        var bar: android.widget.ProgressBar? = null
        val dlg = showSheet(this, "Mengekspor…", { box, _ ->
            progressTv = label("0%", 12f, Ui.DIM)
            box.addView(progressTv)
            bar = android.widget.ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal)
            bar!!.max = frames
            bar!!.progressTintList = android.content.res.ColorStateList.valueOf(Ui.ACCENT)
            box.addView(bar, lp(-1, dp(12)).apply { topMargin = dp(8); bottomMargin = dp(12) })
            box.addView(button("Batal", 2) { Exporter.cancel = true }, lp(-1, -2))
        })
        dlg.setCancelable(false)
        glView.queueEvent {
            val r = Exporter.encode(w, h, fpsV, frames, bitrate, out.absolutePath, { f -> prepareExportFrame(f) }) { done ->
                if (done % 3 == 0 || done == frames) handler.post {
                    bar?.progress = done
                    progressTv?.text = "${done * 100 / frames}%  ·  frame $done / $frames"
                }
            }
            handler.post {
                if (r.ok && AudioExport.hasAudio()) {
                    progressTv?.text = "Menambahkan suara…"
                    bar?.max = 100
                    bar?.progress = 0
                    val pf = maxOf(NativeEngine.nativeFps(), 1)
                    Thread {
                        val ar = AudioExport.addAudio(this, out, frames, fpsV, pf) { pct ->
                            handler.post { bar?.progress = pct; progressTv?.text = "Suara $pct%" }
                        }
                        handler.post { finishExport(dlg, true, ar.file, ar.message ?: "") }
                    }.start()
                } else {
                    finishExport(dlg, r.ok, out, r.message)
                }
            }
        }
    }

    private fun finishExport(dlg: android.app.Dialog, ok: Boolean, file: File, message: String) {
        dlg.dismiss()
        exporting = false
        if (ok) {
            val uri = Exporter.saveToGallery(this, file)
            file.delete()
            val base = if (uri != null) "Tersimpan di galeri: Movies/Studio" else "Ekspor selesai, tapi gagal menyimpan ke galeri"
            toast(if (message.isNotEmpty()) "$base. $message" else base)
        } else {
            file.delete()
            toast(message)
        }
    }

    private fun showAddMenu() {
        showActions(this, "Tambah ke proyek", listOf(
            SheetAction(IconView.STYLE, "Bentuk", "Kotak, elips, poligon, atau bintang yang bisa dianimasikan") { NativeEngine.nativeCommand(0) },
            SheetAction(IconView.COPY, "Gambar", "Foto atau gambar dari galeri HP") { pickImage() },
            SheetAction(IconView.CAMERA, "Video", "Klip dari galeri HP — bisa diberi efek, keyframe, dan kurva kecepatan") { pickVideo() },
            SheetAction(IconView.LAYERS, "Grup", "Wadah kosong: gerak, skala, dan opasitasnya berlaku untuk semua layer di dalamnya") { NativeEngine.nativeAddGroup() },
            SheetAction(IconView.TEXT, "Teks", "Tulisan dengan huruf, ukuran, dan warna yang bisa diubah") { showTextEditor(false) },
            SheetAction(IconView.PLAY, "Suara", "Musik atau rekaman dari HP — volume bisa dikeyframe") { pickAudio() }))
    }

    private fun needLayer(action: () -> Unit) {
        if (lastState == null) toast("Pilih layer dulu: ketuk objek di kanvas atau bar di timeline")
        else action()
    }

    /** Panel alat hanya tampil saat ada layer terpilih dan panel edit belum terbuka (lebih banyak ruang untuk kanvas). */
    private fun updatePanelVisibility() {
        val show = !fullPreview && currentTab < 0 && lastState != null && !multiShown
        val v = if (show) View.VISIBLE else View.GONE
        if (gridPanel.visibility != v) gridPanel.visibility = v
    }

    /** Tombol kecil di baris atas (hanya ikon). */
    private fun stripButton(kind: Int, color: Int, onClick: () -> Unit): View {
        val fl = FrameLayout(this)
        fl.background = card(11f)
        val ic = IconView(this, kind, false, true)
        ic.isClickable = false
        ic.tint = color
        fl.addView(ic, FrameLayout.LayoutParams(-1, -1))
        fl.setOnClickListener { onClick() }
        return fl
    }

    /** Tombol persegi panjang: ikon di atas, nama di bawah (tinggi berbeda per baris, tidak dipaksa persegi). */
    private fun wideButton(kind: Int, text: String, color: Int, onClick: () -> Unit): View {
        val box = LinearLayout(this)
        box.orientation = LinearLayout.VERTICAL
        box.gravity = Gravity.CENTER
        box.background = card(13f)
        val ic = IconView(this, kind, false, true)
        ic.isClickable = false
        ic.tint = color
        box.addView(ic, lp(dp(28), dp(28)))
        val t = label(text, 10.5f, Ui.TEXT, true)
        t.setSingleLine()
        t.gravity = Gravity.CENTER
        box.addView(t, lp(-2, -2))
        box.setOnClickListener { onClick() }
        return box
    }

    /** Tombol rendah: ikon di kiri, nama di kanan (baris Masker / Induk / Grup). Label = anak ke-1. */
    private fun wideRow(kind: Int, text: String, color: Int, onClick: () -> Unit): LinearLayout {
        val box = LinearLayout(this)
        box.gravity = Gravity.CENTER
        box.background = card(13f)
        val ic = IconView(this, kind, false, true)
        ic.isClickable = false
        ic.tint = color
        box.addView(ic, lp(dp(24), dp(24)))
        val t = label(text, 11.5f, Ui.TEXT, true)
        t.setSingleLine()
        t.setPadding(dp(5), 0, 0, 0)
        box.addView(t, lp(-2, -2))
        box.setOnClickListener { onClick() }
        return box
    }

    private fun trimFront() {
        val s = lastState ?: return
        val fr = NativeEngine.nativeFrame()
        if (fr > s.inF && fr < s.outF) NativeEngine.nativeTrim(fr, s.outF) else toast("Geser playhead ke dalam durasi layer dulu")
    }

    private fun trimBack() {
        val s = lastState ?: return
        val fr = NativeEngine.nativeFrame()
        if (fr > s.inF && fr < s.outF) NativeEngine.nativeTrim(s.inF, fr) else toast("Geser playhead ke dalam durasi layer dulu")
    }

    private fun splitHere() {
        val s = lastState ?: return
        val fr = NativeEngine.nativeFrame()
        if (fr > s.inF && fr < s.outF) NativeEngine.nativeCommand(1) else toast("Geser playhead ke dalam durasi layer dulu")
    }

    /**
     * Panel alat layer ala Alight Motion: baris kecil di atas (kecepatan, potong depan, belah, potong belakang, hapus),
     * lalu tombol persegi panjang Warna/Blend/Efek, lalu Transform/Bentuk/Gandakan. Disembunyikan bila tidak ada layer.
     */
    private fun buildToolGrid(): LinearLayout {
        val col = LinearLayout(this)
        col.orientation = LinearLayout.VERTICAL
        col.setBackgroundColor(Ui.BG)
        col.setPadding(dp(12), dp(8), dp(12), dp(10))

        val strip = LinearLayout(this)
        val sb = listOf(
            stripButton(IconView.SPEED, Color.rgb(100, 220, 170)) { openSpeed() },
            stripButton(IconView.TRIM_FRONT, Ui.TEXT) { trimFront() },
            stripButton(IconView.SPLIT, Ui.TEXT) { splitHere() },
            stripButton(IconView.TRIM_BACK, Ui.TEXT) { trimBack() },
            stripButton(IconView.COPY, Ui.TEXT) { NativeEngine.nativeCommand(5) },
            stripButton(IconView.TRASH, Ui.DANGER) { NativeEngine.nativeCommand(2) })
        sb.forEachIndexed { k, v -> strip.addView(v, lp(0, dp(38), 1f).apply { if (k < sb.size - 1) rightMargin = dp(6) }) }
        col.addView(strip, lp(-1, -2).apply { bottomMargin = dp(8) })

        fun row(h: Int, views: List<View>, last: Boolean): LinearLayout {
            val r = LinearLayout(this)
            views.forEachIndexed { k, v -> r.addView(v, lp(0, -1, 1f).apply { if (k < views.size - 1) rightMargin = dp(8) }) }
            col.addView(r, lp(-1, dp(h)).apply { if (!last) bottomMargin = dp(8) })
            return r
        }
        rowBlend = row(58, listOf(
            wideButton(IconView.STYLE, "Warna & Isi", Color.rgb(255, 170, 70)) { openTab(1, true) },
            wideButton(IconView.SCALE, "Border & Bayangan", Color.rgb(255, 130, 100)) { showBorderMenu() },
            wideButton(IconView.LAYERS, "Blend & Opasitas", Color.rgb(120, 200, 255)) { openTab(1) }), false)
        shapeBtn = wideButton(IconView.SCALE, "Edit Bentuk", Color.rgb(230, 210, 90)) {
            when (lastState?.type) {
                0 -> openTab(1, toShape = true)
                6 -> showTextEditor(true)
                else -> toast("Edit bentuk hanya untuk layer bentuk atau teks")
            }
        }
        shapeLabel = (shapeBtn as LinearLayout).getChildAt(1) as TextView
        rowTransform = row(48, listOf(
            wideButton(IconView.MOVE, "Transform", Color.rgb(150, 130, 255)) { openTab(0) },
            shapeBtn,
            wideButton(IconView.SPARK, "Efek", Color.rgb(255, 120, 190)) { openTab(2) },
            wideButton(IconView.STAR, "Preset", Color.rgb(110, 235, 160)) { showPresetMenu() }), true)
        rowAudio = row(58, listOf(
            wideButton(IconView.SPEED, "Volume", Color.rgb(100, 220, 170)) { openTab(1) }), true)
        rowAudio.visibility = View.GONE
        return col
    }

    /** Kecepatan = kurva waktu yang diedit di grafik (curam = cepat, datar = berhenti). */
    private fun openSpeed() {
        if (lastState?.type == 5) { toast("Layer suara: atur volume lewat tombol Volume"); return }
        NativeEngine.nativeSpeedCurve()
        selectChannel(TIME_CH)
        openTab(3)
        handler.postDelayed({ graph.fit() }, 250)
    }

    private fun buildSheet(): View {
        sheet = LinearLayout(this)
        sheet.orientation = LinearLayout.VERTICAL
        sheet.background = panelTop(20f)
        sheet.elevation = dpf(10f)

        // Pegangan: seret untuk mengubah tinggi panel
        val handle = FrameLayout(this)
        val grab = View(this)
        grab.background = rounded(Ui.SURFACE3, 3f)
        handle.addView(grab, FrameLayout.LayoutParams(dp(34), dp(4), Gravity.CENTER))
        var dragY = 0f
        var dragH = 0
        handle.setOnTouchListener { _, e ->
            when (e.actionMasked) {
                MotionEvent.ACTION_DOWN -> { dragY = e.rawY; dragH = sheetH }
                MotionEvent.ACTION_MOVE -> {
                    val maxH = (resources.displayMetrics.heightPixels * 0.6f).toInt()
                    sheetH = (dragH + (dragY - e.rawY)).toInt().coerceIn(dp(150), maxH)
                    val lpp = sheet.layoutParams
                    lpp.height = sheetH
                    sheet.layoutParams = lpp
                }
            }
            true
        }
        sheet.addView(handle, lp(-1, dp(20)))

        // Rail kiri (seperti Alight Motion): kembali · keyframe · kurva
        val body = LinearLayout(this)
        val rail = LinearLayout(this)
        rail.orientation = LinearLayout.VERTICAL
        rail.gravity = Gravity.CENTER_HORIZONTAL
        rail.setBackgroundColor(Ui.alpha(Color.BLACK, 40))
        rail.setPadding(0, dp(6), 0, dp(6))
        val back = IconView(this, IconView.CHEV_LEFT, false, true)
        back.setOnClickListener { goBack() }
        rail.addView(back, lp(dp(44), dp(44)))
        railKey = DiamondView(this)
        railKey.setOnClickListener { railKeyframe() }
        rail.addView(railKey, lp(dp(44), dp(44)))
        railCurve = IconView(this, IconView.CURVE, false, true)
        railCurve.setOnClickListener { if (currentTab == 3) goBack() else openTab(3) }
        rail.addView(railCurve, lp(dp(44), dp(44)))
        rail.addView(View(this), lp(1, 0, 1f))
        railMore = IconView(this, IconView.MORE_H, false, true)
        railMore.setOnClickListener { showResetMenu() }
        rail.addView(railMore, lp(dp(44), dp(44)))
        body.addView(rail, lp(dp(54), -1))

        val right = LinearLayout(this)
        right.orientation = LinearLayout.VERTICAL
        val titles = LinearLayout(this)
        titles.orientation = LinearLayout.HORIZONTAL
        titles.gravity = Gravity.CENTER_VERTICAL
        titles.setPadding(dp(14), dp(2), dp(10), dp(2))
        sheetTitle = label("", 13f, Ui.TEXT, true)
        sheetTitle.setSingleLine()
        sheetTitle.ellipsize = android.text.TextUtils.TruncateAt.END
        sheetSub = label("", 10.5f, Ui.DIM)
        sheetSub.setSingleLine()
        titles.addView(sheetTitle, lp(0, -2, 1f))
        titles.addView(sheetSub, lp(-2, -2).apply { leftMargin = dp(8) })
        right.addView(titles, lp(-1, -2))

        val scroll = ScrollView(this)
        sheetScroll = scroll
        val holder = LinearLayout(this)
        holder.orientation = LinearLayout.VERTICAL
        emptyMsg = label("Ketuk sebuah objek di kanvas atau bar di timeline untuk mulai mengedit.", 12f, Ui.DIM)
        emptyMsg.setPadding(dp(18), dp(20), dp(18), dp(20))
        holder.addView(emptyMsg, lp(-1, -2))
        pages.add(buildTransformPage())
        pages.add(buildStylePage())
        pages.add(buildFxPage())
        pages.add(buildGraphPage())
        for (p in pages) holder.addView(p, lp(-1, -2))
        scroll.addView(holder, FrameLayout.LayoutParams(-1, -2))
        right.addView(scroll, lp(-1, 0, 1f))
        body.addView(right, lp(0, -1, 1f))
        sheet.addView(body, lp(-1, 0, 1f))
        return sheet
    }

    /** Tombol kembali di rail: dari grafik kembali ke tab sebelumnya, selain itu menutup panel. */
    private fun goBack() {
        if (currentTab == 3 && prevTab >= 0) openTab(prevTab, keepFocus = true) else openTab(-1)
    }

    /** ◇ di rail: keyframe di playhead untuk komponen yang sedang difokuskan. */
    private fun railKeyframe() {
        if (currentChannel < 0) { toast("Ketuk komponen yang ingin dianimasikan dulu"); return }
        selectChannel(currentChannel)
        // Geser X/Y diberi keyframe bersama (satu pad); selain itu satu komponen saja
        NativeEngine.nativeKeyHere(currentTab == 0 && tSub == 0 && (currentChannel == 0 || currentChannel == 1))
    }

    private fun defaultFor(ch: Int): Float = when (ch) {
        3, 4, 6 -> 1f
        in 0..9 -> 0f
        else -> {
            val fi = (ch - 256) / 32
            val pi = (ch - 256) % 32
            val fx = lastState?.fx?.getOrNull(fi)
            fx?.let { catalog[it.type] }?.params?.firstOrNull { it.flat == pi }?.def ?: 0f
        }
    }

    private fun resetChannels(chs: List<Int>) {
        if (lastState == null) return
        NativeEngine.nativeGesture(true)
        for (c in chs) NativeEngine.nativeResetChannel(c, defaultFor(c))     // hapus semua keyframe + nilai dasar
        NativeEngine.nativeGesture(false)
    }

    /** Menu … di rail (dua opsi reset ala Alight Motion). */
    private fun showResetMenu() {
        val s = lastState ?: return
        when (currentTab) {
            0 -> showActions(this, "Atur ulang", listOf(
                SheetAction(IconView.MOVE, "Atur Ulang Lokasi", "Posisi X, Y, Z kembali ke 0") { resetChannels(listOf(0, 1, 2)) },
                SheetAction(IconView.ROTATE, "Atur Ulang Transform", "Posisi, rotasi, skala, dan titik pusat kembali ke awal") {
                    resetChannels(listOf(0, 1, 2, 3, 4, 5, 7, 8))
                }))
            1 -> showActions(this, "Atur ulang", listOf(
                SheetAction(IconView.LAYERS, "Atur Ulang Opasitas", "Kembali ke 100%") { resetChannels(listOf(6)) },
                SheetAction(IconView.LAYERS, "Atur Ulang Blend", "Kembali ke Normal") { NativeEngine.nativeSetBlend(0) }))
            2 -> {
                val fi = if (currentChannel >= 256) (currentChannel - 256) / 32 else -1
                val fx = s.fx.getOrNull(fi)
                val def = fx?.let { catalog[it.type] }
                if (fx == null || def == null) { toast("Ketuk parameter sebuah efek dulu"); return }
                showActions(this, "Atur ulang", listOf(
                    SheetAction(IconView.SPARK, "Atur Ulang Parameter", channelName(currentChannel)) { resetChannels(listOf(currentChannel)) },
                    SheetAction(IconView.SPARK, "Atur Ulang Efek", def.label) {
                        resetChannels(def.params.map { 256 + fx.idx * 32 + it.flat })
                    }))
            }
            else -> {}
        }
    }

    private fun pageBox(): LinearLayout {
        val b = LinearLayout(this)
        b.orientation = LinearLayout.VERTICAL
        b.setPadding(dp(14), 0, dp(10), dp(16))
        return b
    }

    private fun showTab(i: Int) = openTab(i)

    private fun openTab(i: Int, toColor: Boolean = false, keepFocus: Boolean = false, toShape: Boolean = false) {
        if (i == 3 && currentTab in 0..2) prevTab = currentTab
        if (!keepFocus && i in 0..2) {
            when (i) {
                0 -> selectChannel(arrayOf(0, 5, 3, 7)[tSub])
                1 -> selectChannel(6)
                else -> if (currentChannel < 256) { currentChannel = -1; NativeEngine.nativeSelectProp(0) }
            }
        }
        if (i == 3) selectChannel(if (currentChannel >= 0) currentChannel else 0)
        railMore.visibility = if (i in 0..2) View.VISIBLE else View.INVISIBLE
        currentTab = i
        sheet.visibility = if (currentTab < 0 || fullPreview) View.GONE else View.VISIBLE
        updatePanelVisibility()
        fxSignature = ""
        railCurve.tint = if (currentTab == 3) Ui.ACCENT_SOFT else Ui.TEXT
        if (currentTab >= 0) sheetScroll.post {
            if (toColor && currentTab == 1 && colorSection.visibility == View.VISIBLE)
                sheetScroll.scrollTo(0, pages[1].top + colorSection.top - dp(4))
            else if (toShape && currentTab == 1 && shapeSection.visibility == View.VISIBLE)
                sheetScroll.scrollTo(0, pages[1].top + shapeSection.top - dp(4))
            else sheetScroll.scrollTo(0, 0)
        }
    }

    // ---------- Tab Transform ----------
    private fun numBoxFor(name: String, ch: Int): NumBox {
        val nb = NumBox(this, name)
        nb.setOnClickListener {
            if (currentChannel == ch) {
                promptNumber(this, channelName(ch), nb.cur) { v ->
                    NativeEngine.nativeGesture(true); NativeEngine.nativeSetParam(ch, v); NativeEngine.nativeGesture(false)
                }
            } else selectChannel(ch)
        }
        return nb
    }

    private fun boxRow(boxes: List<NumBox>): LinearLayout {
        val r = LinearLayout(this)
        boxes.forEachIndexed { k, b -> r.addView(b, lp(0, -2, 1f).apply { if (k < boxes.size - 1) rightMargin = dp(8) }) }
        return r
    }

    private fun setTSub(i: Int) {
        tSub = i
        tSubViews.forEachIndexed { k, v -> v.visibility = if (k == i) View.VISIBLE else View.GONE }
        tSubBtns.forEachIndexed { k, b ->
            b.tint = if (k == i) Color.rgb(110, 235, 160) else Ui.TEXT
            b.background = if (k == i) rounded(Color.rgb(18, 18, 24), 12f) else null
        }
        if (currentTab == 0) selectChannel(arrayOf(0, 5, 3, 7)[i])
    }

    private fun buildTransformPage(): View {
        val page = LinearLayout(this)
        page.setPadding(dp(12), dp(4), dp(0), dp(12))
        val content = LinearLayout(this)
        content.orientation = LinearLayout.VERTICAL

        // 1) Geser: X, Y, Z + pad
        val move = LinearLayout(this)
        move.orientation = LinearLayout.VERTICAL
        for ((nm, ch) in listOf("X" to 0, "Y" to 1, "Z" to 2)) moveBoxes.add(numBoxFor(nm, ch))
        move.addView(boxRow(moveBoxes), lp(-1, -2))
        val padBox = FrameLayout(this)
        padBox.background = rounded(Ui.alpha(Color.WHITE, 8), 12f)
        val pad = PadView(this)
        padBox.addView(pad, FrameLayout.LayoutParams(-1, -1))
        val hint = label("Geser ke sini untuk memindahkan layer", 12.5f, Ui.DIM)
        hint.gravity = Gravity.CENTER
        padBox.addView(hint, FrameLayout.LayoutParams(-1, -1))
        move.addView(padBox, lp(-1, dp(120)).apply { topMargin = dp(10) })
        var bx = 0f; var by = 0f; var ax = 0f; var ay = 0f
        val k = 2.4f / resources.displayMetrics.density
        pad.onStart = {
            val st = lastState
            bx = st?.t?.get(0) ?: 0f; by = st?.t?.get(1) ?: 0f; ax = 0f; ay = 0f
            if (currentChannel > 2) selectChannel(0)
            NativeEngine.nativeGesture(true)
        }
        pad.onDrag = { dx, dy ->
            ax += dx * k; ay += dy * k
            NativeEngine.nativeSetParam(0, bx + ax)
            NativeEngine.nativeSetParam(1, by + ay)
        }
        pad.onStop = { NativeEngine.nativeGesture(false) }

        // 2) Putar: dial
        val rot = FrameLayout(this)
        dial = DialView(this)
        rot.addView(dial, FrameLayout.LayoutParams(-1, -1))
        rotBox = numBoxFor("Derajat", 5)
        rot.addView(rotBox, FrameLayout.LayoutParams(dp(110), -2, Gravity.CENTER))
        var rotAcc = 0f
        dial.onStart = { rotAcc = lastState?.t?.get(4) ?: 0f; selectChannel(5); NativeEngine.nativeGesture(true) }
        dial.onDelta = { d -> rotAcc += d; NativeEngine.nativeSetParam(5, rotAcc) }
        dial.onStop = { NativeEngine.nativeGesture(false) }
        val rotWrap = LinearLayout(this)
        rotWrap.addView(rot, lp(-1, dp(170)))

        // 3) Skala: X, Y + kunci rasio + penggaris
        val scale = LinearLayout(this)
        scale.orientation = LinearLayout.VERTICAL
        for ((nm, ch) in listOf("Skala X" to 3, "Skala Y" to 4)) scaleBoxes.add(numBoxFor(nm, ch))
        scale.addView(boxRow(scaleBoxes), lp(-1, -2))
        val lockChip = chip("Kunci rasio", true) {}
        lockChip.setOnClickListener { scaleLock = !scaleLock; styleChip(lockChip, scaleLock) }
        scale.addView(lockChip, lp(-2, dp(32)).apply { topMargin = dp(10) })
        scaleRuler = RulerView(this)
        scaleRuler.setRange(0f, 20f, 0.012f)
        var s0 = 0f; var x0 = 1f; var y0 = 1f
        scaleRuler.onStart = {
            val st = lastState
            x0 = st?.t?.get(2) ?: 1f; y0 = st?.t?.get(3) ?: 1f
            if (currentChannel != 3 && currentChannel != 4) selectChannel(3)
            s0 = if (currentChannel == 3) x0 else y0
            NativeEngine.nativeGesture(true)
        }
        scaleRuler.onChange = { v ->
            val ratio = if (Math.abs(s0) > 1e-4f) v / s0 else 1f
            if (currentChannel == 3) {
                NativeEngine.nativeSetParam(3, v)
                if (scaleLock) NativeEngine.nativeSetParam(4, y0 * ratio)
            } else {
                NativeEngine.nativeSetParam(4, v)
                if (scaleLock) NativeEngine.nativeSetParam(3, x0 * ratio)
            }
        }
        scaleRuler.onStop = { NativeEngine.nativeGesture(false) }
        scaleRuler.active = true
        scale.addView(scaleRuler, lp(-1, dp(64)).apply { topMargin = dp(8) })

        // 4) Pusat (titik tumpu): X, Y + penggaris
        val anchor = LinearLayout(this)
        anchor.orientation = LinearLayout.VERTICAL
        for ((nm, ch) in listOf("Pusat X" to 7, "Pusat Y" to 8)) anchorBoxes.add(numBoxFor(nm, ch))
        anchor.addView(boxRow(anchorBoxes), lp(-1, -2))
        anchorRuler = RulerView(this)
        anchorRuler.setRange(-3000f, 3000f, 2f)
        anchorRuler.onStart = {
            if (currentChannel != 7 && currentChannel != 8) selectChannel(7)
            NativeEngine.nativeGesture(true)
        }
        anchorRuler.onChange = { v -> NativeEngine.nativeSetParam(currentChannel, v) }
        anchorRuler.onStop = { NativeEngine.nativeGesture(false) }
        anchorRuler.active = true
        anchor.addView(anchorRuler, lp(-1, dp(64)).apply { topMargin = dp(18) })

        for (v in listOf<View>(move, rotWrap, scale, anchor)) {
            tSubViews.add(v)
            content.addView(v, lp(-1, -2))
        }
        page.addView(content, lp(0, -2, 1f))

        val sub = LinearLayout(this)
        sub.orientation = LinearLayout.VERTICAL
        sub.gravity = Gravity.CENTER_HORIZONTAL
        sub.setPadding(dp(4), dp(0), dp(4), 0)
        for ((idx, kind) in listOf(IconView.MOVE, IconView.ROTATE, IconView.SCALE, IconView.ANCHOR).withIndex()) {
            val b = IconView(this, kind, false, true)
            b.setOnClickListener { setTSub(idx) }
            tSubBtns.add(b)
            sub.addView(b, lp(dp(46), dp(46)).apply { bottomMargin = dp(4) })
        }
        page.addView(sub, lp(dp(56), -2))
        setTSub(0)
        return page
    }

    private fun updateTransformTab(s: LState) {
        val t = s.t
        val f = currentChannel
        railKey.keyed = s.keyedT.contains(f)
        moveBoxes[0].setValue(t[0]); moveBoxes[1].setValue(t[1]); moveBoxes[2].setValue(t[6])
        for (k in 0..2) moveBoxes[k].setFocused(f == k)
        dial.angle = t[4]
        rotBox.setValue(t[4], "°"); rotBox.setFocused(f == 5)
        scaleBoxes[0].setValue(t[2]); scaleBoxes[1].setValue(t[3])
        scaleBoxes[0].setFocused(f == 3); scaleBoxes[1].setFocused(f == 4)
        scaleRuler.setValueSilent(if (f == 4) t[3] else t[2])
        anchorBoxes[0].setValue(t[7]); anchorBoxes[1].setValue(t[8])
        anchorBoxes[0].setFocused(f == 7); anchorBoxes[1].setFocused(f == 8)
        anchorRuler.setValueSilent(if (f == 8) t[8] else t[7])
    }

    // ---------- Tab Gaya ----------
    private fun buildStylePage(): View {
        val box = pageBox()

        // Blok "Opasitas & Blend": opasitas + semua mode campur dalam satu tempat
        blendSection = LinearLayout(this)   // dipakai juga untuk menyembunyikan blend pada Grup/Null
        blendSection.orientation = LinearLayout.VERTICAL
        styleLabel = sectionLabel("Opasitas & Blend")
        box.addView(styleLabel)
        opacityRow = ParamRow(this, "Opasitas", 0f, 1f, 0, 1f, 6, { selectChannel(it) }, { NativeEngine.nativeSetParam(6, it) })
        box.addView(opacityRow.view, lp(-1, -2))
        blendBox = LinearLayout(this)
        blendBox.orientation = LinearLayout.VERTICAL
        blendSection.addView(blendBox, lp(-1, -2))
        box.addView(blendSection, lp(-1, -2))

        colorSection = LinearLayout(this)
        colorSection.orientation = LinearLayout.VERTICAL
        colorSection.addView(sectionLabel("Warna"))
        val sw = HorizontalScrollView(this)
        sw.isHorizontalScrollBarEnabled = false
        val swRow = LinearLayout(this)
        palette.forEach { c ->
            val v = View(this)
            v.background = rounded(c, 20f, Ui.HAIR, 2)
            v.setOnClickListener { NativeEngine.nativeSetColor(c) }
            swRow.addView(v, lp(dp(34), dp(34)).apply { rightMargin = dp(10) })
        }
        swRow.addView(chip("Kustom…", false) { customColor() }, lp(-2, dp(34)))
        sw.addView(swRow)
        colorSection.addView(sw, lp(-1, -2))
        box.addView(colorSection, lp(-1, -2))

        shapeSection = LinearLayout(this)
        shapeSection.orientation = LinearLayout.VERTICAL
        shapeSection.addView(sectionLabel("Bentuk"))
        val shScroll = HorizontalScrollView(this)
        shScroll.isHorizontalScrollBarEnabled = false
        val shRow = LinearLayout(this)
        shapeNames.forEachIndexed { i, n ->
            val c = chip(n, false) { applyShape(kind = i) }
            shapeChips.add(c)
            shRow.addView(c, lp(-2, dp(34)).apply { rightMargin = dp(8) })
        }
        shScroll.addView(shRow)
        shapeSection.addView(shScroll, lp(-1, -2))
        cornerRow = SliderRow(this, "Kelengkungan sudut", 0f, 400f, 0, 0f, { applyShape(corner = it) }, null)
        sidesRow = SliderRow(this, "Jumlah sisi / ujung", 3f, 16f, 1, 5f, { applyShape(sides = it.toInt()) }, null)
        innerRow = SliderRow(this, "Kedalaman bintang", 0.05f, 1f, 0, 0.5f, { applyShape(inner = it) }, null)
        shapeSection.addView(cornerRow.view)
        shapeSection.addView(sidesRow.view)
        shapeSection.addView(innerRow.view)
        box.addView(shapeSection, lp(-1, -2))

        return box
    }

    private fun updateStyleTab(s: LState) {
        colorSection.visibility = if (s.type == 0 || s.type == 6) View.VISIBLE else View.GONE
        shapeSection.visibility = if (s.type == 0) View.VISIBLE else View.GONE
        blendSection.visibility = if (s.type == 1 || s.type == 2 || s.type == 5) View.GONE else View.VISIBLE
        styleLabel.text = if (s.type == 5) "Volume" else "Opasitas & Blend"
        opacityRow.rename(if (s.type == 5) "Volume" else "Opasitas")

        shapeChips.forEachIndexed { i, c -> styleChip(c, i == s.shape) }
        cornerRow.view.visibility = if (s.shape == 1) View.VISIBLE else View.GONE
        sidesRow.view.visibility = if (s.shape == 3 || s.shape == 4) View.VISIBLE else View.GONE
        innerRow.view.visibility = if (s.shape == 4) View.VISIBLE else View.GONE
        cornerRow.set(s.corner)
        sidesRow.set(s.sides.toFloat())
        innerRow.set(s.inner)
        opacityRow.set(s.t[5])
        opacityRow.setKeyed(s.keyedT.contains(6))
        opacityRow.setFocused(currentChannel == 6)
        railKey.keyed = s.keyedT.contains(currentChannel)
        refreshBlend(s.blend)
    }

    /** Daftar blend gaya Alight Motion: Normal + kategori yang bisa dibuka, tiap mode punya gambar contoh. */
    private fun refreshBlend(cur: Int) {
        val sig = cur * 100 + blendOpen.hashCode() % 97
        if (sig == blendSig) return
        blendSig = sig
        blendBox.removeAllViews()
        fun row(text: String, selected: Boolean, onClick: () -> Unit): LinearLayout {
            val r = LinearLayout(this)
            r.gravity = Gravity.CENTER_VERTICAL
            r.setPadding(dp(12), dp(10), dp(12), dp(10))
            r.addView(label(text, 13.5f, Ui.TEXT), lp(0, -2, 1f))
            if (selected) r.addView(IconView(this, IconView.CHECK, false, true).apply { tint = Ui.ACCENT_SOFT; isClickable = false },
                lp(dp(24), dp(24)))
            r.setOnClickListener { onClick() }
            return r
        }
        val normal = LinearLayout(this)
        normal.orientation = LinearLayout.VERTICAL
        normal.background = card(14f)
        normal.addView(row("Normal", cur == 0) { NativeEngine.nativeSetBlend(0) })
        blendBox.addView(normal, lp(-1, -2).apply { topMargin = dp(8); bottomMargin = dp(8) })
        for ((gname, modes) in blendGroups) {
            val open = blendOpen == gname
            val gcard = LinearLayout(this)
            gcard.orientation = LinearLayout.VERTICAL
            gcard.background = card(14f)
            val head = LinearLayout(this)
            head.gravity = Gravity.CENTER_VERTICAL
            head.setPadding(dp(10), dp(10), dp(12), dp(10))
            head.addView(IconView(this, if (open) IconView.CHEV_DOWN else IconView.CHEV_RIGHT, false, true)
                .apply { isClickable = false }, lp(dp(26), dp(26)))
            head.addView(label(gname, 14f, Ui.TEXT, true), lp(0, -2, 1f).apply { leftMargin = dp(6) })
            if (modes.contains(cur)) head.addView(label(blendNames[cur], 11f, Ui.ACCENT_SOFT))
            head.setOnClickListener { blendOpen = if (open) "" else gname; blendSig = -2; refreshBlend(cur) }
            gcard.addView(head, lp(-1, -2))
            if (open) {
                val hs = HorizontalScrollView(this)
                hs.isHorizontalScrollBarEnabled = false
                val rowL = LinearLayout(this)
                rowL.setPadding(dp(10), 0, dp(10), dp(8))
                for (m in modes) {
                    val cell = LinearLayout(this)
                    cell.orientation = LinearLayout.VERTICAL
                    cell.gravity = Gravity.CENTER_HORIZONTAL
                    val iv = ImageView(this)
                    iv.scaleType = ImageView.ScaleType.CENTER_CROP
                    iv.setImageBitmap(BlendThumbs.get(this, m))
                    roundClip(iv, dpf(12f))
                    iv.foreground = if (m == cur) rounded(0, 12f, Ui.ACCENT_SOFT, 3) else null
                    cell.addView(iv, lp(dp(72), dp(72)))
                    val t = label(blendNames[m], 10.5f, if (m == cur) Ui.ACCENT_SOFT else Ui.DIM)
                    t.gravity = Gravity.CENTER
                    t.maxLines = 2
                    t.setPadding(0, dp(3), 0, 0)
                    cell.addView(t, lp(dp(72), -2))
                    cell.setOnClickListener { NativeEngine.nativeSetBlend(m) }
                    rowL.addView(cell, lp(dp(72), -2).apply { rightMargin = dp(10) })
                }
                hs.addView(rowL)
                gcard.addView(hs, lp(-1, -2))
            }
            blendBox.addView(gcard, lp(-1, -2).apply { bottomMargin = dp(8) })
        }
    }

    private fun applyShape(kind: Int = -1, corner: Float = Float.NaN, sides: Int = -1, inner: Float = Float.NaN) {
        val s = lastState ?: return
        NativeEngine.nativeSetShape(
            if (kind >= 0) kind else s.shape,
            if (!corner.isNaN()) corner else s.corner,
            if (sides >= 0) sides else s.sides,
            if (!inner.isNaN()) inner else s.inner)
    }

    private fun customColor() {
        val rgb = intArrayOf(200, 200, 200)
        showSheet(this, "Warna kustom", { box, close ->
            val preview = View(this)
            fun refresh() { preview.background = rounded(Color.rgb(rgb[0], rgb[1], rgb[2]), 14f) }
            refresh()
            box.addView(preview, lp(-1, dp(52)).apply { bottomMargin = dp(8) })
            val names = listOf("Merah", "Hijau", "Biru")
            for (k in 0 until 3) {
                val r = SliderRow(this, names[k], 0f, 255f, 1, null, { rgb[k] = it.toInt().coerceIn(0, 255); refresh() }, null)
                r.set(rgb[k].toFloat())
                box.addView(r.view)
            }
            box.addView(button("Terapkan", 1) {
                NativeEngine.nativeSetColor(Color.rgb(rgb[0], rgb[1], rgb[2]))
                close()
            }, lp(-1, -2).apply { topMargin = dp(12) })
        })
    }

    // ---------- Tab Efek ----------
    private fun buildFxPage(): View {
        val box = pageBox()
        box.addView(button("+  Tambah efek", 1) {
            showEffectPicker(this, catalog.values) { NativeEngine.nativeAddEffect(it) }
        }, lp(-1, -2).apply { topMargin = dp(4); bottomMargin = dp(8) })
        fxList = LinearLayout(this)
        fxList.orientation = LinearLayout.VERTICAL
        box.addView(fxList, lp(-1, -2))
        return box
    }

    private fun updateFxTab(s: LState) {
        val sig = s.id.toString() + "|" + s.fx.joinToString(",") { it.type + it.enabled } + "|" + collapsed.size
        if (sig != fxSignature) {
            fxSignature = sig
            rebuildFx(s)
        }
        for ((idx, row) in fxRows) {
            val fx = s.fx.getOrNull(idx) ?: continue
            if (row.flat < fx.vals.size) row.set(fx.vals[row.flat])
            row.setKeyed(fx.keyed.contains(row.flat))
            row.setFocused(row.channel == currentChannel)
        }
        railKey.keyed = if (currentChannel >= 256)
            s.fx.any { f -> f.keyed.any { 256 + f.idx * 32 + it == currentChannel } } else false
    }

    private fun rebuildFx(s: LState) {
        fxList.removeAllViews()
        fxRows.clear()
        if (s.type == 1 || s.type == 2) {
            fxList.addView(label("Efek tidak berlaku untuk Grup/Null.", 12f, Ui.DIM))
            return
        }
        if (s.fx.isEmpty()) {
            fxList.addView(label("Belum ada efek. Ketuk “Tambah efek” untuk memilih dari katalog bergambar.", 12.5f, Ui.DIM)
                .apply { setPadding(dp(4), dp(8), dp(4), dp(8)) })
            return
        }
        for (fx in s.fx) {
            val def = catalog[fx.type]
            val key = s.id.toString() + ":" + fx.idx
            val isCollapsed = collapsed.contains(key)
            val card = LinearLayout(this)
            card.orientation = LinearLayout.VERTICAL
            card.background = this@MainActivity.card(16f)
            card.elevation = dpf(2f)
            card.setPadding(dp(10), dp(10), dp(8), dp(6))

            val head = LinearLayout(this)
            head.gravity = Gravity.CENTER_VERTICAL
            val i = fx.idx
            head.addView(thumbView(this, fx.type, 40), lp(dp(40), dp(40)).apply { rightMargin = dp(10) })
            val tx = LinearLayout(this)
            tx.orientation = LinearLayout.VERTICAL
            tx.addView(label(def?.label ?: fx.type, 13f, if (fx.enabled) Ui.TEXT else Ui.DIM, true).apply {
                setSingleLine(); ellipsize = android.text.TextUtils.TruncateAt.END
            })
            tx.addView(label(def?.category ?: "", 10.5f, Ui.DIM))
            head.addView(tx, lp(0, -2, 1f))
            val onOff = chip(if (fx.enabled) "Aktif" else "Mati", fx.enabled) {
                NativeEngine.nativeSetEffectEnabled(i, !fx.enabled)
            }
            onOff.minHeight = dp(28)
            onOff.textSize = 11f
            onOff.setPadding(dp(10), dp(3), dp(10), dp(3))
            head.addView(onOff, lp(-2, dp(28)).apply { rightMargin = dp(4) })
            val more = IconView(this, if (isCollapsed) IconView.CHEV_DOWN else IconView.UP, false, true)
            more.setOnClickListener { if (isCollapsed) collapsed.remove(key) else collapsed.add(key) }
            head.addView(more, lp(dp(32), dp(32)))
            card.addView(head, lp(-1, -2))

            if (def != null && !isCollapsed) {
                for (p in def.params) {
                    val ch = 256 + fx.idx * 32 + p.flat
                    val row = ParamRow(this, p.label, p.min, p.max, p.kind, p.def, ch,
                        { c -> selectChannel(c) }, { NativeEngine.nativeSetParam(ch, it) })
                    row.flat = p.flat
                    fxRows.add(Pair(fx.idx, row))
                    card.addView(row.view, lp(-1, -2))
                }
                val acts = LinearLayout(this)
                acts.gravity = Gravity.END
                val up = IconView(this, IconView.UP, false, true)
                up.setOnClickListener { if (i > 0) NativeEngine.nativeMoveEffect(i, i - 1) }
                val down = IconView(this, IconView.DOWN, false, true)
                down.setOnClickListener { NativeEngine.nativeMoveEffect(i, i + 1) }
                val rm = IconView(this, IconView.TRASH, false, true)
                rm.tint = Ui.DANGER
                rm.setOnClickListener { NativeEngine.nativeRemoveEffect(i) }
                acts.addView(up, lp(dp(34), dp(34)).apply { rightMargin = dp(6) })
                acts.addView(down, lp(dp(34), dp(34)).apply { rightMargin = dp(6) })
                acts.addView(rm, lp(dp(34), dp(34)))
                card.addView(acts, lp(-1, -2).apply { topMargin = dp(6) })
            }
            fxList.addView(card, lp(-1, -2).apply { bottomMargin = dp(8) })
        }
    }

    // ---------- Tab Grafik ----------
    private fun buildGraphPage(): View {
        val box = pageBox()
        // Baris atas: pilih kanal + tombol keyframe ringkas
        val top = LinearLayout(this)
        top.gravity = Gravity.CENTER_VERTICAL
        channelBtn = chip("Posisi X", false) { }
        channelBtn.gravity = Gravity.CENTER_VERTICAL
        top.addView(channelBtn, lp(0, dp(34), 1f).apply { rightMargin = dp(8) })
        fun small(kind: Int, onClick: () -> Unit): IconView {
            val v = IconView(this, kind)
            v.setOnClickListener { onClick() }
            top.addView(v, lp(dp(34), dp(34)).apply { leftMargin = dp(6) })
            return v
        }
        small(IconView.CHEV_LEFT) { graph.jumpKey(-1) }
        small(IconView.CHEV_RIGHT) { graph.jumpKey(1) }
        small(IconView.PLUS) { NativeEngine.nativeKeyHere(false) }
        small(IconView.FULL) { graph.fit() }
        box.addView(top, lp(-1, -2).apply { topMargin = dp(2); bottomMargin = dp(8) })

        graph = GraphView(this)
        roundClip(graph, dpf(14f))
        box.addView(graph, lp(-1, dp(220)))

        fun onSel(op: Int, nf: Int, a: Float, b: Float, c: Float, d: Float) {
            val f = graph.selectedKeyFrame
            if (f != Int.MIN_VALUE) NativeEngine.nativeKeyEdit(op, f, nf, a, b, c, d)
            else toast("Ketuk sebuah titik keyframe di grafik dulu")
        }

        // Pilihan kurva bergambar (ketuk = terapkan ke keyframe terpilih; lalu tarik pegangan untuk menyempurnakan)
        val head = LinearLayout(this)
        head.gravity = Gravity.CENTER_VERTICAL
        head.addView(sectionLabel("Kurva"), lp(0, -2, 1f))
        val smoothChip = chip("Tangen mulus", true) {}
        smoothChip.minHeight = dp(26)
        smoothChip.textSize = 10.5f
        smoothChip.setPadding(dp(10), dp(2), dp(10), dp(2))
        smoothChip.setOnClickListener {
            graph.smooth = !graph.smooth
            styleChip(smoothChip, graph.smooth)
        }
        head.addView(smoothChip, lp(-2, dp(26)))
        box.addView(head, lp(-1, -2))

        val sc = HorizontalScrollView(this)
        sc.isHorizontalScrollBarEnabled = false
        val row = LinearLayout(this)
        fun tile(k: Int, x1: Float, y1: Float, x2: Float, y2: Float, name: String, f: () -> Unit) {
            val t = CurveTile(this, k, x1, y1, x2, y2, name)
            t.setOnClickListener { f() }
            row.addView(t, lp(dp(64), dp(60)).apply { rightMargin = dp(8) })
        }
        tile(0, 0f, 0f, 0f, 0f, "Tahan") { onSel(3, 0, 0f, 0f, 0f, 0f) }
        tile(1, 0f, 0f, 1f, 1f, "Lurus") { onSel(3, 1, 0f, 0f, 0f, 0f) }
        tile(2, 0.33f, 0f, 0.67f, 1f, "Easy Ease") { onSel(5, 0, 0f, 0f, 0f, 0f) }
        tile(2, 0.42f, 0f, 1f, 1f, "Ease in") { onSel(4, 0, 0.42f, 0f, 1f, 1f) }
        tile(2, 0f, 0f, 0.58f, 1f, "Ease out") { onSel(4, 0, 0f, 0f, 0.58f, 1f) }
        tile(2, 0.37f, 0f, 0.63f, 1f, "Sine") { onSel(4, 0, 0.37f, 0f, 0.63f, 1f) }
        tile(2, 0.16f, 1f, 0.3f, 1f, "Cepat") { onSel(4, 0, 0.16f, 1f, 0.3f, 1f) }
        tile(2, 0.36f, 0f, 0.66f, -0.56f, "Mundur") { onSel(4, 0, 0.36f, 0f, 0.66f, -0.56f) }
        tile(2, 0.34f, 1.56f, 0.64f, 1f, "Melampaui") { onSel(4, 0, 0.34f, 1.56f, 0.64f, 1f) }
        tile(2, 0.3f, 0.2f, 0.7f, 0.8f, "Mulus") { onSel(5, 3, 0f, 0f, 0f, 0f) }
        sc.addView(row)
        box.addView(sc, lp(-1, -2))

        val del = chip("Hapus keyframe", false) {
            val f = graph.selectedKeyFrame
            if (f != Int.MIN_VALUE) NativeEngine.nativeKeyEdit(2, f, 0, 0f, 0f, 0f, 0f)
            else toast("Ketuk sebuah titik keyframe di grafik dulu")
        }
        del.setTextColor(Ui.DANGER)
        box.addView(del, lp(-2, dp(34)).apply { topMargin = dp(10) })
        box.addView(label("Seret titik = pindah · seret pegangan kuning = bentuk kurva · cubit = zoom",
            10.5f, Ui.DIM).apply { setPadding(dp(2), dp(8), dp(2), 0) })
        return box
    }

    private fun channelName(ch: Int): String {
        chNames[ch]?.let { return it }
        if (ch == TIME_CH) return "Kecepatan (kurva waktu)"
        if (ch < 0) return ""
        if (ch >= 256) {
            val fi = (ch - 256) / 32
            val pi = (ch - 256) % 32
            val fx = lastState?.fx?.getOrNull(fi)
            val def = fx?.let { catalog[it.type] }
            val pn = def?.params?.firstOrNull { it.flat == pi }?.label ?: "param $pi"
            return "${def?.label ?: "Efek"} · $pn"
        }
        return "Kanal $ch"
    }

    private fun selectChannel(ch: Int) {
        currentChannel = ch
        NativeEngine.nativeSelectProp(ch)
        graph.setProp(ch)
    }

    private fun updateGraphTab() {
        channelBtn.text = channelName(currentChannel)
    }

    // ---------- Diagnostik ----------
    private fun showDiag() {
        showSheet(this, "Diagnostik GPU & efek", { box, _ ->
            val tv = TextView(this)
            tv.setTextColor(Color.rgb(170, 230, 190))
            tv.textSize = 10f
            tv.typeface = Typeface.MONOSPACE
            tv.setTextIsSelectable(true)
            tv.text = NativeEngine.nativeDiagnostics()
            val sc = ScrollView(this)
            sc.addView(tv)
            box.addView(sc, lp(-1, dp(300)).apply { bottomMargin = dp(10) })
            val row = LinearLayout(this)
            row.addView(button("Uji semua efek") {
                NativeEngine.nativeRequestSelfTest()
                handler.postDelayed({ tv.text = NativeEngine.nativeDiagnostics() }, 800)
            }, lp(0, -2, 1f).apply { rightMargin = dp(8) })
            row.addView(button("Salin") {
                val cm = getSystemService(CLIPBOARD_SERVICE) as ClipboardManager
                cm.setPrimaryClip(ClipData.newPlainText("diagnostik", tv.text))
                toast("Diagnostik disalin")
            }, lp(0, -2, 1f))
            box.addView(row, lp(-1, -2))
        })
    }

    // ---------- Gambar ----------
    @Suppress("DEPRECATION")
    private fun pickImage() {
        val i = Intent(Intent.ACTION_GET_CONTENT)
        i.type = "image/*"
        i.addCategory(Intent.CATEGORY_OPENABLE)
        startActivityForResult(Intent.createChooser(i, "Pilih gambar"), 11)
    }

    /** Border & bayangan = pintasan yang menambahkan efek outline / drop shadow / glow lalu membuka tab Efek. */
    private fun showBorderMenu() {
        fun add(type: String) {
            if (!catalog.containsKey(type)) { toast("Efek tidak tersedia di perangkat ini"); return }
            NativeEngine.nativeAddEffect(type)
            openTab(2)
        }
        showActions(this, "Border & Bayangan", listOf(
            SheetAction(IconView.SCALE, "Border", "Garis tepi di sekeliling bentuk, teks, atau gambar") { add("outline") },
            SheetAction(IconView.STYLE, "Bayangan", "Bayangan jatuh di belakang layer") { add("drop_shadow") },
            SheetAction(IconView.SPARK, "Cahaya (glow)", "Pendar lembut di sekeliling layer") { add("glow") }))
    }

    /** Preset = gaya layer yang disimpan permanen (efek, warna, blend, animasi) untuk ditempel ke layer lain. */
    private fun showPresetMenu() {
        if (lastState == null) { toast("Pilih layer dulu"); return }
        showSheet(this, "Preset", { box, close ->
            box.addView(label("Simpan efek dan gaya dari sebuah layer, lalu tempel ke layer mana pun kapan saja — seperti salin, tetapi tersimpan permanen.",
                12f, Ui.DIM).apply { setLineSpacing(0f, 1.15f) }, lp(-1, -2).apply { bottomMargin = dp(10) })
            val save = LinearLayout(this)
            save.gravity = Gravity.CENTER_VERTICAL
            save.background = card(14f)
            save.setPadding(dp(10), dp(8), dp(10), dp(8))
            val ic = IconView(this, IconView.PLUS, false, true)
            ic.isClickable = false
            ic.tint = Ui.ACCENT_SOFT
            ic.background = rounded(Ui.SURFACE3, 12f)
            save.addView(ic, lp(dp(38), dp(38)).apply { rightMargin = dp(12) })
            val tx = LinearLayout(this)
            tx.orientation = LinearLayout.VERTICAL
            tx.addView(label("Simpan layer ini sebagai preset", 13.5f, Ui.TEXT, true))
            tx.addView(label("Pilih bagian yang disimpan: efek, warna, blend, animasi", 11.5f, Ui.DIM))
            save.addView(tx, lp(0, -2, 1f))
            save.setOnClickListener { close(); showSavePreset() }
            box.addView(save, lp(-1, -2).apply { bottomMargin = dp(12) })

            box.addView(label("Preset saya", 13f, Ui.TEXT, true), lp(-1, -2).apply { bottomMargin = dp(6) })
            val items = PresetStore.list(this)
            if (items.isEmpty()) {
                box.addView(label("Belum ada preset. Pilih layer yang sudah diberi efek, lalu simpan di atas.", 12f, Ui.DIM),
                    lp(-1, -2).apply { bottomMargin = dp(10) })
            }
            for (it in items) {
                val row = LinearLayout(this)
                row.gravity = Gravity.CENTER_VERTICAL
                row.background = card(14f)
                row.setPadding(dp(14), dp(10), dp(12), dp(10))
                val col = LinearLayout(this)
                col.orientation = LinearLayout.VERTICAL
                col.addView(label(it.name, 13.5f, Ui.TEXT, true).apply { setSingleLine(); ellipsize = android.text.TextUtils.TruncateAt.END })
                val parts = ArrayList<String>()
                if (it.mask and PresetStore.M_FX != 0) parts.add("Efek")
                if (it.mask and PresetStore.M_COLOR != 0) parts.add("Warna")
                if (it.mask and PresetStore.M_BLEND != 0) parts.add("Blend")
                if (it.mask and PresetStore.M_TRANSFORM != 0) parts.add("Transform & animasi")
                col.addView(label(parts.joinToString(" · "), 11f, Ui.DIM))
                row.addView(col, lp(0, -2, 1f))
                row.addView(label("Terapkan", 11.5f, Ui.ACCENT_SOFT, true))
                run {
                    val item = it
                    row.setOnClickListener {
                        close()
                        NativeEngine.nativePresetApply(item.text, item.mask)
                    }
                    row.setOnLongClickListener {
                        close()
                        showActions(this, item.name, listOf(
                            SheetAction(IconView.TEXT, "Ganti nama", "") {
                                showNameInput("Ganti nama preset", item.name) { n -> PresetStore.rename(this, item, n); toast("Nama diganti") }
                            },
                            SheetAction(IconView.TRASH, "Hapus preset", "Tidak bisa dibatalkan", true) {
                                PresetStore.delete(this, item.id); toast("Preset dihapus")
                            }))
                        true
                    }
                }
                box.addView(row, lp(-1, -2).apply { bottomMargin = dp(8) })
            }
            if (items.isNotEmpty()) box.addView(label("Ketuk untuk menerapkan ke layer terpilih · tahan untuk ganti nama / hapus", 10.5f, Ui.DIM),
                lp(-1, -2).apply { bottomMargin = dp(10) })

            val anim = LinearLayout(this)
            anim.gravity = Gravity.CENTER_VERTICAL
            anim.background = card(14f)
            anim.setPadding(dp(14), dp(10), dp(12), dp(10))
            val at = LinearLayout(this)
            at.orientation = LinearLayout.VERTICAL
            at.addView(label("Animasi bawaan", 13.5f, Ui.TEXT, true))
            at.addView(label("Masuk, keluar, dan loop siap pakai", 11f, Ui.DIM))
            anim.addView(at, lp(0, -2, 1f))
            anim.addView(IconView(this, IconView.CHEV_RIGHT, false, true).apply { isClickable = false }, lp(dp(26), dp(26)))
            anim.setOnClickListener { close(); showBuiltinAnimMenu() }
            box.addView(anim, lp(-1, -2))
        })
    }

    private fun showSavePreset() {
        val st = lastState ?: return
        showSheet(this, "Simpan preset", { box, close ->
            val et = EditText(this)
            et.setText(st.name)
            et.setSingleLine()
            et.setTextColor(Ui.TEXT)
            et.textSize = 15f
            et.background = card(12f)
            et.setPadding(dp(12), dp(10), dp(12), dp(10))
            et.setSelectAllOnFocus(true)
            box.addView(label("Nama preset", 12f, Ui.DIM), lp(-1, -2).apply { bottomMargin = dp(4) })
            box.addView(et, lp(-1, -2).apply { bottomMargin = dp(12) })
            box.addView(label("Bagian yang disimpan", 12f, Ui.DIM), lp(-1, -2).apply { bottomMargin = dp(6) })
            var mask = PresetStore.M_FX or PresetStore.M_COLOR or PresetStore.M_BLEND
            fun chip(text: String, bit: Int): TextView {
                val tv = TextView(this)
                tv.text = text
                tv.textSize = 12.5f
                tv.gravity = Gravity.CENTER
                tv.setPadding(dp(12), dp(9), dp(12), dp(9))
                fun paint() {
                    val on = mask and bit != 0
                    tv.setTextColor(if (on) Color.WHITE else Ui.TEXT)
                    tv.background = if (on) accentFill(12f) else card(12f)
                    tv.text = (if (on) "✓ " else "") + text
                }
                paint()
                tv.setOnClickListener { mask = mask xor bit; paint() }
                return tv
            }
            val r1 = LinearLayout(this)
            r1.addView(chip("Efek", PresetStore.M_FX), lp(0, -2, 1f).apply { rightMargin = dp(6) })
            r1.addView(chip("Warna & isi", PresetStore.M_COLOR), lp(0, -2, 1f))
            box.addView(r1, lp(-1, -2).apply { bottomMargin = dp(6) })
            val r2 = LinearLayout(this)
            r2.addView(chip("Blend & opasitas", PresetStore.M_BLEND), lp(0, -2, 1f).apply { rightMargin = dp(6) })
            r2.addView(chip("Transform & animasi", PresetStore.M_TRANSFORM), lp(0, -2, 1f))
            box.addView(r2, lp(-1, -2).apply { bottomMargin = dp(14) })
            val ok = TextView(this)
            ok.text = "Simpan preset"
            ok.setTextColor(Color.WHITE)
            ok.textSize = 13f
            ok.typeface = Ui.MEDIUM
            ok.gravity = Gravity.CENTER
            ok.background = accentFill(14f)
            ok.setOnClickListener {
                val txt = NativeEngine.nativePresetExport()
                if (mask == 0) toast("Pilih minimal satu bagian")
                else if (!PresetStore.save(this, et.text.toString(), mask, txt)) toast("Gagal menyimpan preset")
                else { toast("Preset tersimpan"); close() }
            }
            box.addView(ok, lp(-1, dp(42)))
        }, false, true)
    }

    private fun showNameInput(title: String, initial: String, onOk: (String) -> Unit) {
        showSheet(this, title, { box, close ->
            val et = EditText(this)
            et.setText(initial)
            et.setSingleLine()
            et.setTextColor(Ui.TEXT)
            et.textSize = 15f
            et.background = card(12f)
            et.setPadding(dp(12), dp(10), dp(12), dp(10))
            et.setSelectAllOnFocus(true)
            box.addView(et, lp(-1, -2).apply { bottomMargin = dp(10) })
            val ok = TextView(this)
            ok.text = "Simpan"
            ok.setTextColor(Color.WHITE)
            ok.textSize = 13f
            ok.typeface = Ui.MEDIUM
            ok.gravity = Gravity.CENTER
            ok.background = accentFill(14f)
            ok.setOnClickListener { val n = et.text.toString().trim(); if (n.isNotEmpty()) onOk(n); close() }
            box.addView(ok, lp(-1, dp(42)))
        }, false, true)
    }

    /** Animasi bawaan: masuk / keluar / loop, dengan pengatur durasi. Satu langkah undo. */
    private fun showBuiltinAnimMenu() {
        if (lastState == null) { toast("Pilih layer dulu"); return }
        val items = NativeEngine.nativePresetCatalog().split("\n").filter { it.isNotEmpty() }.mapNotNull {
            val p = it.split("|", limit = 3)
            if (p.size < 3) null else Triple(p[0], p[1].toIntOrNull() ?: 0, p[2])
        }
        val titles = listOf("Masuk", "Keluar", "Loop")
        showSheet(this, "Animasi bawaan", { box, close ->
            var secs = 0.6
            val dur = label("Durasi: 0,6 dtk", 12f, Ui.DIM)
            val bar = SeekBar(this)
            bar.max = 38                       // 0,2 .. 2,0 dtk, langkah 0,05
            bar.progress = 8
            bar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar?, v: Int, user: Boolean) {
                    secs = 0.2 + v * 0.05
                    dur.text = "Durasi: " + String.format(java.util.Locale.US, "%.2f", secs).replace('.', ',') + " dtk"
                }
                override fun onStartTrackingTouch(sb: SeekBar?) {}
                override fun onStopTrackingTouch(sb: SeekBar?) {}
            })
            box.addView(dur, lp(-1, -2))
            box.addView(bar, lp(-1, -2).apply { bottomMargin = dp(8) })
            box.addView(label("Loop: durasi = setengah siklus", 11f, Ui.DIM), lp(-1, -2).apply { bottomMargin = dp(6) })
            for (cat in 0..2) {
                box.addView(label(titles[cat], 13f, Ui.TEXT, true), lp(-1, -2).apply { topMargin = dp(6); bottomMargin = dp(6) })
                val inCat = items.withIndex().filter { it.value.second == cat }
                var k = 0
                while (k < inCat.size) {
                    val r = LinearLayout(this)
                    for (j in 0 until 3) {
                        if (k + j >= inCat.size) { r.addView(View(this), lp(0, dp(40), 1f)); continue }
                        val ent = inCat[k + j]
                        val b = TextView(this)
                        b.text = ent.value.third
                        b.textSize = 12.5f
                        b.setTextColor(Ui.TEXT)
                        b.gravity = Gravity.CENTER
                        b.background = card(12f)
                        b.setOnClickListener {
                            NativeEngine.nativeApplyPreset(ent.index, secs)
                            close()
                        }
                        r.addView(b, lp(0, dp(40), 1f).apply { if (j < 2) rightMargin = dp(6) })
                    }
                    box.addView(r, lp(-1, -2).apply { bottomMargin = dp(6) })
                    k += 3
                }
            }
        })
    }
    // ---------- Masker, Induk, Grup, Pilih banyak ----------
    private fun typeLabel(t: Int) = when (t) { 0 -> "Bentuk"; 1 -> "Grup"; 2 -> "Null"; 3 -> "Gambar"; 4 -> "Video"; 6 -> "Teks"; else -> "Layer" }

    /** Masker: layer ini menjadi "jendela" bagi layer tepat di bawahnya. */
    private fun showMatteMenu() {
        val info = NativeEngine.nativeMatteInfo().split("|", limit = 3)
        if (info.size < 3) { toast("Pilih layer dulu"); return }
        val cur = info[0].toIntOrNull() ?: 0
        val below = info[1]
        val status = info[2].toIntOrNull() ?: 0
        val note = when (status) {
            0 -> "Layer ini jadi bentuk pemotong. Layer tepat di bawahnya (\"$below\") hanya terlihat lewat bentuk ini. Layer masker sendiri tidak ditampilkan."
            1 -> "Tidak ada layer di bawah layer ini. Letakkan layer yang mau dipotong tepat di bawahnya (tahan dan seret layer di timeline untuk mengatur urutan)."
            2 -> "Layer di bawahnya (\"$below\") tidak bisa dipotong. Masker bekerja pada bentuk, gambar, video, atau teks."
            3 -> "Layer di bawahnya (\"$below\") sudah menjadi masker. Atur ulang urutan layer."
            else -> "Grup dan Null tidak bisa dipakai sebagai masker."
        }
        fun mark(m: Int, d: String) = if (cur == m) "Aktif · " + d else d
        val rows = ArrayList<SheetAction>()
        if (status == 0 || cur != 0) {
            rows.add(SheetAction(IconView.MASK, "Tampil di dalam bentuk", mark(1, "Layer bawah hanya terlihat di area bentuk ini")) { NativeEngine.nativeSetMatte(1) })
            rows.add(SheetAction(IconView.MASK, "Tampil di luar bentuk (balik)", mark(2, "Area bentuk ini dilubangi dari layer bawah")) { NativeEngine.nativeSetMatte(2) })
        }
        if (cur != 0) rows.add(SheetAction(IconView.CLOSE, "Matikan masker", "Layer ini tampil biasa lagi") { NativeEngine.nativeSetMatte(0) })
        if (rows.isEmpty()) rows.add(SheetAction(IconView.CHECK, "Mengerti", "Tutup") {})
        showActions(this, "Masker", rows, note)
    }

    /** Bilah atas saat layer terpilih: kembali, nama (ketuk = ganti nama), induk, hapus, menu ⋯ — seperti Alight Motion. */
    private fun buildLayerBar(): LinearLayout {
        val bar = LinearLayout(this)
        bar.gravity = Gravity.CENTER_VERTICAL
        bar.setBackgroundColor(Ui.BG)
        bar.setPadding(dp(12), 0, dp(10), 0)
        val back = IconView(this, IconView.CHEV_LEFT, false, true)
        back.setOnClickListener { NativeEngine.nativeSelectLayer(0) }
        bar.addView(back, lp(dp(36), dp(36)).apply { leftMargin = dp(-8); rightMargin = dp(4) })
        val titles = LinearLayout(this)
        titles.orientation = LinearLayout.VERTICAL
        layerNameTv = label("", 15f, Ui.TEXT, true)
        layerNameTv.setSingleLine()
        layerNameTv.ellipsize = android.text.TextUtils.TruncateAt.END
        layerSubTv = label("", 10.5f, Ui.DIM)
        layerSubTv.setSingleLine()
        titles.addView(layerNameTv)
        titles.addView(layerSubTv)
        titles.setOnClickListener { showRenameDialog() }
        bar.addView(titles, lp(0, -2, 1f))
        parentIcon = IconView(this, IconView.LINK, false, true)
        parentIcon.setOnClickListener { showParentPopup(parentIcon) }
        bar.addView(parentIcon, lp(dp(38), dp(38)))
        val del = IconView(this, IconView.TRASH, false, true)
        del.tint = Ui.DANGER
        del.setOnClickListener { NativeEngine.nativeCommand(2) }
        bar.addView(del, lp(dp(38), dp(38)).apply { leftMargin = dp(2) })
        val more = IconView(this, IconView.MORE, false, true)
        more.setOnClickListener { showLayerMenu() }
        bar.addView(more, lp(dp(38), dp(38)).apply { leftMargin = dp(2) })
        bar.visibility = View.GONE
        return bar
    }

    /** Bilah hijau saat Pilih banyak: X (selesai), jumlah terpilih, kelompokkan, gandakan, hapus. */
    private fun buildMultiTop(): LinearLayout {
        val bar = LinearLayout(this)
        bar.gravity = Gravity.CENTER_VERTICAL
        bar.setBackgroundColor(Color.rgb(120, 255, 176))
        bar.setPadding(dp(10), 0, dp(8), 0)
        val dark = Color.rgb(10, 40, 25)
        val x = IconView(this, IconView.CLOSE, false, true)
        x.tint = dark
        x.setOnClickListener { NativeEngine.nativeMultiMode(false) }
        bar.addView(x, lp(dp(38), dp(38)))
        multiCountTv = label("Memilih 0 layer", 14f, dark, true)
        bar.addView(multiCountTv, lp(0, -2, 1f).apply { leftMargin = dp(6) })
        fun act(kind: Int, onClick: () -> Unit) {
            val b = IconView(this, kind, false, true)
            b.tint = dark
            b.setOnClickListener { onClick() }
            bar.addView(b, lp(dp(40), dp(38)))
        }
        act(IconView.GROUP) { NativeEngine.nativeMultiAction(0) }
        act(IconView.COPY) { NativeEngine.nativeMultiAction(2) }
        act(IconView.TRASH) { NativeEngine.nativeMultiAction(1) }
        bar.visibility = View.GONE
        return bar
    }

    private fun updateMultiBar() {
        val n = NativeEngine.nativeMultiCount()
        timeline.multiMode = n >= 0
        val on = n >= 0 && !fullPreview
        multiShown = on
        if (on) {
            val t = if (n == 0) "Ketuk layer untuk memilih" else "Memilih $n layer"
            if (multiCountTv.text.toString() != t) multiCountTv.text = t
            if (currentTab >= 0) openTab(-1)
        }
        updateBars()
    }

    /** Hanya satu bilah atas yang tampil: proyek, layer terpilih, atau pilih banyak. */
    private fun updateBars() {
        val st = lastState
        val which = if (fullPreview) 0 else if (multiShown) 3 else if (st != null) 2 else 1
        fun vis(v: View, on: Boolean) { val w = if (on) View.VISIBLE else View.GONE; if (v.visibility != w) v.visibility = w }
        vis(topBarView, which == 1)
        vis(layerBar, which == 2)
        vis(multiTop, which == 3)
        if (which == 2 && st != null) {
            if (layerNameTv.text.toString() != st.name) layerNameTv.text = st.name
            val sub = typeLabel(st.type) + "  ·  " + Ui.time(st.inF, fps) + " – " + Ui.time(st.outF, fps)
            if (layerSubTv.text.toString() != sub) layerSubTv.text = sub
            parentIcon.alpha = if (st.type == 5) 0.35f else 1f
        }
    }

    private fun showRenameDialog() {
        val st = lastState ?: return
        showSheet(this, "Ganti nama layer", { box, close ->
            val et = EditText(this)
            et.setText(st.name)
            et.setSingleLine()
            et.setTextColor(Ui.TEXT)
            et.textSize = 15f
            et.background = card(12f)
            et.setPadding(dp(12), dp(10), dp(12), dp(10))
            et.setSelectAllOnFocus(true)
            box.addView(et, lp(-1, -2).apply { bottomMargin = dp(10) })
            val ok = TextView(this)
            ok.text = "Simpan"
            ok.setTextColor(Color.WHITE)
            ok.textSize = 13f
            ok.typeface = Ui.MEDIUM
            ok.gravity = Gravity.CENTER
            ok.background = accentFill(14f)
            ok.setOnClickListener {
                val n = et.text.toString().trim()
                if (n.isNotEmpty()) NativeEngine.nativeRename(n)
                close()
            }
            box.addView(ok, lp(-1, dp(42)))
        }, false, true)
    }

    /** Menu ⋯ layer: masker, grup, pilih banyak, ganti nama. */
    private fun showLayerMenu() {
        val st = lastState ?: return
        val rows = ArrayList<SheetAction>()
        if (st.type != 5) {
            if (st.type != 1 && st.type != 2) {
                val mm = NativeEngine.nativeMatteMode()
                rows.add(SheetAction(IconView.MASK, if (mm == 0) "Jadikan masker" else "Masker · " + (if (mm == 1) "aktif" else "balik"),
                    "Layer ini memotong layer tepat di bawahnya") { showMatteMenu() })
            }
            if (st.type == 1) rows.add(SheetAction(IconView.GROUP, "Bongkar grup", "Isi grup tetap ada") { NativeEngine.nativeUngroup() })
            else rows.add(SheetAction(IconView.GROUP, "Bungkus dalam grup", "Grup baru berisi layer ini") {
                NativeEngine.nativeMultiMode(true)
                NativeEngine.nativeMultiAction(0)
            })
        }
        rows.add(SheetAction(IconView.SELECT, "Pilih beberapa layer", "Tandai banyak layer, lalu kelompokkan / gandakan / hapus") { enterMultiMode() })
        rows.add(SheetAction(IconView.TEXT, "Ganti nama", st.name) { showRenameDialog() })
        showActions(this, "Layer", rows)
    }

    /** Daftar induk kecil (popup) dari ikon tautan di bilah atas. Urutan sama dengan timeline (atas ke bawah). */
    private fun showParentPopup(anchor: View) {
        val lines = NativeEngine.nativeParentData().split("\n").filter { it.isNotEmpty() }
        if (lines.isEmpty()) return
        val cur = lines[0].toIntOrNull() ?: 0
        val list = LinearLayout(this)
        list.orientation = LinearLayout.VERTICAL
        val sc = ScrollView(this)
        sc.addView(list)
        val pw = android.widget.PopupWindow(sc, dp(230), ViewGroup.LayoutParams.WRAP_CONTENT, true)
        pw.elevation = dpf(12f)
        pw.setBackgroundDrawable(rounded(Ui.SURFACE2, 14f))
        fun item(text: String, sub: String, on: Boolean, id: Int) {
            val row = LinearLayout(this)
            row.gravity = Gravity.CENTER_VERTICAL
            row.setPadding(dp(14), dp(10), dp(12), dp(10))
            if (on) row.setBackgroundColor(Ui.SURFACE3)
            val tv = label(text, 13.5f, if (on) Ui.ACCENT_SOFT else Ui.TEXT, on)
            tv.setSingleLine()
            tv.ellipsize = android.text.TextUtils.TruncateAt.END
            row.addView(tv, lp(0, -2, 1f))
            if (sub.isNotEmpty()) row.addView(label(sub, 10.5f, Ui.DIM), lp(-2, -2).apply { leftMargin = dp(8) })
            row.setOnClickListener { pw.dismiss(); NativeEngine.nativeSetParent(id) }
            list.addView(row, lp(-1, -2))
        }
        item("Tidak ada", "", cur == 0, 0)
        for (k in 1 until lines.size) {
            val p = lines[k].split("|", limit = 3)
            val id = p[0].toIntOrNull() ?: continue
            val tp = if (p.size > 2) p[2].toIntOrNull() ?: 0 else 0
            item(if (p.size > 1) p[1] else "Layer", if (tp == 0) "" else typeLabel(tp), id == cur, id)
        }
        val maxH = (resources.displayMetrics.heightPixels * 0.55f).toInt()
        list.measure(View.MeasureSpec.makeMeasureSpec(dp(230), View.MeasureSpec.EXACTLY), View.MeasureSpec.UNSPECIFIED)
        pw.height = minOf(list.measuredHeight, maxH)
        pw.showAsDropDown(anchor, -dp(230) + anchor.width, 0)
    }

    private fun enterMultiMode() {
        NativeEngine.nativeMultiMode(true)
        toast("Ketuk layer di timeline untuk menandai")
    }

    private fun toggleMultiMode() {
        if (NativeEngine.nativeMultiCount() >= 0) NativeEngine.nativeMultiMode(false) else enterMultiMode()
    }

    // ---------- Teks ----------
    private class TextBmp(val name: String, val w: Int, val h: Int)

    private fun unescapeText(s: String): String {
        val sb = StringBuilder()
        var i = 0
        while (i < s.length) {
            val c = s[i]
            if (c == '\\' && i + 1 < s.length) {
                val n = s[i + 1]
                sb.append(if (n == 'n') '\n' else n)
                i += 2
            } else {
                sb.append(c)
                i++
            }
        }
        return sb.toString()
    }

    /** Render teks putih menjadi PNG di folder media (2× resolusi agar tajam saat diperbesar). */
    private fun renderText(text: String, size: Float, style: Int): TextBmp? {
        try {
            var scale = 2f
            val fam = arrayOf("sans-serif", "serif", "monospace", "casual")[(style shr 2) and 3]
            val paint = TextPaint(android.graphics.Paint.ANTI_ALIAS_FLAG)
            paint.color = Color.WHITE
            paint.typeface = Typeface.create(fam, style and 3)
            var bw = 2
            var bh = 2
            var layout: StaticLayout? = null
            while (true) {
                paint.textSize = size * scale
                var wide = 1f
                for (line in text.split("\n")) wide = maxOf(wide, Layout.getDesiredWidth(line, paint))
                val width = Math.ceil(wide.toDouble()).toInt().coerceIn(1, 4000)
                val lay = StaticLayout.Builder.obtain(text, 0, text.length, paint, width)
                    .setAlignment(Layout.Alignment.ALIGN_CENTER).build()
                layout = lay
                val pad0 = Math.ceil((size * scale * 0.15f).toDouble()).toInt()
                bw = width + 2 * pad0
                bh = lay.height + 2 * pad0
                if ((bw <= 4096 && bh <= 4096) || scale <= 0.26f) break
                scale *= 0.5f
            }
            bw = bw.coerceIn(2, 4096)
            bh = bh.coerceIn(2, 4096)
            val pad = Math.ceil((size * scale * 0.15f).toDouble()).toInt()
            val key = text.hashCode().toLong() * 31L + size.toBits() * 17L + style * 7L + bw * 131L + bh
            val name = "txt_" + java.lang.Long.toHexString(key and 0xFFFFFFFFFFFFL) + ".png"
            val f = File(File(filesDir, "media"), name)
            if (!f.exists()) {
                f.parentFile?.mkdirs()
                val bmp = Bitmap.createBitmap(bw, bh, Bitmap.Config.ARGB_8888)
                val cv = Canvas(bmp)
                cv.translate(pad.toFloat(), pad.toFloat())
                layout?.draw(cv)
                java.io.FileOutputStream(f).use { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }
                bmp.recycle()
            }
            return TextBmp(name, Math.round(bw / scale), Math.round(bh / scale))
        } catch (e: Exception) {
            return null
        }
    }

    /** Dialog teks: ketik isi, pilih huruf, ukuran. editing = ubah layer Teks terpilih; selain itu tambah baru. */
    private fun showTextEditor(editing: Boolean) {
        var text = ""
        var size = 96f
        var style = 0
        if (editing) {
            val p = NativeEngine.nativeTextData().split("|", limit = 3)
            if (p.size < 3) { toast("Pilih layer teks dulu"); return }
            size = p[0].toFloatOrNull() ?: 96f
            style = p[1].toIntOrNull() ?: 0
            text = unescapeText(p[2])
        }
        showSheet(this, if (editing) "Edit teks" else "Teks baru", { box, close ->
            val e = EditText(this)
            e.setText(text)
            e.setSelection(e.text.length)
            e.setTextColor(Ui.TEXT)
            e.setHintTextColor(Ui.DIM)
            e.hint = "Ketik teks di sini…"
            e.textSize = 16f
            e.minLines = 2
            e.maxLines = 6
            e.gravity = Gravity.TOP or Gravity.START
            e.background = card(12f)
            e.setPadding(dp(12), dp(10), dp(12), dp(10))
            e.inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_MULTI_LINE or InputType.TYPE_TEXT_FLAG_CAP_SENTENCES
            box.addView(e, lp(-1, -2).apply { bottomMargin = dp(10) })

            val famNames = listOf("Sans", "Serif", "Mono", "Kasual")
            val famChips = ArrayList<TextView>()
            val boldChip = chip("Tebal", (style and 1) != 0) {}
            val italicChip = chip("Miring", (style and 2) != 0) {}
            fun paintChips() {
                styleChip(boldChip, (style and 1) != 0)
                styleChip(italicChip, (style and 2) != 0)
                famChips.forEachIndexed { k, c -> styleChip(c, ((style shr 2) and 3) == k) }
            }
            boldChip.setOnClickListener { style = style xor 1; paintChips() }
            italicChip.setOnClickListener { style = style xor 2; paintChips() }
            val styleRow = HorizontalScrollView(this)
            styleRow.isHorizontalScrollBarEnabled = false
            val sr = LinearLayout(this)
            sr.addView(boldChip, lp(-2, dp(34)).apply { rightMargin = dp(8) })
            sr.addView(italicChip, lp(-2, dp(34)).apply { rightMargin = dp(14) })
            famNames.forEachIndexed { k, n ->
                val c = chip(n, false) { style = (style and 3) or (k shl 2); paintChips() }
                famChips.add(c)
                sr.addView(c, lp(-2, dp(34)).apply { rightMargin = dp(8) })
            }
            styleRow.addView(sr)
            box.addView(styleRow, lp(-1, -2).apply { bottomMargin = dp(8) })
            paintChips()

            val sizeRow = SliderRow(this, "Ukuran huruf", 16f, 400f, 0, 96f, { size = it }, null)
            sizeRow.set(size)
            box.addView(sizeRow.view, lp(-1, -2).apply { bottomMargin = dp(10) })

            box.addView(button("Terapkan", 2) {
                val t = e.text.toString().trim()
                if (t.isEmpty()) {
                    toast("Teks masih kosong")
                } else {
                    val b = renderText(t, size, style)
                    if (b == null) {
                        toast("Gagal membuat teks")
                    } else {
                        if (editing) NativeEngine.nativeSetText(b.name, b.w, b.h, t, size, style)
                        else NativeEngine.nativeAddText(b.name, b.w, b.h, t, size, style)
                        close()
                    }
                }
            }, lp(-1, -2))
        }, keyboard = true)
    }

    private fun pickAudio() {
        val i = Intent(Intent.ACTION_OPEN_DOCUMENT)
        i.type = "audio/*"
        i.addCategory(Intent.CATEGORY_OPENABLE)
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        startActivityForResult(i, 13)
    }

    private fun displayName(uri: Uri): String {
        try {
            contentResolver.query(uri, arrayOf(android.provider.OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
                if (c.moveToFirst()) {
                    val n = c.getString(0)
                    if (!n.isNullOrBlank()) return n.substringBeforeLast('.')
                }
            }
        } catch (_: Exception) {
        }
        return "Suara"
    }

    /** Suara pilihan pengguna: dekode ke PCM di latar lalu masuk sebagai layer suara (tidak digambar, hanya terdengar). */
    private fun importAudio(uri: Uri, name: String) {
        val fpsNow = if (fps > 0) fps else 30
        val at = NativeEngine.nativeFrame()
        toast("Mengimpor suara…")
        Thread {
            val info = AudioProxy.extract(this, uri, fpsNow) { msg -> handler.post { toast(msg) } }
            if (info != null) handler.post { NativeEngine.nativeAddAudio(info.clip, info.frames, at, name) }
            else if (AudioProxy.noAudio) handler.post { toast("Berkas ini tidak punya suara") }
        }.start()
    }

    /** Suara dari video yang diimpor: layer suara muncul di latar tanpa mengubah layer yang sedang dipilih. */
    private fun importAudioAt(uri: Uri, name: String, at: Int) {
        val fpsNow = if (fps > 0) fps else 30
        Thread {
            val info = AudioProxy.extract(this, uri, fpsNow) { }
            if (info != null) handler.post {
                val before = Parsing.state(NativeEngine.nativeLayerState())?.id ?: 0
                NativeEngine.nativeAddAudio(info.clip, info.frames, at, name)
                NativeEngine.nativeSelectLayer(before)
            }
        }.start()
    }

    private fun pickVideo() {
        val i = Intent(Intent.ACTION_OPEN_DOCUMENT)   // izin baca bisa dipertahankan agar frame video dilanjutkan setelah aplikasi dibuka ulang
        i.type = "video/*"
        i.addCategory(Intent.CATEGORY_OPENABLE)
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
        startActivityForResult(i, 12)
    }

    /** Video langsung masuk setelah frame pertama siap; frame lainnya dibuat di latar (lihat VideoProxy). */
    private fun importVideo(uri: Uri) {
        try {
            contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        } catch (_: Exception) {
        }
        val fpsNow = if (fps > 0) fps else 30
        Thread {
            val info = VideoProxy.open(this, uri, fpsNow) { msg -> handler.post { toast(msg) } }
            if (info != null) handler.post {
                val at = NativeEngine.nativeFrame()
                NativeEngine.nativeAddVideo(info.clip, info.w, info.h, info.frames)
                // suara dari video menjadi layer suara terpisah (bisa dipotong/dibisukan sendiri); video tetap terpilih
                importAudioAt(uri, "Suara " + displayName(uri), at)
            }
        }.start()
    }

    /** Dipanggil di thread GL sebelum merender frame ekspor: pastikan semua frame video yang dibutuhkan sudah jadi tekstur. */
    private fun prepareExportFrame(frame: Int) {
        for (name in NativeEngine.nativeMissingVideoFrames(frame)) {
            VideoProxy.ensure(this, name)    // ekspor menunggu frame yang belum selesai dibuat di latar
            uploadImage(name)
        }
    }

    @Deprecated("Deprecated in Java")
    @Suppress("DEPRECATION")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == 13 && resultCode == RESULT_OK) {
            val a: Uri = data?.data ?: return
            importAudio(a, displayName(a))
            return
        }
        if (requestCode == 12 && resultCode == RESULT_OK) {
            val v: Uri = data?.data ?: return
            importVideo(v)
            return
        }
        if (requestCode != 11 || resultCode != RESULT_OK) return
        val uri: Uri = data?.data ?: return
        try {
            val dir = File(filesDir, "media")
            dir.mkdirs()
            val name = "img_" + System.currentTimeMillis() + ".bin"
            val f = File(dir, name)
            contentResolver.openInputStream(uri)?.use { ins -> f.outputStream().use { ins.copyTo(it) } }
                ?: throw IllegalStateException("tidak bisa membuka file")
            val o = BitmapFactory.Options()
            o.inJustDecodeBounds = true
            BitmapFactory.decodeFile(f.absolutePath, o)
            if (o.outWidth <= 0 || o.outHeight <= 0) {
                f.delete()
                Toast.makeText(this, "Bukan gambar yang valid", Toast.LENGTH_LONG).show()
                return
            }
            NativeEngine.nativeAddImage(name, o.outWidth, o.outHeight)
        } catch (e: Exception) {
            Toast.makeText(this, "Gagal impor gambar: $e", Toast.LENGTH_LONG).show()
        }
    }

    /** Dipanggil di thread GL: muat gambar yang diminta engine dan unggah sebagai tekstur. */
    fun serviceImageRequests() {
        for (n in 0 until 6) {
            val name = NativeEngine.nativeNextImageRequest()
            if (name.isEmpty()) return
            uploadImage(name)
        }
    }

    /** Thread GL: decode berkas media `name` dan serahkan teksturnya ke engine (tex 0 = gagal). */
    private fun uploadImage(name: String) {
        var tex = 0
        var w = 0
        var h = 0
        if (name.indexOf('/') > 0 && !VideoProxy.exists(this, name)) {
            // frame video belum selesai dibuat: minta didahulukan, engine mencoba lagi sebentar lagi
            VideoProxy.want(this, name)
            NativeEngine.nativeSetImageTexture(name, 0, 0, 0)
            return
        }
        try {
            val path = File(File(filesDir, "media"), name).absolutePath
            val b = BitmapFactory.Options()
            b.inJustDecodeBounds = true
            BitmapFactory.decodeFile(path, b)
            var sample = 1
            while (maxOf(b.outWidth, b.outHeight) / sample > 2048) sample *= 2
            val o = BitmapFactory.Options()
            o.inSampleSize = sample
            o.inPreferredConfig = android.graphics.Bitmap.Config.ARGB_8888
            val bmp = BitmapFactory.decodeFile(path, o)
            if (bmp != null) {
                val ids = IntArray(1)
                GLES30.glGenTextures(1, ids, 0)
                GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, ids[0])
                GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MIN_FILTER, GLES30.GL_LINEAR)
                GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MAG_FILTER, GLES30.GL_LINEAR)
                GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_WRAP_S, GLES30.GL_CLAMP_TO_EDGE)
                GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_WRAP_T, GLES30.GL_CLAMP_TO_EDGE)
                GLUtils.texImage2D(GLES30.GL_TEXTURE_2D, 0, bmp, 0)
                w = bmp.width
                h = bmp.height
                bmp.recycle()
                tex = ids[0]
            }
        } catch (e: Exception) {
            tex = 0
        }
        NativeEngine.nativeSetImageTexture(name, tex, w, h)
    }

    @Suppress("DEPRECATION")
    private fun applyInsets(v: View, ins: WindowInsets) {
        if (Build.VERSION.SDK_INT >= 30) {
            val i = ins.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout())
            v.setPadding(i.left, i.top, i.right, i.bottom)
        } else {
            v.setPadding(ins.systemWindowInsetLeft, ins.systemWindowInsetTop,
                ins.systemWindowInsetRight, ins.systemWindowInsetBottom)
        }
    }

    @Suppress("OVERRIDE_DEPRECATION", "DEPRECATION")
    override fun onBackPressed() {
        if (currentTab >= 0) goBack() else super.onBackPressed()
    }

    override fun onResume() {
        super.onResume()
        if (!::glView.isInitialized) return
        glView.onResume()
        handler.post(poll)
        // Contoh gambar efek dibuat di latar belakang setelah GL siap, supaya katalog langsung bergambar.
        handler.postDelayed({
            // Tiap kategori efek memakai gambar contoh yang berbeda (thumb_src_0..4.jpg di assets).
            for (idx in 0 until 5) {
                try {
                    assets.open("thumb_src_$idx.jpg").use { s ->
                        val bmp = android.graphics.BitmapFactory.decodeStream(s)
                        if (bmp != null) {
                            val w = bmp.width
                            val h = bmp.height
                            val px = IntArray(w * h)
                            bmp.getPixels(px, 0, w, 0, 0, w, h)
                            val raw = ByteArray(w * h * 4)
                            for (i in px.indices) {
                                raw[i * 4] = ((px[i] shr 16) and 0xFF).toByte()
                                raw[i * 4 + 1] = ((px[i] shr 8) and 0xFF).toByte()
                                raw[i * 4 + 2] = (px[i] and 0xFF).toByte()
                                raw[i * 4 + 3] = 0xFF.toByte()
                            }
                            NativeEngine.nativeSetThumbSource(idx, raw, w, h)
                        }
                    }
                } catch (e: Exception) { /* lewati gambar yang gagal dimuat */ }
            }
            Thumbs.request()
        }, 1200)
        Choreographer.getInstance().postFrameCallback(frameCb)
    }

    private val frameCb = object : Choreographer.FrameCallback {
        override fun doFrame(t: Long) {
            if (NativeEngine.nativePlaying()) {
                val fr = NativeEngine.nativeFrame()
                val dur = NativeEngine.nativeDuration()
                timeline.setPlayhead(fr, true)
                setTime(fr, dur)
            }
            Choreographer.getInstance().postFrameCallback(this)
        }
    }

    /** Gambar mini proyek untuk layar awal. Dirender engine di thread GL sebelum GL dijeda. */
    private fun saveThumbnail() {
        val id = projectId ?: return
        if (!::glView.isInitialized) return
        try {
            val size = NativeEngine.nativeProjectSize()
            val k = 360f / maxOf(size[0], size[1])
            val w = (size[0] * k).toInt().coerceAtLeast(2)
            val h = (size[1] * k).toInt().coerceAtLeast(2)
            val frame = NativeEngine.nativeFrame()
            val latch = java.util.concurrent.CountDownLatch(1)
            var raw: ByteArray? = null
            glView.queueEvent {
                try { raw = NativeEngine.nativeRenderRGBA(frame, w, h) } finally { latch.countDown() }
            }
            if (!latch.await(700, java.util.concurrent.TimeUnit.MILLISECONDS)) return
            val b = raw ?: return
            if (b.size != w * h * 4) return
            val px = IntArray(w * h)
            for (i in px.indices) {
                px[i] = Color.rgb(b[i * 4].toInt() and 0xFF, b[i * 4 + 1].toInt() and 0xFF, b[i * 4 + 2].toInt() and 0xFF)
            }
            ProjectStore.saveThumb(this, id, android.graphics.Bitmap.createBitmap(px, w, h, android.graphics.Bitmap.Config.ARGB_8888))
        } catch (_: Exception) {}
    }

    override fun onPause() {
        Choreographer.getInstance().removeFrameCallback(frameCb)
        handler.removeCallbacks(poll)
        saveThumbnail()
        if (::audioPlayer.isInitialized) audioPlayer.stop()
        if (::glView.isInitialized) glView.onPause()
        NativeEngine.nativeSaveNow()
        super.onPause()
    }

    override fun onDestroy() {
        VideoProxy.onError = null
        VideoProxy.stop()
        if (::audioPlayer.isInitialized) audioPlayer.stop()
        super.onDestroy()
    }

    private class EngineRenderer(private val act: MainActivity) : GLSurfaceView.Renderer {
        // Jam harus monoton sepanjang hidup proses (jangan di-reset saat surface dibuat ulang).
        private val startMs = SystemClock.elapsedRealtime()

        override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
            NativeEngine.nativeInit()
        }

        override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
            NativeEngine.nativeResize(width, height)
        }

        override fun onDrawFrame(gl: GL10?) {
            NativeEngine.nativeRender(SystemClock.elapsedRealtime() - startMs)
            act.serviceImageRequests()
        }
    }
}
