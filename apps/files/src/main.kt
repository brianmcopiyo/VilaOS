package files

import app.Cap
import app.VilaApp
import app.require
import storage.Storage
import ui.Action
import ui.Screen
import ui.SoftKey
import ui.Ui

class FilesApp : VilaApp() {
    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.STORAGE_FILES)
        Storage.filesList(".", "file_preview")
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> Action.run("preview")
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
