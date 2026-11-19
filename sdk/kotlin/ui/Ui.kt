package ui

enum class SoftKey {
    LEFT,
    CENTER,
    RIGHT
}

class Screen

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

class DialKeypad(val id: String, val onKey: (String) -> Unit = {}) {
    fun attach(screen: Screen) {}
}

class TextInput(val id: String, val placeholder: String = "") {
    fun attach(screen: Screen) {}
    fun value(screen: Screen): String = "text:$id"
    fun clear(screen: Screen) {
        Ui.clearText(id)
    }
    fun setText(screen: Screen, text: String) {
        Ui.setText(id, text)
    }
}

class ActionButton(val id: String, val variant: String = "primary", val onClick: () -> Unit = {}) {
    fun attach(screen: Screen) {}
}

class ListRow(val id: String, val onSelect: (Int) -> Unit = {}) {
    fun attach(screen: Screen) {}
}

class SurfaceCard(val id: String = "") {
    fun attach(screen: Screen) {}
}

class StatusBadge(val id: String = "") {
    fun setText(text: String) {
        Ui.setText(id, text)
    }
    fun attach(screen: Screen) {}
}

class AppTile(val id: String = "") {
    fun attach(screen: Screen) {}
}

object SoftkeyBar {
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

object Action {
    fun run(name: String) {}
}

object Widget {
    fun text(id: String): String = "text:$id"
}
