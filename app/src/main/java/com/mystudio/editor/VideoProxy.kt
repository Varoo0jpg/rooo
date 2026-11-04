package com.mystudio.editor

import android.content.Context
import android.graphics.Bitmap
import android.media.MediaMetadataRetriever
import android.net.Uri
import android.os.Build
import java.io.File
import java.io.FileOutputStream
import java.util.ArrayDeque
import java.util.BitSet

/**
 * Video "proxy" yang dibuat SECARA MALAS (lazy): layer video langsung masuk ke proyek setelah frame pertama siap
 * (sepersekian detik), sisanya dibuat di latar belakang. Frame yang sedang dibutuhkan engine (playhead, pemutaran,
 * ekspor) didahulukan; sisanya diisi berurutan dari awal. Hasilnya satu JPEG per frame proyek di
 * filesDir/media/<klip>/NNNNNN.jpg. Berkas sumber dicatat di media/<klip>/src.txt supaya pembuatan dilanjutkan
 * setelah aplikasi dibuka ulang.
 */
object VideoProxy {
    const val MAX_SECONDS = 600
    private const val MAX_SIDE = 960
    private const val WORKERS = 3

    class Info(val clip: String, val w: Int, val h: Int, val frames: Int)

    private class Clip(val name: String, val uri: Uri, val fps: Int, val frames: Int, val w: Int, val h: Int) {
        val done = BitSet(frames)
        val inFlight = HashSet<Int>()
        val wanted = ArrayDeque<Int>()
        var cursor = 0
        var failed = false
        var doneCount = 0
        fun complete() = doneCount >= frames || failed
    }

    private val lock = Object()
    private val clips = HashMap<String, Clip>()
    private val workers = ArrayList<Thread>()
    @Volatile private var gen = 0
    @Volatile private var appCtx: Context? = null
    @Volatile var onError: ((String) -> Unit)? = null

    private fun dir(ctx: Context, clip: String) = File(File(ctx.filesDir, "media"), clip)
    private fun frameFile(ctx: Context, clip: String, i: Int) = File(dir(ctx, clip), String.format("%06d.jpg", i))

    // ---------- Impor ----------

