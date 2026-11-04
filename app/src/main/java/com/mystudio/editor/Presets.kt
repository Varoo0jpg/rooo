package com.mystudio.editor

import android.content.Context
import java.io.File

/**
 * Preset tersimpan permanen: gaya sebuah layer (warna, blend, transform/animasi, efek) yang bisa ditempel ke layer lain,
 * seperti "salin" tetapi tidak hilang. Satu berkas per preset di filesDir/presets: baris 1 "P1<TAB>mask<TAB>nama", sisanya
 * teks layer dari engine (nativePresetExport).
 */
object PresetStore {
    const val M_COLOR = 1
    const val M_BLEND = 2
    const val M_TRANSFORM = 4
    const val M_FX = 8

    class Item(val id: String, val name: String, val mask: Int, val text: String)

    private fun dir(ctx: Context): File {
        val d = File(ctx.applicationContext.filesDir, "presets")
        if (!d.exists()) d.mkdirs()
        return d
    }

    private fun clean(n: String) = n.replace('\t', ' ').replace('\n', ' ').trim().take(40)

    fun list(ctx: Context): List<Item> {
        val out = ArrayList<Item>()
        val files = dir(ctx).listFiles { f -> f.name.endsWith(".txt") } ?: return out
        files.sortBy { it.name }
        for (f in files) {
            try {
                val t = f.readText()
                val nl = t.indexOf('\n')
                if (nl < 0) continue
                val head = t.substring(0, nl).split("\t")
                if (head.size < 3 || head[0] != "P1") continue
                out.add(Item(f.name.removeSuffix(".txt"), head[2], head[1].toIntOrNull() ?: 0, t.substring(nl + 1)))
            } catch (_: Exception) {
            }
        }
        return out
    }

    fun save(ctx: Context, name: String, mask: Int, text: String): Boolean {
        if (text.isEmpty()) return false
        return try {
            val id = java.lang.Long.toHexString(System.currentTimeMillis()) + java.lang.Integer.toHexString((Math.random() * 4095).toInt())
            val nm = clean(name).ifEmpty { "Preset" }
            val tmp = File(dir(ctx), "$id.tmp")
            tmp.writeText("P1\t$mask\t$nm\n$text")
            tmp.renameTo(File(dir(ctx), "$id.txt"))
        } catch (_: Exception) {
            false
        }
    }

    fun delete(ctx: Context, id: String) {
        try { File(dir(ctx), "$id.txt").delete() } catch (_: Exception) {}
    }

    fun rename(ctx: Context, item: Item, name: String) {
        try {
            val nm = clean(name).ifEmpty { item.name }
            val f = File(dir(ctx), item.id + ".txt")
            val tmp = File(dir(ctx), item.id + ".tmp")
            tmp.writeText("P1\t${item.mask}\t$nm\n${item.text}")
            tmp.renameTo(f)
        } catch (_: Exception) {
        }
    }
}
