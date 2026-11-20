package pkg

data class PackageInfo(
    val id: String = "",
    val name: String = "",
    val icon: String = "",
    val version: String = "",
    val capabilities: String = ""
)

object PackageManager {
    fun listInstalled(): List<PackageInfo> = emptyList()
    fun onChanged(callback: () -> Unit) {}
    fun refresh() {}
    fun renderGrid(gridId: String, filter: String = "") {}
    fun count(): Int = 0
    fun pollChanged(): Int = 0
    fun find(id: String): String = ""
}
