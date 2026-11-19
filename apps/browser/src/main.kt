package browser

import app.Cap
import app.VilaApp
import app.require
import ui.Action
import ui.Screen
import ui.SoftKey
import ui.Ui

class BrowserApp : VilaApp() {
    override fun onStart(screen: Screen) {
        require(Cap.UI)
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> Action.run("ok")
            SoftKey.CENTER -> Action.run("ok")
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
