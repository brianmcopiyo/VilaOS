class Row(
    val id: String,
    val icon: String = "",
    val iconSet: String = "lucide",
    val onSelect: (Int) -> Unit = {}
) {
    fun setSubtitle(text: String) {
        Ui.setText(id, text)
    }
    fun setSubtitle(screen: Screen, text: String) {
        Ui.setText(id, text)
    }
    fun attach(screen: Screen) {}
}
