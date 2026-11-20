object Footer {
    fun left(label: String, action: String) {
        Ui.softkey(SoftKey.LEFT, label, action)
    }
    fun center(label: String, action: String) {
        Ui.softkey(SoftKey.CENTER, label, action)
    }
    fun right(label: String, action: String) {
        Ui.softkey(SoftKey.RIGHT, label, action)
    }
}
