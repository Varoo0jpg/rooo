package com.mystudio.editor

/** Data efek & keadaan layer yang dikirim engine C++ sebagai teks. */
class PDef(val flat: Int, val label: String, val min: Float, val max: Float, val def: Float, val kind: Int)

class FxDef(val type: String, val label: String, val category: String, val passes: Int) {
    val params = ArrayList<PDef>()
}

class FxState(val idx: Int, val type: String, val enabled: Boolean, val vals: FloatArray, val keyed: Set<Int>)

class LState(
    val id: Int, val name: String, val type: Int, val shape: Int, val corner: Float, val sides: Int,
    val inner: Float, val blend: Int, val inF: Int, val outF: Int, val z: Int,
    val t: FloatArray, val keyedT: Set<Int>, val fx: List<FxState>
)

object Parsing {
    fun catalog(s: String): LinkedHashMap<String, FxDef> {
        val out = LinkedHashMap<String, FxDef>()
        var cur: FxDef? = null
        for (line in s.split("\n")) {
            val p = line.split("|")
            if (p.size >= 5 && p[0] == "E") {
                val d = FxDef(p[1], p[2], p[3], p[4].toIntOrNull() ?: 1)
                cur = d
                out[p[1]] = d
            } else if (p.size >= 7 && p[0] == "P") {
                val d = cur ?: continue
                d.params.add(PDef(p[1].toIntOrNull() ?: 0, p[2], p[3].toFloatOrNull() ?: 0f,
                    p[4].toFloatOrNull() ?: 1f, p[5].toFloatOrNull() ?: 0f, p[6].toIntOrNull() ?: 0))
            }
        }
        return out
    }

    fun state(s: String): LState? {
        if (s.isEmpty()) return null
        var l: List<String>? = null
        var t = FloatArray(9)
        var keyedT: Set<Int> = emptySet()
        val fx = ArrayList<FxState>()
        for (line in s.split("\n")) {
            val p = line.split("|")
            if (p.isEmpty()) continue
            if (p[0] == "L" && p.size >= 13) {
                l = p
            } else if (p[0] == "T" && p.size >= 7) {
                t = FloatArray(9) { p.getOrNull(it + 1)?.toFloatOrNull() ?: 0f }
            } else if (p[0] == "K" && p.size >= 2) {
                keyedT = if (p[1].isEmpty()) emptySet() else p[1].split(",").mapNotNull { it.toIntOrNull() }.toSet()
            } else if (p[0] == "F" && p.size >= 6) {
                val vals = if (p[4].isEmpty()) FloatArray(0)
                else p[4].split(",").map { it.toFloatOrNull() ?: 0f }.toFloatArray()
                val keyed = if (p[5].isEmpty()) emptySet() else p[5].split(",").mapNotNull { it.toIntOrNull() }.toSet()
                fx.add(FxState(p[1].toIntOrNull() ?: 0, p[2], p[3] == "1", vals, keyed))
            }
        }
        val q = l ?: return null
        return try {
            LState(q[1].toInt(), q[2], q[3].toInt(), q[4].toInt(), q[5].toFloat(), q[6].toInt(),
                q[7].toFloat(), q[8].toInt(), q[10].toInt(), q[11].toInt(), q[12].toInt(), t, keyedT, fx)
        } catch (e: NumberFormatException) {
            null
        }
    }
}
