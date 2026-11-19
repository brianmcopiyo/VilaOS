package notes

import app.Cap
import app.VilaApp
import app.require
import storage.Storage
import ui.Action
import ui.Screen
import ui.SoftKey
import ui.TextInput
import ui.Ui
import ui.Widget

class NotesApp : VilaApp() {
    private val noteInput = TextInput(id = "note", placeholder = "Write a note")

    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.STORAGE_APP)
        Storage.get("note1", "note")
        noteInput.attach(screen)
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> {
                Storage.set("note1", Widget.text("note"))
                Action.run("save_note")
            }
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
