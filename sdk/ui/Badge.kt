class Badge(
    val id: String = "",
    val icon: String = "",
    val iconSet: String = "lucide"
) {
    fun setText(text: String) {
        Ui.setText(id, text)
    }
    fun setText(screen: Screen, text: String) {
        Ui.setText(id, text)
    }
    fun attach(screen: Screen) {}
}
