package com.mystudio.editor

import android.content.Context
import android.graphics.Bitmap
import java.io.File

/**
 * Daftar proyek di penyimpanan aplikasi: files/projects/<id>.studio (+ .meta nama & ukuran, .jpg gambar mini).
 * File .studio sendiri ditulis oleh engine (CRC + cadangan bergilir); di sini hanya mengelola berkasnya.
 */
object ProjectStore {
    class Info(val id: String, val name: String, val w: Int, val h: Int, val fps: Int, val sec: Int,
               val modified: Long, val file: File, val thumb: File)

    private val suffixes = listOf("", ".bak1", ".bak2", ".bak3", ".tmp", ".corrupt")

    fun dir(ctx: Context): File = File(ctx.filesDir, "projects").apply { mkdirs() }
    fun fileOf(ctx: Context, id: String) = File(dir(ctx), "$id.studio")
    private fun metaOf(ctx: Context, id: String) = File(dir(ctx), "$id.meta")
    fun thumbOf(ctx: Context, id: String) = File(dir(ctx), "$id.jpg")

    private fun writeMeta(ctx: Context, id: String, name: String, w: Int, h: Int, fps: Int, sec: Int) {
        try {
            metaOf(ctx, id).writeText("${name.replace('\n', ' ')}\n$w\n$h\n$fps\n$sec\n")
        } catch (_: Exception) {}
    }

    /** Proyek tunggal dari versi lama (files/project.studio) dipindah menjadi proyek pertama. */
    fun migrateLegacy(ctx: Context) {
        val legacy = File(ctx.filesDir, "project.studio")
        if (!legacy.exists()) return
        val id = "p0"
        try {
            for (s in suffixes) {
                val src = File(ctx.filesDir, "project.studio$s")
                if (src.exists()) src.renameTo(File(dir(ctx), "$id.studio$s"))
            }
            writeMeta(ctx, id, "Proyek pertama", 1080, 1920, 30, 10)
        } catch (_: Exception) {}
    }

    fun list(ctx: Context): List<Info> {
        val out = ArrayList<Info>()
        val files = dir(ctx).listFiles() ?: return out
        for (f in files) {
            if (!f.name.endsWith(".studio")) continue
            val id = f.name.removeSuffix(".studio")
            var name = "Proyek"
            var w = 1080; var h = 1920; var fps = 30; var sec = 10
            try {
                val l = metaOf(ctx, id).readLines()
                if (l.isNotEmpty() && l[0].isNotBlank()) name = l[0]
                w = l.getOrNull(1)?.toIntOrNull() ?: w
                h = l.getOrNull(2)?.toIntOrNull() ?: h
                fps = l.getOrNull(3)?.toIntOrNull() ?: fps
                sec = l.getOrNull(4)?.toIntOrNull() ?: sec
            } catch (_: Exception) {}
            out.add(Info(id, name, w, h, fps, sec, f.lastModified(), f, thumbOf(ctx, id)))
        }
        out.sortByDescending { it.modified }
        return out
    }

    fun create(ctx: Context, name: String, w: Int, h: Int, fps: Int, sec: Int): Info? {
        val id = "p" + System.currentTimeMillis()
        val f = fileOf(ctx, id)
        if (!NativeEngine.nativeCreateProject(f.absolutePath, w, h, fps, sec)) return null
        writeMeta(ctx, id, name, w, h, fps, sec)
        return list(ctx).firstOrNull { it.id == id }
    }

    fun rename(ctx: Context, p: Info, newName: String) {
        writeMeta(ctx, p.id, newName.ifBlank { p.name }, p.w, p.h, p.fps, p.sec)
    }

    fun duplicate(ctx: Context, p: Info) {
        val id = "p" + System.currentTimeMillis()
        try {
            p.file.copyTo(fileOf(ctx, id), overwrite = true)
            if (p.thumb.exists()) p.thumb.copyTo(thumbOf(ctx, id), overwrite = true)
            writeMeta(ctx, id, p.name + " salinan", p.w, p.h, p.fps, p.sec)
        } catch (_: Exception) {}
    }

    fun delete(ctx: Context, p: Info) {
        for (s in suffixes) File(dir(ctx), "${p.id}.studio$s").delete()
        metaOf(ctx, p.id).delete()
        thumbOf(ctx, p.id).delete()
    }

    fun saveThumb(ctx: Context, id: String, bmp: Bitmap) {
        try {
            thumbOf(ctx, id).outputStream().use { bmp.compress(Bitmap.CompressFormat.JPEG, 82, it) }
        } catch (_: Exception) {}
    }

    fun nextDefaultName(ctx: Context): String {
        val names = list(ctx).map { it.name }.toSet()
        var i = list(ctx).size + 1
        while ("Proyek $i" in names) i++
        return "Proyek $i"
    }
}