    /** Baca metadata, buat frame pertama, daftarkan klip. Panggil dari thread latar. Cepat (puluhan–ratusan ms). */
    fun open(ctx0: Context, uri: Uri, fps: Int, error: (String) -> Unit): Info? {
        val ctx = ctx0.applicationContext
        appCtx = ctx
        val meta = MediaMetadataRetriever()
        val frames: Int
        val outW: Int
        val outH: Int
        try {
            meta.setDataSource(ctx, uri)
            val durMs = meta.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)?.toLongOrNull() ?: 0L
            var w = meta.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH)?.toIntOrNull() ?: 0
            var h = meta.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT)?.toIntOrNull() ?: 0
            val rot = meta.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_ROTATION)?.toIntOrNull() ?: 0
            if (rot == 90 || rot == 270) { val t = w; w = h; h = t }
            if (durMs <= 0L || w <= 0 || h <= 0) { error("Video tidak bisa dibaca"); return null }
            if (durMs > MAX_SECONDS * 1000L) { error("Video terlalu panjang (maks. 10 menit)"); return null }
            frames = Math.max(1, (durMs * fps / 1000L).toInt())
            val sc = Math.min(1f, MAX_SIDE.toFloat() / Math.max(w, h))
            outW = Math.max(2, (w * sc).toInt() / 2 * 2)
            outH = Math.max(2, (h * sc).toInt() / 2 * 2)
            val clip = "vid_" + java.lang.Long.toHexString(System.currentTimeMillis()) +
                java.lang.Integer.toHexString((Math.random() * 65535).toInt())
            val d = dir(ctx, clip)
            if (!d.mkdirs()) { error("Gagal membuat folder media"); return null }
            File(d, "src.txt").writeText("$uri\n$fps\n$frames\n$outW\n$outH\n")
            // frame pertama langsung (agar layer tampil seketika)
            val c = Clip(clip, uri, fps, frames, outW, outH)
            val first = decode(meta, c, 0, null)
            if (first == null) {
                d.deleteRecursively()
                error("Frame video tidak bisa didekode (format tidak didukung?)")
                return null
            }
            writeFrame(ctx, c, 0, first)
            first.recycle()
            synchronized(lock) {
                c.done.set(0); c.doneCount = 1; c.cursor = 1
                clips[clip] = c
                startWorkers()
                lock.notifyAll()
            }
            return Info(clip, outW, outH, frames)
        } catch (e: Exception) {
            error("Video tidak bisa dibuka: $e")
            return null
        } finally {
            try { meta.release() } catch (_: Exception) {}
        }
    }

    // ---------- Permintaan dari engine ----------

    private fun parse(name: String): Pair<String, Int>? {
        val slash = name.indexOf('/')
        if (slash <= 0) return null
        val idx = name.substring(slash + 1).substringBefore('.').toIntOrNull() ?: return null
        return name.substring(0, slash) to idx
    }

    /** Daftarkan klip dari src.txt bila belum terdaftar (mis. setelah aplikasi dibuka ulang). */
    private fun ensureRegistered(ctx: Context, clip: String): Clip? {
        synchronized(lock) {
            clips[clip]?.let { return it }
            try {
                val lines = File(dir(ctx, clip), "src.txt").readLines()
                if (lines.size < 5) return null
                val c = Clip(clip, Uri.parse(lines[0]), lines[1].toInt(), lines[2].toInt(), lines[3].toInt(), lines[4].toInt())
                val files = dir(ctx, clip).list() ?: emptyArray()
                for (f in files) {
                    if (!f.endsWith(".jpg")) continue
                    val i = f.substringBefore('.').toIntOrNull() ?: continue
                    if (i in 0 until c.frames && !c.done.get(i)) { c.done.set(i); c.doneCount++ }
                }
                clips[clip] = c
                if (!c.complete()) { appCtx = ctx.applicationContext; startWorkers(); lock.notifyAll() }
                return c
            } catch (_: Exception) {
                return null
            }
        }
    }

    fun exists(ctx: Context, name: String): Boolean {
        val (clip, idx) = parse(name) ?: return true
        return frameFile(ctx, clip, idx).exists()
    }

    /** Minta frame `name` didahulukan. Mengembalikan false bila berkasnya belum ada. */
    fun want(ctx: Context, name: String): Boolean {
        val (clip, idx) = parse(name) ?: return true
        if (frameFile(ctx, clip, idx).exists()) return true
        val c = ensureRegistered(ctx, clip) ?: return false
        synchronized(lock) {
            if (!c.done.get(idx) && !c.inFlight.contains(idx) && !c.wanted.contains(idx)) {
                c.wanted.addLast(idx)
                while (c.wanted.size > 12) c.wanted.pollFirst()   // hanya permintaan terbaru yang relevan
                lock.notifyAll()
            }
        }
        return false
    }

    /** Untuk ekspor: tunggu sampai frame ada (maks. timeoutMs). */
    fun ensure(ctx: Context, name: String, timeoutMs: Long = 30000): Boolean {
        val t0 = System.currentTimeMillis()
        while (!want(ctx, name)) {
            val (clip, _) = parse(name) ?: return true
            synchronized(lock) { if (clips[clip]?.failed == true) return false }
            if (System.currentTimeMillis() - t0 > timeoutMs) return false
            try { Thread.sleep(8) } catch (_: InterruptedException) { return false }
        }
        return true
    }

    /** Persen kemajuan pembuatan frame (0–99) untuk klip yang belum selesai, atau -1 bila tidak ada pekerjaan. */
    fun progress(): Int {
        synchronized(lock) {
            var total = 0L
            var done = 0L
            for (c in clips.values) if (!c.complete()) { total += c.frames; done += c.doneCount }
            return if (total == 0L) -1 else Math.min(99, (done * 100 / total).toInt())
        }
    }

    fun stop() {
        gen++
        synchronized(lock) {
            clips.clear()
            lock.notifyAll()
        }
        workers.clear()
    }

    // ---------- Pekerja latar ----------

    private fun startWorkers() {   // dipanggil dalam lock
        workers.removeAll { !it.isAlive }
        while (workers.size < WORKERS) {
            val g = gen
            val t = Thread { workerLoop(g) }
            t.isDaemon = true
            t.priority = Thread.NORM_PRIORITY - 1
            workers.add(t)
            t.start()
        }
    }

    private class Job(val clip: Clip, val idx: Int)

    private fun nextJob(): Job? {   // dipanggil dalam lock
        for (c in clips.values) {
            if (c.complete()) continue
            while (c.wanted.isNotEmpty()) {
                val i = c.wanted.pollLast()
                if (i != null && !c.done.get(i) && !c.inFlight.contains(i)) { c.inFlight.add(i); return Job(c, i) }
            }
        }
        for (c in clips.values) {
            if (c.complete()) continue
            while (c.cursor < c.frames && (c.done.get(c.cursor) || c.inFlight.contains(c.cursor))) c.cursor++
            if (c.cursor < c.frames) { val i = c.cursor++; c.inFlight.add(i); return Job(c, i) }
        }
        return null
    }

    private fun workerLoop(g: Int) {
        val retrievers = HashMap<String, MediaMetadataRetriever>()
        val lastBmp = HashMap<String, Bitmap>()
        try {
            while (g == gen) {
                var job: Job?
                synchronized(lock) {
                    job = nextJob()
                    if (job == null) {
                        // frame yang tertinggal (misalnya cursor sudah lewat): cek ulang sebelum tidur
                        for (c in clips.values) if (!c.complete()) {
                            val i = c.done.nextClearBit(0)
                            if (i < c.frames && !c.inFlight.contains(i)) { c.inFlight.add(i); job = Job(c, i); break }
                        }
                    }
                    if (job == null) {
                        try { lock.wait(1500) } catch (_: InterruptedException) { return }
                    }
                }
                val j = job ?: continue
                val ctx = appCtx ?: return
                val c = j.clip
                try {
                    var r = retrievers[c.name]
                    if (r == null) {
                        r = MediaMetadataRetriever()
                        r.setDataSource(ctx, c.uri)
                        retrievers[c.name] = r
                    }
                    var bmp = decode(r, c, j.idx, lastBmp[c.name])
                    if (bmp == null) bmp = lastBmp[c.name]          // frame gagal: ulangi frame sebelumnya
                    if (bmp == null) {
                        // tetangga yang sudah jadi sebagai cadangan
                        throw IllegalStateException("frame ${j.idx} tidak bisa didekode")
                    }
                    writeFrame(ctx, c, j.idx, bmp)
                    if (bmp !== lastBmp[c.name]) { lastBmp[c.name]?.recycle(); lastBmp[c.name] = bmp }
                    synchronized(lock) { c.inFlight.remove(j.idx); c.done.set(j.idx); c.doneCount++ }
                } catch (e: Exception) {
                    synchronized(lock) {
                        c.inFlight.remove(j.idx)
                        if (!c.failed) {
                            c.failed = true
                            onError?.invoke("Pembuatan frame video berhenti: sumber video tidak bisa dibaca")
                        }
                    }
                    try { retrievers.remove(c.name)?.release() } catch (_: Exception) {}
                }
                synchronized(lock) {
                    if (c.complete()) {
                        try { retrievers.remove(c.name)?.release() } catch (_: Exception) {}
                        lastBmp.remove(c.name)?.recycle()
                    }
                }
            }
        } finally {
            for (r in retrievers.values) try { r.release() } catch (_: Exception) {}
            for (b in lastBmp.values) b.recycle()
        }
    }

    // ---------- Dekode & tulis ----------

    private fun decode(r: MediaMetadataRetriever, c: Clip, idx: Int, @Suppress("UNUSED_PARAMETER") last: Bitmap?): Bitmap? {
        val us = idx.toLong() * 1_000_000L / c.fps
        var bmp: Bitmap? = null
        try {
            bmp = if (Build.VERSION.SDK_INT >= 27)
                r.getScaledFrameAtTime(us, MediaMetadataRetriever.OPTION_CLOSEST, c.w, c.h)
            else r.getFrameAtTime(us, MediaMetadataRetriever.OPTION_CLOSEST)
        } catch (_: Exception) {
        }
        if (bmp == null) return null
        if (bmp.width != c.w || bmp.height != c.h) {
            val s = Bitmap.createScaledBitmap(bmp, c.w, c.h, true)
            if (s !== bmp) bmp.recycle()
            bmp = s
        }
        return bmp
    }

    /** Tulis ke berkas sementara lalu ganti nama, supaya engine tidak pernah membaca JPEG yang setengah jadi. */
    private fun writeFrame(ctx: Context, c: Clip, idx: Int, bmp: Bitmap) {
        val f = frameFile(ctx, c.name, idx)
        val tmp = File(f.parentFile, f.name + ".tmp")
        FileOutputStream(tmp).use { bmp.compress(Bitmap.CompressFormat.JPEG, 82, it) }
        if (!tmp.renameTo(f)) { tmp.delete(); throw IllegalStateException("gagal menyimpan frame") }
    }
}
