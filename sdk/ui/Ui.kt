object Ui {
    fun setText(id: String, text: String) {}
    fun appendText(id: String, text: String) {}
    fun clearText(id: String) {}
    fun getText(id: String): String = ""
    fun softkey(side: SoftKey, label: String, action: String = "") {}
    fun dialog(title: String, body: String) {}
    fun push(layout: String) {}
    fun replace(layout: String) {}
    fun pop() {}
    fun home() {}
}
