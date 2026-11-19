package browser

import app.Cap
import app.VilaApp
import app.require
import ui.Screen
import ui.SoftKey
import ui.Ui

class BrowserApp : VilaApp() {
    private fun add(a: Int, b: Int): Int {
        return a + b
    }

    override fun onStart(screen: Screen) {
        require(Cap.UI)
        val n = add(2, 3)
        Ui.setText("hint", "Ready " + n.toString())
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> Ui.setText("hint", "OK")
            SoftKey.CENTER -> Ui.setText("hint", "OK")
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
