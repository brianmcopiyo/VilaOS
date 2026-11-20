package files

import app.Cap
import app.VilaApp
import app.require
import storage.Storage
import Screen
import SoftKey
import Ui
class FilesApp : VilaApp() {
    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.STORAGE_FILES)
        Storage.filesList(".", "file_preview")
    }

    override fun onAction(action: String) {
        when (action) {
            "preview" -> {
                val name = Ui.getText("file_preview")
                Ui.setText("file_preview", "Preview: " + name)
            }
        }
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> {
                val name = Ui.getText("file_preview")
                Ui.setText("file_preview", "Preview: " + name)
            }
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
