class Header(val id: String = "") {
    fun setText(text: String) {
        Ui.setText(id, text)
    }
    fun attach(screen: Screen) {}
}
