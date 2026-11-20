package example

import app.Cap
import app.VilaApp
import app.require
import Button
import Screen
import SoftKey
import Ui

class ExampleApp : VilaApp() {
    private val okBtn = Button(id = "ok_btn", variant = "primary") {
        Ui.pop()
    }

    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.STORAGE_APP)
        Ui.setText("hint", "Ready")
        okBtn.attach(screen)
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
