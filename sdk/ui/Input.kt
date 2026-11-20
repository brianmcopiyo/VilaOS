class Input(val id: String, val placeholder: String = "") {
    fun attach(screen: Screen) {}
    fun value(screen: Screen): String = "text:$id"
    fun text(screen: Screen): String = value(screen)
    fun clear(screen: Screen) {
        Ui.clearText(id)
    }
    fun setText(screen: Screen, text: String) {
        Ui.setText(id, text)
    }
    fun onTextChanged(handler: (String) -> Unit) {}
}
