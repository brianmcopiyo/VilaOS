class Keypad(val id: String, val onKey: (String) -> Unit = {}) {
    fun attach(screen: Screen) {}
}
