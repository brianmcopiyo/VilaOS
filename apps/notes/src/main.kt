package notes

import app.Cap
import app.VilaApp
import app.require
import storage.Storage
import ui.Input
import ui.Screen
import ui.SoftKey
import ui.Ui

class NotesApp : VilaApp() {
    private val noteInput = Input(id = "note", placeholder = "Write a note")

    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.STORAGE_APP)
        Storage.get("note1", "note")
        noteInput.attach(screen)
    }

    override fun onAction(action: String) {
        when (action) {
            "save_note" -> {
                val body = Ui.getText("note")
                Storage.set("note1", body)
                Ui.setText("note_status", "Note saved")
            }
        }
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> {
                val body = Ui.getText("note")
                Storage.set("note1", body)
                Ui.setText("note_status", "Note saved")
            }
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
